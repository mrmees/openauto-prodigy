cmake_minimum_required(VERSION 3.22)
project(open-androidauto VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)

file(GLOB PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(open-androidauto STATIC
    ${PROTO_SRCS}
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
    include/oaa/Channel/IChannelHandler.hpp
    include/oaa/Channel/IAVChannelHandler.hpp
    include/oaa/Channel/ControlChannel.hpp
    include/oaa/Session/SessionState.hpp
    include/oaa/Session/SessionConfig.hpp
    include/oaa/Session/AASession.hpp
    src/Channel/IChannelHandler.cpp
    src/Channel/IAVChannelHandler.cpp
    src/Channel/ControlChannel.cpp
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
    src/Session/AASession.cpp
)

target_include_directories(open-androidauto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(open-androidauto PUBLIC
    Qt6::Core
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
    protobuf::libprotobuf
)

# Tests
option(OAA_BUILD_TESTS "Build open-androidauto tests" ON)
if(OAA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
