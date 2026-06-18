cmake_minimum_required(VERSION 3.21)
project(FileXorTool VERSION 1.0.0 LANGUAGES CXX)

if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent Test)
qt_standard_project_setup()

# Исходники основного приложения
set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/worker.cpp
    src/worker.h
    src/file_processor.cpp
    src/file_processor.h
    src/file_result.h
    src/file_cache.cpp
    src/file_cache.h
    src/file_utils.cpp
    src/file_utils.h
    src/utils/hex_parser.cpp
    src/utils/hex_parser.h
    src/operations/ibinary_operation.h
    src/operations/binary_operations.cpp
    src/operations/binary_operations.h
    src/operations/operation_factory.cpp
    src/operations/operation_factory.h
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent)

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
# ТЕСТЫ (раздельные исполняемые файлы)
# ==========================================
enable_testing()

set(APP_SOURCES_FOR_TESTS
    ${CMAKE_SOURCE_DIR}/src/mainwindow.cpp
    ${CMAKE_SOURCE_DIR}/src/worker.cpp
    ${CMAKE_SOURCE_DIR}/src/file_cache.cpp
    ${CMAKE_SOURCE_DIR}/src/file_processor.cpp
    ${CMAKE_SOURCE_DIR}/src/file_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/hex_parser.cpp

    ${CMAKE_SOURCE_DIR}/src/operations/binary_operations.cpp
    ${CMAKE_SOURCE_DIR}/src/operations/operation_factory.cpp
    
    ${CMAKE_SOURCE_DIR}/src/tests/test_helpers.cpp
    ${CMAKE_SOURCE_DIR}/src/tests/test_gui_helpers.cpp
)

macro(add_qt_test TEST_NAME TEST_SOURCE)
    qt_add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
        Qt6::Concurrent
    )
    target_include_directories(${TEST_NAME} PRIVATE 
        ${CMAKE_SOURCE_DIR}/src
    )
    target_sources(${TEST_NAME} PRIVATE ${APP_SOURCES_FOR_TESTS})
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endmacro()

add_qt_test(test_file_utils ${CMAKE_SOURCE_DIR}/src/tests/test_file_utils.cpp)
add_qt_test(test_file_cache ${CMAKE_SOURCE_DIR}/src/tests/test_file_cache.cpp)
add_qt_test(test_worker     ${CMAKE_SOURCE_DIR}/src/tests/test_worker.cpp)
add_qt_test(test_stress     ${CMAKE_SOURCE_DIR}/src/tests/test_stress.cpp)
add_qt_test(test_mainwindow ${CMAKE_SOURCE_DIR}/src/tests/test_mainwindow.cpp)
add_qt_test(test_operations ${CMAKE_SOURCE_DIR}/src/tests/test_operations.cpp)

# ==========================================
# Упаковка
# ==========================================
include(CPack)
set(CPACK_PACKAGE_NAME "FileXorTool")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-win64")