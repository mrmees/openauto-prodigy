# Phase 6: Proto Files

**Context:** Porting the 108 proto2 files from aasdk to open-androidauto. Phases 1-5 complete — library has full transport, framing, crypto, and messenger layers.

**Source:** `libs/aasdk/aasdk_proto/*.proto` (108 files, proto2 syntax)
**Destination:** `libs/open-androidauto/proto/`

## Proto Strategy

These are proto2 files (required for AA wire compatibility — proto3 drops `required` field validation that Google's AA app enforces). They represent years of reverse engineering. We copy them as-is except for updating the package name from `aasdk.proto.*` to `oaa.proto` for clean namespace separation.

Generated .pb.h/.pb.cc files go in the build directory via CMake's `protobuf_generate_cpp`.

---

- [x] **Port all 108 proto2 files and add protobuf generation to CMake.** Copy all .proto files from `libs/aasdk/aasdk_proto/` to `libs/open-androidauto/proto/` (exclude CMakeLists.txt if present). In each .proto file, update the `package` declaration: change any `package aasdk.proto...;` to `package oaa.proto;`. Also update any `import` statements that reference other proto files to use the correct relative paths (the aasdk protos use bare filenames like `import "StatusEnum.proto"` which should still work since they'll all be in the same directory). Update `libs/open-androidauto/CMakeLists.txt`: add `find_package(Protobuf REQUIRED)`, glob all .proto files with `file(GLOB PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto)`, call `protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})`, add `${PROTO_SRCS}` to library sources, add `${CMAKE_CURRENT_BINARY_DIR}` to PUBLIC include dirs (for generated headers), add `protobuf::libprotobuf` to PUBLIC link libraries. Build: `cd libs/open-androidauto/build && cmake .. && make -j$(nproc)`. Verify all 108 proto files compile and generate headers. Run existing tests to verify nothing broke. Commit: `feat(oaa): port 108 proto2 files from aasdk with protobuf generation`
