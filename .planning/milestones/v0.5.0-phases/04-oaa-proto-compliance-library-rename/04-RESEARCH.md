# Phase 4: OAA Proto Compliance & Library Rename - Research

**Researched:** 2026-03-08
**Domain:** C++ codebase cleanup, CMake library rename, git submodule path change, test maintenance
**Confidence:** HIGH

## Summary

This phase is pure housekeeping -- no new protocol features, no new libraries, no external research needed. The work is entirely within the existing codebase: (1) remove dead code left over from retracted proto v1.2 messages, (2) fix a bug where the WiFi SDP descriptor sends SSID instead of BSSID, (3) rename the `libs/open-androidauto/` directory and CMake target to `prodigy-oaa-protocol`, and (4) fix the one failing test and ensure 100% pass rate.

All changes are mechanical and well-scoped. The only risk is the git submodule path change during the library rename, which requires careful `.gitmodules` editing and `git mv` to preserve submodule tracking.

**Primary recommendation:** Do the rename last (after dead code cleanup and bssid fix) to minimize the number of files that need both content changes AND path changes.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Full removal of dead code -- no stubs, no "RETRACTED" comments
- Remove all dead signals, slots, orchestrator connections for retracted protos
- Specifically remove: AudioFocusState handler stubs, AudioStreamType handler stubs, NavigationFocusIndication handler stub, any lingering MediaPlaybackCommand references
- Remove MessageIds.hpp constants for retracted messages (0x8021, 0x8022)
- Verify and fix WiFi ssid/bssid semantics in SDP descriptor
- Update ROADMAP.md and REQUIREMENTS.md to reflect v1.2 reality
- Rename `libs/open-androidauto/` to `libs/prodigy-oaa-protocol/`
- Rename CMake target from `open-androidauto` to `prodigy-oaa-protocol`
- Keep C++ namespace as `oaa::` -- unchanged
- Proto submodule stays nested: `libs/prodigy-oaa-protocol/proto/`
- All tests must be green (100% pass rate) at phase completion
- Fix the failing navbar controller test
- Remove tests that reference retracted APIs/protos
- Add new tests covering v1.2 code paths
- Keep debug panel buttons for Assist(219) and Voice(231) keycodes

### Claude's Discretion
- Exact ordering of cleanup vs rename operations
- Whether to batch all dead code removal into one plan or split by subsystem
- Test implementation details (mock strategies, assertion patterns)

### Deferred Ideas (OUT OF SCOPE)
- Verify keycode-based media/voice control on Pi (keycodes 219/231) -- separate phase
- Functional validation of v1.2 protocol behavior end-to-end -- separate testing phase
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CLN-01 | Remove all retracted proto dead code (handlers, signals, stubs, comments) | Dead code locations fully mapped below -- AudioChannelHandler, NavigationChannelHandler, Orchestrator, MessageIds.hpp, ServiceDiscoveryBuilder |
| CLN-02 | WiFi SDP descriptor sends actual BSSID (MAC address), not SSID | Bug confirmed: `ServiceDiscoveryBuilder::buildWifiDescriptor()` passes `wifiSsid_` to `set_bssid()`. Fix pattern exists in `BluetoothDiscoveryService` |
| REN-01 | Protocol library renamed to `libs/prodigy-oaa-protocol/` with matching CMake target | All 46 files referencing `open-androidauto` identified. Submodule path change documented |
| TST-01 | All 64 tests pass (100% pass rate) | Failing test identified: `test_navbar_controller::testHoldProgressEmittedDuringHold`. Retracted-API test comments already cleaned in prior phases |
</phase_requirements>

## Standard Stack

No new libraries. All work uses existing project tools:

| Tool | Version | Purpose |
|------|---------|---------|
| CMake | 3.22+ | Build system -- target rename, path updates |
| Qt 6.4/6.8 | Dual-platform | Test framework (QTest), network interfaces (QNetworkInterface) |
| git | System | `git mv` for directory rename, `.gitmodules` update |
| protobuf | System | Proto-generated headers unchanged (namespace stays `oaa::`) |

## Architecture Patterns

### Dead Code Removal Inventory

All locations requiring changes, verified by grep:

