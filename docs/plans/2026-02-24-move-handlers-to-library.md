# Plan: Move Channel Handlers into open-androidauto Library

## Context

All 7 AA channel handlers (`Video`, `Audio`, `AVInput`, `Input`, `Sensor`, `Bluetooth`, `WiFi`) currently live in the app at `src/core/aa/handlers/` under the `oap::aa` namespace. They're pure protocol logic — parse protobuf, build protobuf responses, manage protocol state — with zero app-level dependencies. They belong in the `open-androidauto` library so any AA head unit can reuse them.

**ServiceDiscoveryBuilder** stays in the app — it depends on `oap::YamlConfig` (app-level config). Moving it would require abstracting the config interface, which is a separate task.

## Namespace

Handlers move from `oap::aa` → `oaa::hu`. The library uses `oaa` as its root namespace. `hu` (head unit) is the new sub-namespace for HU-side protocol implementations, matching the existing library pattern (`oaa::ChannelId`, `oaa::IChannelHandler`, etc.).

## Task 1: Move handlers, update namespace, update CMake (atomic)

Everything in one commit so the build is never broken.

**Move files (7 pairs = 14 files):**

| From (app) | To (library) |
|------------|-------------|
| `src/core/aa/handlers/VideoChannelHandler.{hpp,cpp}` | `libs/open-androidauto/{include/oaa,src}/HU/Handlers/VideoChannelHandler.{hpp,cpp}` |
| `src/core/aa/handlers/AudioChannelHandler.{hpp,cpp}` | same pattern |
| `src/core/aa/handlers/AVInputChannelHandler.{hpp,cpp}` | same pattern |
| `src/core/aa/handlers/InputChannelHandler.{hpp,cpp}` | same pattern |
| `src/core/aa/handlers/SensorChannelHandler.{hpp,cpp}` | same pattern |
| `src/core/aa/handlers/BluetoothChannelHandler.{hpp,cpp}` | same pattern |
| `src/core/aa/handlers/WiFiChannelHandler.{hpp,cpp}` | same pattern |

**Namespace changes in each moved file:**
- `namespace oap { namespace aa {` → `namespace oaa { namespace hu {`
- Update include paths: `"VideoChannelHandler.hpp"` → `"oaa/HU/Handlers/VideoChannelHandler.hpp"`

**CMake changes:**
- `libs/open-androidauto/CMakeLists.txt` — add all 14 new source/header files to `open-androidauto` target
- `src/CMakeLists.txt` — remove 7 `.cpp` files from app target
- `tests/CMakeLists.txt` — remove handler `.cpp` from individual test targets (they get it from the library link), update include paths

**App consumer updates:**
- `src/core/aa/AndroidAutoOrchestrator.hpp` — `#include <oaa/HU/Handlers/...>`, use `oaa::hu::` prefix
- `src/core/aa/AndroidAutoOrchestrator.cpp` — update handler type references
- Any other files in `src/` that reference handlers

**Test updates:**
- All `tests/test_*_channel_handler.cpp` files — update `#include` paths and namespace references
- `tests/test_oaa_integration.cpp` — update includes, remove handler .cpp from its sources
- `tests/test_aa_orchestrator.cpp` — update if it references handlers directly

**Build + test:**
```bash
cd build && cmake .. && cmake --build . -j$(nproc)
ctest --output-on-failure
```

**Commit:** `refactor: move AA channel handlers into open-androidauto library`

## Task 2: Clean up empty handlers directory

- Delete `src/core/aa/handlers/` (now empty)
- Update any remaining stale references

**Commit:** `chore: remove empty handlers directory`

## Task 3: Update documentation

- Update `libs/open-androidauto/README.md` with new HU module description
- Update `CLAUDE.md` repository layout section
- Brief note in docs about the `oaa::hu` namespace for handler consumers

**Commit:** `docs: document HU handler module in open-androidauto`

## Verification

1. **Local build:** `cmake --build build -j$(nproc)` — zero errors
2. **All tests pass:** `ctest --test-dir build --output-on-failure` — zero regressions
3. **Binary runs:** `./build/src/openauto-prodigy --help || true` — no linkage errors
4. **No stale references:** `grep -r "oap::aa::.*ChannelHandler" src/ tests/` — should return nothing
5. **Deploy to Pi, build, connect phone** — AA session works identically to before
