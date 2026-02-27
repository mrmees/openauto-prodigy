# open-androidauto Qt-First Organization Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Reorganize AA HU helper code into `libs/open-androidauto` (single Qt target) with no protocol behavior changes.

**Architecture:** Move protocol-adjacent HU handlers and service discovery builder from app core into library `oaa/HU/*`, keep platform-specific orchestration/hardware code in app, and preserve compatibility through temporary wrapper headers. Use TDD-style verification by running focused unit tests after each move and full AA test suite at the end.

**Tech Stack:** C++17, Qt6, CMake, protobuf, open-androidauto static library, Qt Test

---

### Task 1: Create New Library HU Folder Structure

**Files:**
- Create: `libs/open-androidauto/include/oaa/HU/Handlers/.gitkeep`
- Create: `libs/open-androidauto/include/oaa/HU/Discovery/.gitkeep`
- Create: `libs/open-androidauto/src/HU/Handlers/.gitkeep`
- Create: `libs/open-androidauto/src/HU/Discovery/.gitkeep`

**Step 1: Create directories**

```bash
mkdir -p libs/open-androidauto/include/oaa/HU/Handlers
mkdir -p libs/open-androidauto/include/oaa/HU/Discovery
mkdir -p libs/open-androidauto/src/HU/Handlers
mkdir -p libs/open-androidauto/src/HU/Discovery
```

**Step 2: Add placeholders for git**

```bash
touch libs/open-androidauto/include/oaa/HU/Handlers/.gitkeep
touch libs/open-androidauto/include/oaa/HU/Discovery/.gitkeep
touch libs/open-androidauto/src/HU/Handlers/.gitkeep
touch libs/open-androidauto/src/HU/Discovery/.gitkeep
```

**Step 3: Commit**

```bash
git add libs/open-androidauto/include/oaa/HU libs/open-androidauto/src/HU
git commit -m "chore: add HU module directories to open-androidauto"
```

### Task 2: Move Handler Headers/Implementations Into Library

**Files:**
- Move: `src/core/aa/handlers/AudioChannelHandler.hpp` -> `libs/open-androidauto/include/oaa/HU/Handlers/AudioChannelHandler.hpp`
- Move: `src/core/aa/handlers/AudioChannelHandler.cpp` -> `libs/open-androidauto/src/HU/Handlers/AudioChannelHandler.cpp`
- Move: `src/core/aa/handlers/VideoChannelHandler.hpp` -> `libs/open-androidauto/include/oaa/HU/Handlers/VideoChannelHandler.hpp`
- Move: `src/core/aa/handlers/VideoChannelHandler.cpp` -> `libs/open-androidauto/src/HU/Handlers/VideoChannelHandler.cpp`
- Move: `src/core/aa/handlers/AVInputChannelHandler.hpp` -> `libs/open-androidauto/include/oaa/HU/Handlers/AVInputChannelHandler.hpp`
- Move: `src/core/aa/handlers/AVInputChannelHandler.cpp` -> `libs/open-androidauto/src/HU/Handlers/AVInputChannelHandler.cpp`
- Move: `src/core/aa/handlers/InputChannelHandler.hpp` -> `libs/open-androidauto/include/oaa/HU/Handlers/InputChannelHandler.hpp`
- Move: `src/core/aa/handlers/InputChannelHandler.cpp` -> `libs/open-androidauto/src/HU/Handlers/InputChannelHandler.cpp`
- Move: `src/core/aa/handlers/SensorChannelHandler.hpp` -> `libs/open-androidauto/include/oaa/HU/Handlers/SensorChannelHandler.hpp`
- Move: `src/core/aa/handlers/SensorChannelHandler.cpp` -> `libs/open-androidauto/src/HU/Handlers/SensorChannelHandler.cpp`
- Move: `src/core/aa/handlers/BluetoothChannelHandler.hpp` -> `libs/open-androidauto/include/oaa/HU/Handlers/BluetoothChannelHandler.hpp`
- Move: `src/core/aa/handlers/BluetoothChannelHandler.cpp` -> `libs/open-androidauto/src/HU/Handlers/BluetoothChannelHandler.cpp`
- Move: `src/core/aa/handlers/WiFiChannelHandler.hpp` -> `libs/open-androidauto/include/oaa/HU/Handlers/WiFiChannelHandler.hpp`
- Move: `src/core/aa/handlers/WiFiChannelHandler.cpp` -> `libs/open-androidauto/src/HU/Handlers/WiFiChannelHandler.cpp`

**Step 1: Move files without behavior edits**

```bash
git mv src/core/aa/handlers/AudioChannelHandler.hpp libs/open-androidauto/include/oaa/HU/Handlers/AudioChannelHandler.hpp
git mv src/core/aa/handlers/AudioChannelHandler.cpp libs/open-androidauto/src/HU/Handlers/AudioChannelHandler.cpp
# Repeat for remaining 6 handler pairs
```

**Step 2: Update include guards/includes only as needed for new path**

```cpp
// Example include update in moved .cpp
#include <oaa/HU/Handlers/AudioChannelHandler.hpp>
```

**Step 3: Run focused handler tests**

Run:
```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure -R "test_(audio_channel_handler|video_channel_handler|avinput_channel_handler|input_channel_handler|sensor_channel_handler|bluetooth_channel_handler|wifi_channel_handler)"
```
Expected: PASS, no behavior changes.

**Step 4: Commit**

```bash
git add libs/open-androidauto/include/oaa/HU/Handlers libs/open-androidauto/src/HU/Handlers
git commit -m "refactor: move AA channel handlers into open-androidauto HU module"
```

### Task 3: Move ServiceDiscoveryBuilder Into Library Discovery Module

