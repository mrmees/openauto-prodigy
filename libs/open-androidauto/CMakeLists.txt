cmake_minimum_required(VERSION 3.22)
project(open-androidauto VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(OpenSSL REQUIRED)

add_library(open-androidauto STATIC)

target_include_directories(open-androidauto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(open-androidauto PUBLIC
    Qt6::Core
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Tests
option(OAA_BUILD_TESTS "Build open-androidauto tests" ON)
if(OAA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
