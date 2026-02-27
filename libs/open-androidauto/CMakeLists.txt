cmake_minimum_required(VERSION 3.22)
project(open-androidauto VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)

# Proto generation: custom protoc invocation to preserve oaa/ directory structure
set(PROTO_IMPORT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/proto/open-android-auto/proto)
set(PROTO_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB_RECURSE PROTO_FILES
    ${PROTO_IMPORT_ROOT}/oaa/*.proto)

set(PROTO_SRCS)
set(PROTO_HDRS)

foreach(PROTO_FILE ${PROTO_FILES})
    # Get path relative to import root (e.g., oaa/control/PingRequestMessage.proto)
    file(RELATIVE_PATH PROTO_REL ${PROTO_IMPORT_ROOT} ${PROTO_FILE})
    # Derive output paths
    string(REPLACE ".proto" ".pb.cc" PROTO_SRC "${PROTO_OUTPUT_DIR}/${PROTO_REL}")
    string(REPLACE ".proto" ".pb.h" PROTO_HDR "${PROTO_OUTPUT_DIR}/${PROTO_REL}")

    list(APPEND PROTO_SRCS ${PROTO_SRC})
    list(APPEND PROTO_HDRS ${PROTO_HDR})

    get_filename_component(PROTO_OUT_DIR ${PROTO_SRC} DIRECTORY)

    add_custom_command(
        OUTPUT ${PROTO_SRC} ${PROTO_HDR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_OUT_DIR}
        COMMAND protobuf::protoc
            --proto_path=${PROTO_IMPORT_ROOT}
            --cpp_out=${PROTO_OUTPUT_DIR}
            ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating protobuf: ${PROTO_REL}"
    )
endforeach()

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
    include/oaa/Messenger/ProtocolLogger.hpp
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
    src/Messenger/ProtocolLogger.cpp
    src/Session/AASession.cpp
    include/oaa/HU/Handlers/VideoChannelHandler.hpp
    include/oaa/HU/Handlers/AudioChannelHandler.hpp
    include/oaa/HU/Handlers/AVInputChannelHandler.hpp
    include/oaa/HU/Handlers/InputChannelHandler.hpp
    include/oaa/HU/Handlers/SensorChannelHandler.hpp
    include/oaa/HU/Handlers/BluetoothChannelHandler.hpp
    include/oaa/HU/Handlers/WiFiChannelHandler.hpp
    include/oaa/HU/Handlers/StubChannelHandler.hpp
    include/oaa/HU/Handlers/NavigationChannelHandler.hpp
    include/oaa/HU/Handlers/MediaStatusChannelHandler.hpp
    include/oaa/HU/Handlers/PhoneStatusChannelHandler.hpp
    src/HU/Handlers/NavigationChannelHandler.cpp
    src/HU/Handlers/MediaStatusChannelHandler.cpp
    src/HU/Handlers/PhoneStatusChannelHandler.cpp
    src/HU/Handlers/VideoChannelHandler.cpp
    src/HU/Handlers/AudioChannelHandler.cpp
    src/HU/Handlers/AVInputChannelHandler.cpp
    src/HU/Handlers/InputChannelHandler.cpp
    src/HU/Handlers/SensorChannelHandler.cpp
    src/HU/Handlers/BluetoothChannelHandler.cpp
    src/HU/Handlers/WiFiChannelHandler.cpp
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
