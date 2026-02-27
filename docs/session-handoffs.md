# Session Handoffs

Newest entries first.

---

## 2026-02-27 — Bluetooth cleanup, install script overhaul, aasdk removal

**What changed:**

Bluetooth:
- Created `BluetoothManager` — D-Bus Adapter1 setup, Agent1 pairing, PairedDevicesModel, auto-connect retry loop
- HSP HS profile registered by C++; HFP AG owned by PipeWire's bluez5 plugin (no conflict on fresh Trixie)
- `PairingDialog.qml` overlay for on-screen PIN confirmation
- BlueZ polkit rule for non-root pairing agent registration
- `updateConnectedDevice()` tracks Device1.Connected property changes

Install script (`install.sh`):
- Reordered: deps → build → hardware config (interactive prompts grouped after build)
- Hardware detection: INPUT_PROP_DIRECT touch filtering with device names, WiFi interface scan, audio sink selection via `pactl`
- WiFi: random password per install, country code auto-detection (iw reg → locale → ipinfo.io → US fallback), rfkill unblock
- BlueZ: `--compat` systemd override for SDP registration
- labwc: `mouseEmulation="no"` for multi-touch
- Services: openauto-prodigy, web config, system service — all created and enabled
- Launch option at end: starts app via systemd (works from SSH)
- Robustness: `{ ... exit; }` wrapper for self-update safety, ERR trap, `set -e`-safe conditionals (no `[[ ]] &&` pattern), hostapd failure non-fatal
- Build: cmake warnings suppressed by default (`--verbose`/`-v` to restore)

Documentation:
- Removed obsolete aasdk references from source code, CLAUDE.md, README, development.md, INDEX.md, aa-video-resolution.md
- Historical docs (design-decisions, debugging-notes, troubleshooting, phone-side-debug, cross-reference, archive) left as-is
- Updated roadmap: BT cleanup, install overhaul, aasdk removal → Done

**Status:** Install script validated on fresh RPi OS Trixie. Pairing works. AA connection blocked by SDP "Permission denied" (group membership needs reboot). Not yet validated end-to-end on fresh drive.

**Next steps:**
1. Reboot Pi and validate full AA session on fresh install
2. Investigate/suppress system pairing notification (draws over Prodigy UI)
3. Stale BT device pruning (two-pass approach — deferred)
4. Remove Python profile registration from codebase (Task 16 — pending)

---

## 2026-02-27 — Settings UI restructure & visual cleanup

**What changed:**
- Replaced settings tile grid with scrollable ListView + section headers
- Added `SettingsListItem.qml` control (icon + label + chevron)
- Added `SettingsQmlRole` to PluginModel for dynamic plugin settings pages
- Restructured AudioSettings into Output/Microphone sections
- Converted AboutSettings to Flickable
- Removed all small subtext, subtitles, hint text, and info banners from all settings pages — design rule: if it's not important enough to show prominently, it belongs in the web config panel
- Increased `rowH` (64→80) and `iconSize` (28→36) in UiMetrics for car-screen glanceability
- Removed dead `pinHint` reference from CompanionSettings
- Created `docs/settings-tree.md` — editable spec of every settings page, section, and control

**Why:**
- Original tile grid didn't scale with growing settings pages. Scrollable list with section headers is standard for settings UIs and handles any number of entries.
- Small screen at arm's length in a car needs big touch targets and no squinting at subtext.

**Status:** Complete. All 48 tests pass. Cross-compiled and validated on Pi.

**Key design rule established:**
- No `fontTiny` or `fontSmall` italic hint text on the Pi UI. Either show it at `fontBody` or move it to the web config panel.
- Edit `docs/settings-tree.md` to describe desired settings layout changes.

---

## 2026-02-27 — system-service shutdown timeout hardening

**What changed:**
- Updated `system-service/bt_profiles.py`:
  - `BtProfileManager.close()` now wraps `disconnect()` in try/except (called on event loop thread — `to_thread` is unsafe since dbus-next touches loop internals).
  - On exception, logs a warning and still clears `self._bus = None`.
- Updated `system-service/openauto_system.py` shutdown block:
  - Added `shutdown_sequence()` wrapped by `asyncio.wait_for(..., timeout=10.0)` for an overall teardown deadline.
  - Wrapped `proxy.disable()` with `asyncio.wait_for(..., timeout=5.0)` and warning on timeout.
  - Wrapped `ipc.stop()` with `asyncio.wait_for(..., timeout=3.0)` and warning on timeout.
  - If overall teardown timeout hits, logs forced shutdown error.
