cmake_minimum_required(VERSION 3.22)
project(openauto-prodigy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt 6 — Core, Quick, and QuickControls2 for UI shell
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2)
qt_standard_project_setup()

add_subdirectory(src)
add_subdirectory(tests)