**`libs/open-androidauto/include/oaa/HU/Handlers/AudioChannelHandler.hpp`**
- Line 27: Comment about removed signals -- DELETE
- Lines 33-34: `handleAudioFocusState()` and `handleAudioStreamType()` private method declarations -- DELETE

**`libs/open-androidauto/src/HU/Handlers/AudioChannelHandler.cpp`**
- Line 11: Comment about removed pb.h includes -- DELETE
- Lines 54-59: Switch cases for `AUDIO_FOCUS_STATE` (0x8021) and `AUDIO_STREAM_TYPE` (0x8022) -- DELETE (unhandled-message logger from Phase 1 catches these via `unknownMessage` signal in default case)
- Lines 153-165: `handleAudioFocusState()` and `handleAudioStreamType()` method bodies -- DELETE

**`libs/open-androidauto/include/oaa/Channel/MessageIds.hpp`**
- Lines 32-33: `AUDIO_FOCUS_STATE = 0x8021` and `AUDIO_STREAM_TYPE = 0x8022` constants -- DELETE
- Line 63: Comment about NAV_FOCUS_INDICATION being retracted -- DELETE (the constant was already removed, only comment remains)

**`libs/open-androidauto/include/oaa/HU/Handlers/NavigationChannelHandler.hpp`**
- Line 40: Comment about removed navigationFocusChanged -- DELETE

**`libs/open-androidauto/src/HU/Handlers/NavigationChannelHandler.cpp`**
- Line 8: `#include "NavigationFocusIndicationMessage.pb.h"` -- DELETE (include for retracted proto)
- Lines 46-48: Switch case for `0x8005` (retracted NAV_FOCUS_INDICATION) -- DELETE (falls to default/unhandled)
- Lines 170-171: Comment about removed handleFocusIndication -- DELETE

**`src/core/aa/AndroidAutoOrchestrator.cpp`**
- Line 347: Comment about removed audioFocusStateChanged/audioStreamTypeChanged -- DELETE
- Line 429: Comment about removed navigationFocusChanged -- DELETE

**`src/core/aa/ServiceDiscoveryBuilder.cpp`**
- Line 25: Comment about retracted PhoneStatusChannelData -- DELETE

### WiFi BSSID Fix

**Bug:** `ServiceDiscoveryBuilder::buildWifiDescriptor()` (line 401) calls:
```cpp
wifiChannel->set_bssid(wifiSsid_.toStdString());
```
This sends the SSID string (e.g., "OpenAutoProdigy") as the BSSID field. The proto definition (`WifiChannelData.proto`) documents this field as "HU WiFi AP BSSID" with gold confidence from deep trace.

**Fix pattern:** `BluetoothDiscoveryService::buildWifiInfoResponse()` already correctly retrieves the wlan0 MAC:
```cpp
for (const auto& iface : QNetworkInterface::allInterfaces()) {
    if (iface.name() == "wlan0")
        bssid = iface.hardwareAddress();
}
```

**Implementation:** Either:
1. Pass the BSSID (MAC) to `ServiceDiscoveryBuilder` alongside the SSID, or
2. Have `ServiceDiscoveryBuilder` read it directly via `QNetworkInterface`

Option 1 is cleaner -- the builder already takes constructor params. Add a `wifiBssid` parameter. The orchestrator already has access to `QNetworkInterface` (same code path as `BluetoothDiscoveryService`).

### Library Rename Procedure

**Step 1: Update `.gitmodules`**
```ini
# Before:
[submodule "libs/open-androidauto/proto"]
    path = libs/open-androidauto/proto
    url = https://github.com/mrmees/open-android-auto.git

# After:
[submodule "libs/prodigy-oaa-protocol/proto"]
    path = libs/prodigy-oaa-protocol/proto
    url = https://github.com/mrmees/open-android-auto.git
```

**Step 2: Move the directory**
```bash
git mv libs/open-androidauto libs/prodigy-oaa-protocol
```
Git handles the submodule tracking if `.gitmodules` is updated first. The submodule itself (`proto/`) moves with its parent.

**Step 3: Update all references (46 files found, but filter to source/build files)**

