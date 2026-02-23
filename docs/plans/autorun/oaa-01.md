# Phase 1: Project Scaffolding

**Context:** We're building `open-androidauto` — a Qt-native C++17 replacement for `libs/aasdk/`. This is Phase 1: creating the directory structure, CMake build, and version constants.

**Design doc:** `docs/plans/2026-02-23-open-androidauto-design.md`
**Implementation plan:** `docs/plans/2026-02-23-open-androidauto-implementation.md`

---

- [x] **Create directory structure, Version.hpp, CMakeLists.txt, and version test.** *(Completed: directory tree created, all files written per spec, test_version passes with 3 test functions covering all 8 constants)* Create the full directory tree under `libs/open-androidauto/` with `include/oaa/{Transport,Messenger,Channel,Session}`, `src/{Transport,Messenger,Channel,Session}`, `proto/`, and `tests/`. Write `include/oaa/Version.hpp` with protocol constants (PROTOCOL_MAJOR=1, PROTOCOL_MINOR=7, FRAME_MAX_PAYLOAD=0x4000, FRAME_HEADER_SIZE=2, FRAME_SIZE_SHORT=2, FRAME_SIZE_EXTENDED=6, TLS_OVERHEAD=29, BIO_BUFFER_SIZE=20480) all as `constexpr` in namespace `oaa`. Write `CMakeLists.txt` that builds a STATIC library with Qt6::Core, Qt6::Network, OpenSSL::SSL, OpenSSL::Crypto, and includes `include/` as PUBLIC. Write `tests/CMakeLists.txt` with a helper function `oaa_add_test(name source)` that creates test executables linked against `open-androidauto` and `Qt6::Test`. Write `tests/test_version.cpp` using QtTest that verifies all constants. Build and run: `cd libs/open-androidauto && mkdir -p build && cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`. Commit: `feat(oaa): scaffold open-androidauto library with version constants`