**Files:**
- Move: `src/core/aa/ServiceDiscoveryBuilder.hpp` -> `libs/open-androidauto/include/oaa/HU/Discovery/ServiceDiscoveryBuilder.hpp`
- Move: `src/core/aa/ServiceDiscoveryBuilder.cpp` -> `libs/open-androidauto/src/HU/Discovery/ServiceDiscoveryBuilder.cpp`

**Step 1: Move files**

```bash
git mv src/core/aa/ServiceDiscoveryBuilder.hpp libs/open-androidauto/include/oaa/HU/Discovery/ServiceDiscoveryBuilder.hpp
git mv src/core/aa/ServiceDiscoveryBuilder.cpp libs/open-androidauto/src/HU/Discovery/ServiceDiscoveryBuilder.cpp
```

**Step 2: Keep current behavior and constructor shape unchanged**

```cpp
// Keep YamlConfig-dependent constructor and build() behavior as-is in this phase.
```

**Step 3: Run discovery + integration tests**

Run:
```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R "test_(service_discovery_builder|oaa_integration|aa_orchestrator)"
```
Expected: PASS.

**Step 4: Commit**

```bash
git add libs/open-androidauto/include/oaa/HU/Discovery libs/open-androidauto/src/HU/Discovery
git commit -m "refactor: move service discovery builder into open-androidauto"
```

### Task 4: Add Transitional Compatibility Includes and Update App Includes

**Files:**
- Create: `src/core/aa/handlers/AudioChannelHandler.hpp`
- Create: `src/core/aa/handlers/VideoChannelHandler.hpp`
- Create: `src/core/aa/handlers/AVInputChannelHandler.hpp`
- Create: `src/core/aa/handlers/InputChannelHandler.hpp`
- Create: `src/core/aa/handlers/SensorChannelHandler.hpp`
- Create: `src/core/aa/handlers/BluetoothChannelHandler.hpp`
- Create: `src/core/aa/handlers/WiFiChannelHandler.hpp`
- Create: `src/core/aa/ServiceDiscoveryBuilder.hpp`
- Modify: `src/core/aa/AndroidAutoOrchestrator.hpp`
- Modify: `tests/*.cpp` includes that reference old paths

**Step 1: Add wrapper headers**

```cpp
// src/core/aa/handlers/VideoChannelHandler.hpp
#pragma once
#include <oaa/HU/Handlers/VideoChannelHandler.hpp>
```

**Step 2: Update direct includes in app/tests to library paths**

```cpp
#include <oaa/HU/Handlers/VideoChannelHandler.hpp>
#include <oaa/HU/Discovery/ServiceDiscoveryBuilder.hpp>
```

**Step 3: Build and run all AA-related tests**

Run:
```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R "test_(aa_orchestrator|oaa_integration|service_discovery_builder|.*channel_handler)"
```
Expected: PASS.

**Step 4: Commit**

```bash
git add src/core/aa tests
git commit -m "refactor: add compatibility headers and switch app/tests to oaa HU includes"
```

### Task 5: Wire Moved Sources Into Library CMake and Remove App Source Ownership

**Files:**
- Modify: `libs/open-androidauto/CMakeLists.txt`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Add HU headers/sources to `open-androidauto` target**

```cmake
# libs/open-androidauto/CMakeLists.txt
add_library(open-androidauto STATIC
  # existing...
  include/oaa/HU/Handlers/VideoChannelHandler.hpp
  src/HU/Handlers/VideoChannelHandler.cpp
  include/oaa/HU/Discovery/ServiceDiscoveryBuilder.hpp
  src/HU/Discovery/ServiceDiscoveryBuilder.cpp
)
```

**Step 2: Remove moved .cpp files from app target/test local compilation lists**

```cmake
# src/CMakeLists.txt and tests/CMakeLists.txt
# remove direct source references for moved files, rely on open-androidauto link
```

**Step 3: Full build + full tests**

Run:
```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```
Expected: zero regressions.

**Step 4: Commit**

```bash
git add libs/open-androidauto/CMakeLists.txt src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "build: make open-androidauto own HU handlers and discovery modules"
```

### Task 6: Documentation and Hobbyist Entry-Point Cleanup

**Files:**
- Modify: `libs/open-androidauto/README.md`
- Modify: `README.md`
- Create: `docs/open-androidauto-hu-quickstart.md`

**Step 1: Document new HU API locations**

```markdown
- `oaa/HU/Handlers/*` for default channel handlers
- `oaa/HU/Discovery/ServiceDiscoveryBuilder` for service descriptors
```

**Step 2: Add minimal “custom app bring-up” example skeleton**

```cpp
// pseudo-snippet in docs
oaa::TCPTransport transport;
oaa::AASession session(&transport, cfg);
oaa::hu::handlers::VideoChannelHandler video;
session.registerChannel(oaa::ChannelId::Video, &video);
```

**Step 3: Verify docs references**

Run:
```bash
rg -n "src/core/aa/handlers|src/core/aa/ServiceDiscoveryBuilder" README.md docs libs/open-androidauto -S
```
Expected: no stale references to old primary locations.

**Step 4: Commit**

```bash
git add libs/open-androidauto/README.md README.md docs/open-androidauto-hu-quickstart.md
git commit -m "docs: add hu module layout and quickstart for hobbyist integrations"
```

### Final Verification Gate

**Step 1: Run all tests once more**

Run:
```bash
ctest --test-dir build --output-on-failure
```
Expected: PASS.

**Step 2: Smoke-run app binary**

Run:
```bash
./build/src/openauto-prodigy --help || true
```
Expected: binary launches (or shows usage) without linkage errors.

**Step 3: Record completion summary**
- List moved files.
- List API paths for hobbyists.
- Confirm no behavior changes introduced.