Source files requiring path/target name changes:
| File | Change |
|------|--------|
| `CMakeLists.txt` (root) | `add_subdirectory(libs/prodigy-oaa-protocol)`, comment update |
| `libs/prodigy-oaa-protocol/CMakeLists.txt` | `project(prodigy-oaa-protocol ...)`, `add_library(prodigy-oaa-protocol ...)`, `target_include_directories(prodigy-oaa-protocol ...)`, `target_link_libraries(prodigy-oaa-protocol ...)` |
| `src/CMakeLists.txt` | Include paths and link target: `prodigy-oaa-protocol` |
| `src/core/Logging.cpp` | Reference in logging filter string if any |
| `src/core/Logging.hpp` | Reference in logging filter string if any |
| `CLAUDE.md` | Documentation references |
| `README.md` | Documentation references |
| `docs/development.md` | Build instructions |
| `.gitmodules` | Submodule path |

**C++ namespace stays `oaa::`** -- no source code changes needed for namespace. Only CMake target names and filesystem paths change.

### Navbar Test Fix

The failing test is `testHoldProgressEmittedDuringHold` -- assertion `lastProgress > firstProgress` returns FALSE. This is a timing-dependent test using `QTest::qWait()` for gesture hold detection. The hold progress timer likely isn't firing reliably in the test harness. Fix by either:
1. Increasing wait times
2. Using `QSignalSpy::wait()` with timeout instead of `qWait()`
3. Directly invoking the timer callback in tests

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Git submodule rename | Manual file moves + .git/modules editing | `git mv` after `.gitmodules` update | Git tracks submodule paths internally; manual moves break tracking |
| MAC address retrieval | Shell out to `ip link show` or parse `/sys/class/net/*/address` | `QNetworkInterface::allInterfaces()` | Already used in `BluetoothDiscoveryService`, cross-platform Qt API |

## Common Pitfalls

### Pitfall 1: Submodule Path Corruption
**What goes wrong:** Moving a git submodule directory without updating `.gitmodules` first leaves git in a confused state where the submodule can't be found.
**How to avoid:** Update `.gitmodules` BEFORE `git mv`. Then `git add .gitmodules` and the new path together.

### Pitfall 2: Stale Build Cache After Rename
**What goes wrong:** CMake cache retains old library paths. Build appears to succeed but links against stale objects.
**How to avoid:** Clean build directory after rename (`rm -rf build && mkdir build`), or at minimum delete `CMakeCache.txt`.

### Pitfall 3: Removing Switch Cases Without Safety Net
**What goes wrong:** Removing `case AUDIO_FOCUS_STATE:` from the switch means 0x8021 falls to `default:` which emits `unknownMessage`. This is correct -- the Phase 1 unhandled-message logger catches it. But verify the default case path actually exists and works.
**How to avoid:** Check that the `default:` case in `AudioChannelHandler::onMessage()` emits `unknownMessage(messageId, data)` -- confirmed it does (line 80-81).

