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
    

# Запуск всех тестов
test:
    @cmake --build "{{DEBUG_DIR}}" --target test_operations test_file_utils test_file_cache test_worker test_stress test_mainwindow
    @cd "{{DEBUG_DIR}}" && ctest --output-on-failure
    @echo "✅ All tests passed"

# Запуск только быстрых unit-тестов
test-fast:
    @cmake --build "{{DEBUG_DIR}}" --target test_operations test_file_utils test_file_cache
    @cd "{{DEBUG_DIR}}" && ctest --output-on-failure -R "test_operations|test_file_utils|test_file_cache"
    @echo "✅ Fast tests passed"

# Запуск интеграционных тестов Worker
test-worker:
    @cmake --build "{{DEBUG_DIR}}" --target test_worker
    @cd "{{DEBUG_DIR}}" && ctest --output-on-failure -R test_worker
    @echo "✅ Worker tests passed"

# Запуск стресс-тестов (боевые условия)
test-stress:
    @cmake --build "{{DEBUG_DIR}}" --target test_stress
    @cd "{{DEBUG_DIR}}" && ctest --output-on-failure -R test_stress -V
    @echo "✅ Stress tests passed (no crashes!)"

# Запуск GUI-тестов
test-gui:
    @cmake --build "{{DEBUG_DIR}}" --target test_mainwindow
    @cd "{{DEBUG_DIR}}" && ctest --output-on-failure -R test_mainwindow -V
    @echo "✅ GUI tests passed"