set shell := ["sh", "-c"]

DEBUG_DIR := "build/debug"
RELEASE_DIR := "build/release"
DIST_DIR := "dist"

# === РАЗРАБОТКА (Debug) ===

clean:
    @if [ -f "{{DEBUG_DIR}}/CMakeCache.txt" ]; then \
        cmake --build "{{DEBUG_DIR}}" --target clean; \
    fi
    @echo " Debug-артефакты удалены"

clean-debug:
    @rm -rf "{{DEBUG_DIR}}"
    @echo "🗑️ Папка {{DEBUG_DIR}} удалена"

configure-debug:
    @cmake -B "{{DEBUG_DIR}}" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
    @echo "⚙️ Debug сконфигурирован"

build: configure-debug
    @cmake --build "{{DEBUG_DIR}}" --parallel
    @echo "🔨 Debug собран"

run: build
    @./"{{DEBUG_DIR}}"/FileXorTool.exe

# === РЕЛИЗ И ПАКЕТ ===

clean-release:
    @rm -rf "{{RELEASE_DIR}}" "{{DIST_DIR}}"
    @echo "🗑️ Папки {{RELEASE_DIR}} и {{DIST_DIR}} удалены"

configure-release:
    @cmake -B "{{RELEASE_DIR}}" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
    @echo "⚙️ Release сконфигурирован"

build-release: configure-release
    @cmake --build "{{RELEASE_DIR}}" --parallel
    @echo "🔨 Release собран"

# Установка в dist/
install: build-release
    @cmake --install "{{RELEASE_DIR}}" --prefix "$PWD/{{DIST_DIR}}"
    @echo "✅ Установлено в {{DIST_DIR}}/"

# Создание ZIP и NSIS через CPack (остается в build/release/_CPack_Packages/)
package: build-release
    @cmake --build "{{RELEASE_DIR}}" --target package
    @echo "✅ Дистрибутив создан в {{RELEASE_DIR}}/_CPack_Packages/"

run-release: install
    @./"{{DIST_DIR}}/bin/FileXorTool.exe"

# === ОБЩАЯ ОЧИСТКА ===

clean-all:
    @rm -rf build dist
    @echo "🗑️ Всё удалено"

# Установка всех зависимостей MSYS2
# ВНИМАНИЕ: Запускай MSYS2 MinGW64 от имени АДМИНИСТРАТОРА!
setup-msys:
    @echo "🔄 Обновление базовой системы MSYS2..."
    @pacman -Syu --noconfirm
    @echo "📦 Установка пакетов для C++/Qt разработки..."
    @pacman -S --noconfirm \
        mingw-w64-x86_64-toolchain \
        mingw-w64-x86_64-cmake \
        mingw-w64-x86_64-qt6-base \
        mingw-w64-x86_64-qt6-tools \
        mingw-w64-x86_64-nsis \
        mingw-w64-x86_64-just
    @echo "✅ Все пакеты успешно установлены!"
    
# Запуск тестов
test:
    @cmake --build "{{DEBUG_DIR}}" --target FileXorTool_tests
    @cd "{{DEBUG_DIR}}" && ctest --output-on-failure
    @echo "✅ Тесты пройдены"

# Запуск тестов с подробным выводом
test-verbose:
    @cmake --build "{{DEBUG_DIR}}" --target FileXorTool_tests
    @cd "{{DEBUG_DIR}}" && ctest --output-on-failure -V
    @echo "✅ Тесты пройдены"