cmake_minimum_required(VERSION 3.22)
project(open-androidauto VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(OpenSSL REQUIRED)

add_library(open-androidauto STATIC
    include/oaa/Transport/ITransport.hpp
    include/oaa/Transport/TCPTransport.hpp
    include/oaa/Transport/ReplayTransport.hpp
    include/oaa/Messenger/FrameType.hpp
    include/oaa/Messenger/FrameHeader.hpp
    include/oaa/Messenger/FrameSerializer.hpp
    include/oaa/Messenger/FrameAssembler.hpp
    include/oaa/Messenger/FrameParser.hpp
    include/oaa/Messenger/Cryptor.hpp
    include/oaa/Messenger/EncryptionPolicy.hpp
    include/oaa/Messenger/Messenger.hpp
    src/Transport/ITransport.cpp
    src/Transport/TCPTransport.cpp
    src/Transport/ReplayTransport.cpp
    src/Messenger/FrameHeader.cpp
    src/Messenger/FrameSerializer.cpp
    src/Messenger/FrameAssembler.cpp
    src/Messenger/FrameParser.cpp
    src/Messenger/Cryptor.cpp
    src/Messenger/EncryptionPolicy.cpp
    src/Messenger/Messenger.cpp
)

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