- Added/updated tests:
  - `system-service/tests/test_bt_profiles.py` for `close()` timeout warning + bus cleanup.
  - `system-service/tests/test_openauto_system.py` for proxy-disable timeout continuation, IPC-stop timeout warning, and overall forced-shutdown logging.

**Why:**
- Prevent shutdown deadlocks when D-Bus or bluetoothd is unresponsive and ensure teardown continues far enough to avoid lingering IPC socket/proxy state.

**Status:** Complete for requested `system-service` fixes and targeted tests. Repository-wide `ctest` has pre-existing environment/integration failures unrelated to these edits.

**Next steps:**
1. Re-run `ctest --output-on-failure` in an environment with display/network test prerequisites (or apply existing CI/headless test profile).
2. Validate daemon shutdown on target Pi with induced bluetoothd/D-Bus fault conditions.
3. If needed, add a focused `system-service` test target to CI so these Python reliability checks run independently of Qt integration constraints.

**Verification commands/results:**
- `pytest -q system-service/tests/test_bt_profiles.py`
  - Passed (`15 passed`).
- `pytest -q system-service/tests/test_openauto_system.py -k shutdown`
  - Passed (`3 passed, 9 deselected`).
- `pytest -q system-service/tests/test_openauto_system.py`
  - Passed (`12 passed`).
- `cd build && cmake --build . -j$(nproc)`
  - Passed (`Built target openauto-prodigy`).
- `cd build && ctest --output-on-failure`
  - Failed with 4 tests not related to `system-service` changes:
    - `test_tcp_transport` (listen/bind failure)
    - `test_companion_listener` (Qt platform/display plugin init failure)
    - `test_aa_orchestrator` (listen/bind failure on port 15277)
    - `test_video_frame_pool` (Qt platform/display plugin init failure)

---

## 2026-02-26 — Proto Repo Migration & Community Release

