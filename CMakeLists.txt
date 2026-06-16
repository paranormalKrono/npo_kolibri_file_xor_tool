cmake_minimum_required(VERSION 3.21)
project(FileXorTool VERSION 1.0.0 LANGUAGES CXX)

if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test)
qt_standard_project_setup()

# Исходники основного приложения
set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/worker.cpp
    src/worker.h
    src/utils/hex_parser.cpp
    src/utils/hex_parser.h
    src/utils/file_utils.cpp
    src/utils/file_utils.h
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

if(MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++)
endif()

# Установка .exe
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

# Автоматический поиск зависимостей
if(WIN32 AND MINGW)
    find_program(_MINGW_DLL libwinpthread-1.dll
        PATHS "$ENV{MINGW_PREFIX}/bin" "C:/msys64/mingw64/bin" "C:/msys2/mingw64/bin"
        NO_DEFAULT_PATH
    )
    get_filename_component(MINGW_BIN_DIR "${_MINGW_DLL}" DIRECTORY)

    install(CODE "
        message(STATUS '🔍 Автоматический поиск зависимостей...')
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES \"\$<TARGET_FILE:${PROJECT_NAME}>\"
            RESOLVED_DEPENDENCIES_VAR _resolved
            UNRESOLVED_DEPENDENCIES_VAR _unresolved
            DIRECTORIES \"${MINGW_BIN_DIR}\"
            POST_EXCLUDE_REGEXES
                \".*[Ss]ystem32/.*\\\\.dll\"
                \".*[Ss]ys[Ww]OW64/.*\\\\.dll\"
                \"api-ms-win-.*\\\\.dll\"
                \"ext-ms-win-.*\\\\.dll\"
        )
        foreach(_dep \${_resolved})
            file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}/bin\" TYPE SHARED_LIBRARY FILES \"\${_dep}\")
            message(STATUS \"   📦 Copied: \${_dep}\")
        endforeach()
    ")
endif()

# Деплой Qt
if(WIN32)
    qt_generate_deploy_app_script(
        TARGET ${PROJECT_NAME}
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()

# ==========================================
# ТЕСТЫ
# ==========================================
enable_testing()

# Явно указываем пути от корня проекта
set(TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/src/tests/test_main.cpp
)

qt_add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME}_tests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)

# Добавляем исходники приложения, которые мы хотим протестировать
target_sources(${PROJECT_NAME}_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/utils/hex_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/file_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/worker.cpp
)

# Даем тестам доступ к заголовочным файлам приложения
target_include_directories(${PROJECT_NAME}_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/utils
)

# Регистрируем тест в CTest
add_test(NAME FileXorToolTests COMMAND ${PROJECT_NAME}_tests)

# ==========================================
# Упаковка
# ==========================================
include(CPack)
set(CPACK_PACKAGE_NAME "FileXorTool")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-win64")