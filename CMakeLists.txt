cmake_minimum_required(VERSION 3.22)
project(openauto-prodigy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt 6 — Core, Quick, QuickControls2 for UI shell, Multimedia for video output
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Multimedia Network DBus)
find_package(Qt6 OPTIONAL_COMPONENTS Bluetooth)
qt_standard_project_setup()

# Suppress Qt QML static plugin import warnings on shared-library Qt builds
# (e.g. Debian/Trixie on RPi). Plugins load at runtime; static import targets
# don't exist in distro packages and produce harmless but noisy CMake warnings.
option(OAP_SKIP_QML_PLUGIN_IMPORT
    "Disable static QML plugin imports (recommended for distro-packaged Qt)" ON)

# Android Auto dependencies
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
include(FindPkgConfig)
pkg_check_modules(LIBAV REQUIRED libavcodec libavutil)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
find_package(yaml-cpp REQUIRED)

# open-androidauto — Qt-native AA protocol library
add_subdirectory(libs/open-androidauto)

# Capture git hash at configure time for version identification
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE OAP_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT OAP_GIT_HASH)
    set(OAP_GIT_HASH "unknown")
endif()

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
