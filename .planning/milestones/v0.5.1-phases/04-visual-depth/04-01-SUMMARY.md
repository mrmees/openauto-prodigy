---
phase: 04-visual-depth
plan: 01
subsystem: infra
tags: [qt, qt6.8, aqtinstall, build-system, cmake]

# Dependency graph
requires: []
provides:
  - Qt 6.8.2 dev VM environment with QtQuick.Effects available
  - Clean codebase with no QT_VERSION guards
  - Qt 6.8 minimum version enforced in CMake
affects: [04-02, 04-03, 04-04]

# Tech tracking
tech-stack:
  added: [aqtinstall, Qt 6.8.2 (dev VM)]
  patterns: [single Qt version target (no version guards)]

key-files:
  created: []
  modified:
    - CMakeLists.txt
    - src/core/aa/DmaBufVideoBuffer.cpp
    - src/core/aa/DmaBufVideoBuffer.hpp
    - src/core/aa/VideoDecoder.cpp
    - src/core/aa/VideoFramePool.hpp
    - tests/test_video_frame_pool.cpp

key-decisions:
  - "aqtinstall linux_gcc_64 arch name (not gcc_64) for Qt 6.8.2 download"
  - "Removed acquire() fallback entirely -- all frame pool usage is now acquireRecycled()"
  - "libbluetooth-dev installed on dev VM (Qt 6.8.2 enables Bluetooth module)"

patterns-established:
  - "Single Qt version: no QT_VERSION preprocessor guards anywhere in src/"
  - "Dev VM CMAKE_PREFIX_PATH=~/Qt/6.8.2/gcc_64 for all builds"

requirements-completed: []

# Metrics
duration: 9min
completed: 2026-03-10
---

# Phase 04 Plan 01: Qt 6.8 Upgrade Summary

**Dev VM upgraded to Qt 6.8.2 via aqtinstall, all QT_VERSION guards removed, 65 tests pass**

## Performance

- **Duration:** 9 min
- **Started:** 2026-03-10T02:47:38Z
- **Completed:** 2026-03-10T02:56:08Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- Qt 6.8.2 installed on dev VM via aqtinstall with QtQuick.Effects module available
- All QT_VERSION preprocessor guards removed from source (DmaBufVideoBuffer, VideoDecoder, VideoFramePool)
- CMake enforces Qt 6.8 minimum version
- All 65 tests pass on Qt 6.8.2

## Task Commits

Each task was committed atomically:

1. **Task 1: Install Qt 6.8.2 on dev VM via aqtinstall** - `741037b` (chore)
2. **Task 2: Remove all QT_VERSION guards from C++ source** - `e0b427a` (feat)

## Files Created/Modified
- `CMakeLists.txt` - Qt 6.8 minimum version enforcement + CMAKE_PREFIX_PATH comment
- `src/core/aa/DmaBufVideoBuffer.cpp` - Removed #if QT_VERSION guards wrapping entire file
- `src/core/aa/DmaBufVideoBuffer.hpp` - Removed guards, updated comment about Qt 6.8 requirement
- `src/core/aa/VideoDecoder.cpp` - Removed 4 guard blocks, deleted Qt < 6.8 CPU DRM_PRIME transfer fallback
- `src/core/aa/VideoFramePool.hpp` - Removed guards, deleted acquire() fallback, kept only RecycledVideoBuffer path
- `tests/test_video_frame_pool.cpp` - Updated tests to use acquireRecycled() exclusively, removed QT_VERSION guards

## Decisions Made
- Used `aqtinstall` with arch name `linux_gcc_64` (not `gcc_64`) for Qt 6.8.2 download
- Installed `qtmultimedia`, `qtconnectivity`, `qtshadertools` extra modules (not in base Qt install)
- Installed `libbluetooth-dev` on dev VM since Qt 6.8.2 includes Qt6::Bluetooth module
- Removed `acquire()` method entirely from VideoFramePool -- all usage migrated to `acquireRecycled()`
- Updated test assertions to match recycled pool behavior (sequential acquire+release reuses buffers)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] aqtinstall arch name mismatch**
- **Found during:** Task 1
- **Issue:** `gcc_64` arch name rejected by aqtinstall; correct name is `linux_gcc_64`
- **Fix:** Used `aqt list-qt` to discover correct arch, installed with `linux_gcc_64`
- **Files modified:** None (install command only)
- **Verification:** Qt installed successfully at ~/Qt/6.8.2/gcc_64/

**2. [Rule 3 - Blocking] Missing Qt6Multimedia module**
- **Found during:** Task 1
- **Issue:** Base aqtinstall does not include Qt6Multimedia, Qt6Bluetooth, or Qt6ShaderTools
- **Fix:** Installed `qtmultimedia qtconnectivity qtshadertools` modules separately
- **Files modified:** None (install command only)
- **Verification:** CMake configure succeeds finding all required Qt6 components

**3. [Rule 3 - Blocking] Missing libbluetooth-dev on dev VM**
- **Found during:** Task 1
- **Issue:** Qt 6.8.2 includes Qt6::Bluetooth which triggers BluetoothDiscoveryService compilation requiring bluetooth.h
- **Fix:** Installed `libbluetooth-dev` via apt
- **Files modified:** None (system package)
- **Verification:** Full build succeeds

**4. [Rule 1 - Bug] Test file used removed acquire() method**
- **Found during:** Task 2
- **Issue:** test_video_frame_pool.cpp used `pool.acquire()` which was removed with Qt < 6.8 code
- **Fix:** Rewrote tests to use `acquireRecycled()`, updated assertions for recycled pool behavior
- **Files modified:** tests/test_video_frame_pool.cpp
- **Verification:** All 65 tests pass

---

**Total deviations:** 4 auto-fixed (1 bug, 3 blocking)
**Impact on plan:** All fixes necessary for successful build. No scope creep.

## Issues Encountered
- apt lock held by stale process (killed, ran dpkg --configure -a to recover)

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Dev VM now builds against Qt 6.8.2 with QtQuick.Effects available
- Ready for 04-02 (shadow/depth system implementation using MultiEffect)
- Pi already runs Qt 6.8.2, so no Pi-side changes needed

---
*Phase: 04-visual-depth*
*Completed: 2026-03-10*
