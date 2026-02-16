cmake_minimum_required(VERSION 3.22)
project(openauto-prodigy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt 6 — only Core and Quick needed for Phase 1 skeleton
find_package(Qt6 REQUIRED COMPONENTS Core Quick)
qt_standard_project_setup()

add_subdirectory(src)