**What changed:**
- Created standalone repo [open-android-auto](https://github.com/mrmees/open-android-auto) with:
  - 164 proto files organized into 13 categories (moved from `libs/open-androidauto/proto/`)
  - Protocol docs: reference, cross-reference, wireless BT setup, video resolution, display rendering, phone-side debug, troubleshooting
  - Decompiled headunit firmware analysis (Alpine, Kenwood, Pioneer, Sony)
  - APK indexer tools + 156MB SQLite database (git-lfs)
- Integrated open-android-auto as git submodule in openauto-prodigy
- Updated CMakeLists.txt with custom protoc invocation (preserves `oaa/<category>/` directory structure)
- Updated 25 C++ source files (129 includes) for new proto paths
- Removed old flat proto files from openauto-prodigy
- Cleaned duplicated docs from openauto-prodigy (firmware, protocol reference, APK indexer)
- Fixed broken `docs/INDEX.md` links (aa-protocol/ paths never existed)
- Merged PR #5 (video ACK delta fix) and removed dead `ackCounter_` from both handlers

**Why:**
- Proto definitions are the most broadly useful artifact from this project. Standalone repo lets the AA community use them without pulling in the full head unit implementation.
- Deduplication keeps openauto-prodigy focused on implementation.

**Status:** Complete. Both repos pushed, Pi deployed with latest build, 48/48 tests pass.

**Key gotcha for future reference:**
- `protobuf_generate_cpp` puts all generated files flat — doesn't preserve directory structure. Must use custom `foreach` + `add_custom_command` with proper `--proto_path` when protos have subdirectory imports.

---

## 2026-02-26 — Video ACK Delta Fix (Gearhead RxVid Crash Candidate)

**What changed:**
- Updated video ACK behavior in `libs/open-androidauto/src/HU/Handlers/VideoChannelHandler.cpp`:
  - `AVMediaAckIndication.value` now sends delta permits (`1` per frame) instead of cumulative `ackCounter_`.
- Added regression coverage in `tests/test_video_channel_handler.cpp`:
  - `testMediaDataEmitsFrameAndAck` now sends two frames and validates both ACK payload values are `1`.

**Why:**
- Phone logs showed repeated Gearhead crash: `FATAL EXCEPTION: RxVid` with `java.lang.Error: Maximum permit count exceeded`.
- Cumulative video ACK values can over-replenish phone-side permits (triangular growth) and plausibly trigger semaphore overflow.
- Audio channel already uses delta ACK semantics; video now matches that flow-control model.

**Status:** Complete and verified locally (build + full tests pass).

**Next steps:**
1. Run extended real-device AA session (>40 minutes at 30fps) to confirm no recurrence of `RxVid` / `Maximum permit count exceeded`.
2. Capture and compare phone logcat + Pi hostapd timeline during validation session.
3. Commit and push this change set to the active Prodigy branch/PR.

**Verification commands/results:**
- `cd build && cmake --build . -j$(nproc) --target test_video_channel_handler && ctest -R test_video_channel_handler --output-on-failure`
  - First run (before fix): failed on ACK payload value (`actual 2`, `expected 1`).
  - Second run (after fix): passed.
- `cd build && cmake --build . -j$(nproc)`
  - Passed (`Built target openauto-prodigy`).
- `cd build && ctest --output-on-failure`
  - Passed (`100% tests passed, 0 tests failed out of 48`).

---

## 2026-02-26 — Documentation Cleanup & Structured Workflow

**What changed:**
- Created structured workflow files: AGENTS.md, project-vision.md, roadmap-current.md, session-handoffs.md, wishlist.md
- Moved 10 AA protocol docs into `docs/aa-protocol/` subdirectory
- Imported openauto-pro-community repo into `docs/OpenAutoPro_archive_information/` (18 files)
- Consolidated 57 completed plan docs into 5 milestone summaries (`milestone-01` through `milestone-05`)
- Moved 3 active plans to `docs/plans/active/`
- Moved 4 workspace loose files to `docs/OpenAutoPro_archive_information/needs-review/`
- Rewrote INDEX.md to match new structure
- Rewrote CLAUDE.md — trimmed from ~20KB to ~15KB, removed stale status/philosophy sections, added workflow references
- Updated workspace CLAUDE.md to reflect consolidation
- Trimmed MEMORY.md to remove content now captured in milestone summaries

**Why:** 10 days of intense development left docs scattered across 3 repos with no workflow structure. Companion-app had a proven AGENTS.md workflow loop — replicated it here.

**Status:** Complete. All 14 tasks executed. Commits are on `feat/proxy-routing-exceptions` branch.

**Next steps:**
1. Review `docs/OpenAutoPro_archive_information/needs-review/` files — decide final disposition for miata-hardware-reference.md (likely move to main docs as plugin input)
2. Start using the workflow loop — next session should reference roadmap-current.md priorities
3. Consider archiving the openauto-pro-community GitHub repo now that content is imported

**Verification:**
- 14 reference docs at `docs/` root
- 5 milestone summaries + 3 active plans in `docs/plans/`
- 10 AA protocol docs in `docs/aa-protocol/`
- 22 archive files in `docs/OpenAutoPro_archive_information/`
- No orphaned plan files

---

## 2026-02-27 — Prebuilt Distribution Packaging Workflow

**What changed:**
- Added a prebuilt installer script: `install-prebuilt.sh`.
  - Installs runtime dependencies.
  - Prompts for hardware/network setup (WiFi AP + touch discovery).
  - Deploys packaged payload to `~/openauto-prodigy`.
  - Generates `~/.openauto/config.yaml` and installs/enables systemd services.
- Added release packaging helper: `tools/package-prebuilt-release.sh`.
  - Builds a distributable tarball from `build-pi/src/openauto-prodigy` + runtime payload files.
  - Produces `dist/openauto-prodigy-prebuilt-<tag>.tar.gz`.
- Added automated test coverage for artifact layout:
  - `tests/test_prebuilt_release_package.py`.
  - Registered in `tests/CMakeLists.txt` as `test_prebuilt_release_package`.
- Updated docs:
  - `README.md` (new prebuilt distribution section).
  - `docs/development.md` (prebuilt install path + package creation commands).
  - `docs/roadmap-current.md` (marked prebuilt distribution workflow complete in recent done items).
- Updated `.gitignore` to ignore `dist/` artifacts.

**Why:**
- Enable shipping the current app state as a Pi-ready prebuilt release, avoiding source builds on target systems.
- Keep distribution repeatable and testable via a scripted packaging flow.

**Status:** Complete and verified locally/cross-compile. PR branch contains scripts, tests, and docs updates.

**Next steps:**
1. Validate `install-prebuilt.sh` end-to-end on target Pi hardware from a fresh tarball extract.
2. Optionally trim payload contents (for example, omit `system-service/tests`) to reduce archive size.
3. Add CI job to run `test_prebuilt_release_package` and optionally produce release artifacts.

**Verification commands/results:**
- `cd build && cmake --build . -j$(nproc)`
  - Passed.
- `cd build && ctest --output-on-failure`
  - Passed (`100% tests passed, 0 tests failed out of 51`).
- `./cross-build.sh -DCMAKE_BUILD_TYPE=Release`
  - Passed (`Build complete: build-pi/src/openauto-prodigy`).
- `cd build && ctest -R test_prebuilt_release_package --output-on-failure`
  - Red phase: failed as expected before implementation (`FileNotFoundError` for missing packaging script).
  - Green phase: passed after implementation.
- `./tools/package-prebuilt-release.sh --build-dir build-pi --output-dir dist --version-tag local-test`
  - Passed; created `dist/openauto-prodigy-prebuilt-local-test.tar.gz`.

---

## 2026-02-27 — Installer Mode Selection + Release Convention Finalization

**What changed:**
- Expanded `install.sh` to support:
  - `--mode <source|prebuilt>`
  - `--list-prebuilt`
  - `--prebuilt-index <N>`
  - interactive mode picker (local build vs GitHub prebuilt download)
- Added GitHub prebuilt release discovery/listing in `install.sh`, including compatibility with legacy prebuilt asset names.
- Added/extended platform guardrails in `install.sh`:
  - OS family check (`debian` / `raspbian`)
  - architecture check (`aarch64`/`armv7l`)
  - hardware model check for Raspberry Pi 4 (`/proc/device-tree/model`)
  - checks now run for both source and prebuilt install flows.
- Finalized release packaging convention in `tools/package-prebuilt-release.sh`:
  - target-qualified asset naming: `openauto-prodigy-prebuilt-<tag>-<target>.tar.gz`
  - new `--target` argument (default `pi4-aarch64`)
  - added `RELEASE.json` metadata payload.
- Added test coverage:
  - new `tests/test_install_list_prebuilt.py`
  - updated `tests/test_prebuilt_release_package.py` for target-qualified name + `RELEASE.json`
  - registered new test in `tests/CMakeLists.txt`.
- Updated docs:
  - `README.md`, `docs/development.md` (mode options + OS/hardware checks + naming)
  - `docs/release-packaging.md` (new convention doc)
  - `docs/INDEX.md` (linked release-packaging doc)
  - `docs/roadmap-current.md` (done item expanded).

**Why:**
- End users need a first-class choice between local source builds and precompiled releases.
- Distribution artifacts need a stable, explicit naming structure before publishing releases.
- Installer should validate both software platform and target hardware before proceeding to reduce unsupported installs and troubleshooting churn.

**Status:** Complete in `feature/prebuilt-distribution`; ready to push as follow-up commit on PR #6.

**Next steps:**
1. Validate `install.sh --mode prebuilt` end-to-end on actual Pi 4 hardware using a GitHub-hosted release tarball.
2. Tag and publish first release using the finalized convention (`vX.Y.Z` + `-pi4-aarch64` asset suffix).
3. Optionally add CI coverage for `install.sh --list-prebuilt` and packaging script execution in release workflows.

**Verification commands/results:**
- `bash -n install.sh`
  - Passed.
- `bash -n tools/package-prebuilt-release.sh`
  - Passed.
- `python3 -m py_compile tests/test_install_list_prebuilt.py tests/test_prebuilt_release_package.py`
  - Passed.
- `cd build && ctest -R 'test_install_list_prebuilt|test_prebuilt_release_package' --output-on-failure`
  - Passed (`2/2`).
- `cd build && cmake --build . -j$(nproc)`
  - Passed.
- `cd build && ctest --output-on-failure`
  - Passed (`100% tests passed, 0 tests failed out of 52`).
- `./cross-build.sh -DCMAKE_BUILD_TYPE=Release`
  - Passed (`Build complete: build-pi/src/openauto-prodigy`).
