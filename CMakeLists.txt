cmake_minimum_required(VERSION 3.22)
# NOTE: the numeric VERSION below is CMake-internal only. The displayed /
# reported app version is git-derived — see the OAP_VERSION block below.
project(openauto-prodigy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt 6.8+ — Required for MultiEffect (QtQuick.Effects) shadow/depth system.
# Dev VM: cmake -DCMAKE_PREFIX_PATH=~/Qt/6.8.2/gcc_64 ..
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Multimedia Network WebSockets DBus)
find_package(Qt6 OPTIONAL_COMPONENTS Bluetooth WebEngineQuick)
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
pkg_check_modules(SYSTEMD QUIET IMPORTED_TARGET libsystemd)
find_package(yaml-cpp REQUIRED)

# prodigy-oaa-protocol — Qt-native AA protocol library
add_subdirectory(libs/prodigy-oaa-protocol)

# External API v1 protobuf contract (prodigy-private, NOT the AA submodule)
add_subdirectory(proto)

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

# App version identity: ALPHA-YY-MM-DD-NN milestone tags (ANNOTATED — no
# --tags below, so lightweight tags are ignored), minted by
# scripts/tag-alpha.sh (see AGENTS.md § Versioning). Captured at CONFIGURE
# time — after tagging, reconfigure + rebuild or the binary keeps the old
# string. No ALPHA tag yet (or git absent) -> ALPHA-untagged-<shorthash>.
execute_process(
    COMMAND git describe --match "ALPHA-*" --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE OAP_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
# Validate before embedding in a compile definition — a malformed or
# hand-crafted tag must not inject garbage into C++ source. (CMake regex
# has no {n} quantifier; digits are spelled out.)
if(NOT OAP_VERSION MATCHES "^ALPHA-[0-9][0-9]-[0-9][0-9]-[0-9][0-9]-[0-9][0-9]+(-[0-9]+-g[0-9a-f]+)?(-dirty)?$")
    if(OAP_VERSION)
        message(WARNING "Ignoring malformed ALPHA tag describe output: '${OAP_VERSION}'")
    endif()
    set(OAP_VERSION "ALPHA-untagged-${OAP_GIT_HASH}")
endif()
message(STATUS "App version: ${OAP_VERSION}")

add_subdirectory(src)
add_subdirectory(tools/eme-probe)

option(BUILD_TESTING "Build test executables" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
