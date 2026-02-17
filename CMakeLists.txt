cmake_minimum_required(VERSION 3.22)
project(openauto-prodigy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt 6 — Core, Quick, and QuickControls2 for UI shell
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2)
qt_standard_project_setup()

# Android Auto dependencies
find_package(Boost REQUIRED COMPONENTS system log log_setup)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
include(FindPkgConfig)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# aasdk — Android Auto protocol library
# Save our flags before aasdk overwrites them
set(_saved_cxx_flags "${CMAKE_CXX_FLAGS}")
add_subdirectory(libs/aasdk)
# Restore our flags (aasdk sets -Wall -pedantic globally)
set(CMAKE_CXX_FLAGS "${_saved_cxx_flags}")

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