### Pitfall 4: NavigationFocusIndicationMessage.pb.h Include
**What goes wrong:** `NavigationChannelHandler.cpp` line 8 includes `NavigationFocusIndicationMessage.pb.h`. If this proto file still exists in the submodule (it does -- protos aren't deleted, just marked retracted), removing the include is safe. But if the proto was actually deleted, builds would fail before this phase.
**How to avoid:** The include should be removed regardless. No code in the handler uses it after the v1.2 cleanup.

## Code Examples

### BSSID Fix Pattern
```cpp
// In ServiceDiscoveryBuilder constructor -- add wifiBssid parameter
ServiceDiscoveryBuilder::ServiceDiscoveryBuilder(
    oap::YamlConfig* yamlConfig,
    const QString& btMacAddress,
    const QString& wifiSsid,
    const QString& wifiPassword,
    const QString& wifiBssid)  // NEW
    : yamlConfig_(yamlConfig)
    , btMacAddress_(btMacAddress)
    , wifiSsid_(wifiSsid)
    , wifiPassword_(wifiPassword)
    , wifiBssid_(wifiBssid)     // NEW
{}

// In buildWifiDescriptor -- use actual BSSID
wifiChannel->set_bssid(wifiBssid_.toStdString());  // Was: wifiSsid_.toStdString()
```

### CMake Target Rename Pattern
```cmake
# libs/prodigy-oaa-protocol/CMakeLists.txt
project(prodigy-oaa-protocol VERSION 0.1.0 LANGUAGES CXX)

add_library(prodigy-oaa-protocol STATIC ...)
target_include_directories(prodigy-oaa-protocol PUBLIC ...)
target_link_libraries(prodigy-oaa-protocol PUBLIC ...)
```

```cmake
# src/CMakeLists.txt
target_include_directories(openauto-core PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/prodigy-oaa-protocol/include
    ${CMAKE_BINARY_DIR}/libs/prodigy-oaa-protocol
)
target_link_libraries(openauto-core PUBLIC prodigy-oaa-protocol)
```

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Qt Test (QTest) via Qt 6.4/6.8 |
| Config file | `tests/CMakeLists.txt` with `oap_add_test()` helper |
| Quick run command | `ctest --test-dir build --output-on-failure` |
| Full suite command | `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CLN-01 | Retracted handlers removed, unknowns fall to default | unit | `ctest --test-dir build -R test_audio_channel_handler --output-on-failure` | Existing (update needed) |
| CLN-01 | Retracted nav focus code removed | unit | `ctest --test-dir build -R test_navigation_channel_handler --output-on-failure` | Existing (no change needed) |
| CLN-02 | WiFi descriptor sends MAC not SSID | unit | `ctest --test-dir build -R test_service_discovery_builder --output-on-failure` | Existing (add bssid test) |
| REN-01 | Library compiles under new name | build | `cd build && cmake --build . -j$(nproc)` | N/A (build verification) |
| TST-01 | All 64 tests pass | full suite | `ctest --test-dir build --output-on-failure` | Existing |
| TST-01 | Navbar controller hold test fixed | unit | `ctest --test-dir build -R test_navbar_controller --output-on-failure` | Existing (fix needed) |

### Sampling Rate
- **Per task commit:** `ctest --test-dir build --output-on-failure`
- **Per wave merge:** Full rebuild + `ctest --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
None -- existing test infrastructure covers all phase requirements. Only modifications to existing tests needed:
- [ ] Update `test_audio_channel_handler.cpp` -- verify retracted message IDs (0x8021, 0x8022) now emit `unknownMessage` instead of being handled
- [ ] Add bssid test to `test_service_discovery_builder.cpp` -- verify WiFi descriptor contains MAC format
- [ ] Fix `test_navbar_controller.cpp::testHoldProgressEmittedDuringHold` -- timing issue

## Open Questions

1. **NavigationFocusIndicationMessage.pb.h removal safety**
   - What we know: The include exists on line 8 of NavigationChannelHandler.cpp. No code references it after v1.2 cleanup.
   - What's unclear: Whether the proto file itself still exists in the current submodule commit (ed53047). It should -- protos are retracted, not deleted.
   - Recommendation: Remove the include. If the proto file doesn't exist, the build already fails; if it does exist, the include is dead weight.

2. **Test count after cleanup**
   - What we know: Currently 64 tests, 63 passing. The retracted-API tests were already cleaned in prior phases (only comments remain).
   - What's unclear: Whether removing the retracted handler stubs changes the test count or just modifies existing test assertions.
   - Recommendation: Test count should stay at 64. The `testRetractedMessageDoesNotCrash` test in `test_audio_channel_handler.cpp` should be updated to verify messages fall to `unknownMessage` instead of being silently handled.

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection of all files listed above
- `WifiChannelData.proto` field 1 comment: "HU WiFi AP BSSID" (gold confidence, deep_trace)
- `BluetoothDiscoveryService.cpp` existing BSSID retrieval pattern
- `MessageIds.hpp` complete message ID inventory
- `AudioChannelHandler.cpp/hpp` retracted handler stubs
- `.gitmodules` current submodule configuration

### Secondary (MEDIUM confidence)
- `test_navbar_controller` failure analysis -- timing issue diagnosis based on error message and QTest::qWait pattern

## Metadata

**Confidence breakdown:**
- Dead code inventory: HIGH -- verified by grep across entire source tree
- BSSID fix: HIGH -- proto docs + existing correct implementation in BluetoothDiscoveryService
- Library rename: HIGH -- all references enumerated, git submodule rename is well-documented
- Test fix: MEDIUM -- navbar test failure is timing-dependent; exact fix needs experimentation

**Research date:** 2026-03-08
**Valid until:** Indefinite (codebase-specific, no external dependencies)
