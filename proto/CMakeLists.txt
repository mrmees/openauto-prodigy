# External API v1 protobuf library (package prodigy.api.v1)
# Contract files live in proto/api/ and are ADDITIVE-ONLY after freeze.
find_package(Protobuf REQUIRED)

set(API_PROTO_IMPORT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(API_PROTO_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB API_PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/api/*.proto)

set(API_PROTO_SRCS "")
set(API_PROTO_HDRS "")
foreach(PROTO_FILE ${API_PROTO_FILES})
    file(RELATIVE_PATH PROTO_REL ${API_PROTO_IMPORT_ROOT} ${PROTO_FILE})
    string(REPLACE ".proto" ".pb.cc" PROTO_SRC "${API_PROTO_OUTPUT_DIR}/${PROTO_REL}")
    string(REPLACE ".proto" ".pb.h"  PROTO_HDR "${API_PROTO_OUTPUT_DIR}/${PROTO_REL}")
    get_filename_component(PROTO_OUT_DIR ${PROTO_SRC} DIRECTORY)
    add_custom_command(
        OUTPUT ${PROTO_SRC} ${PROTO_HDR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_OUT_DIR}
        COMMAND protobuf::protoc
            --proto_path=${API_PROTO_IMPORT_ROOT}
            --cpp_out=${API_PROTO_OUTPUT_DIR}
            ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating API protobuf: ${PROTO_REL}")
    list(APPEND API_PROTO_SRCS ${PROTO_SRC})
    list(APPEND API_PROTO_HDRS ${PROTO_HDR})
endforeach()

add_library(prodigy-api-proto STATIC ${API_PROTO_SRCS})
target_include_directories(prodigy-api-proto PUBLIC ${API_PROTO_OUTPUT_DIR})
target_link_libraries(prodigy-api-proto PUBLIC protobuf::libprotobuf)
# Generated protobuf code triggers -Wall noise; keep it quiet like the oaa lib.
set_target_properties(prodigy-api-proto PROPERTIES CXX_STANDARD 17)
