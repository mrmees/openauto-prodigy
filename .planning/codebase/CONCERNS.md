# Codebase Concerns

**Analysis Date:** 2026-03-01

---

## Tech Debt

**Python BT profile registration still in codebase (Task 16 — PENDING):**
- Issue: `system-service/bt_profiles.py` registers HFP AG, HSP HS, and AA UUID profiles via D-Bus. This was the original approach before `BluetoothManager` was rewritten in C++. The C++ code now owns these profiles. The Python file is dead code but hasn't been removed yet.
- Files: `system-service/bt_profiles.py`, `system-service/openauto_system.py`
- Impact: Dual profile registration races if both are active. Confusion about where profiles are managed. Adds noise to any future refactor of the system service.
- Fix approach: Delete `system-service/bt_profiles.py`, remove its import from `system-service/openauto_system.py`, verify C++ `BluetoothManager` covers all profiles end-to-end on Pi.

**Phone plugin — dial and DTMF are UI stubs only:**
- Issue: `PhonePlugin::dial()` and `PhonePlugin::sendDTMF()` update UI state but do not actually initiate calls or send tones over BlueZ HFP.
- Files: `src/plugins/phone/PhonePlugin.cpp` lines 342-399
- Impact: Dialing from the phone plugin silently does nothing over the air. The phone UI shows "Dialing" but the phone never rings.
- Fix approach: Implement HFP AG call control via BlueZ `org.bluez.NetworkServer1` or the RFCOMM AT command layer. DTMF requires `AT+VTS` over HFP AG.

**Companion pairing PIN uses weak key derivation:**
- Issue: `CompanionListenerService::generatePairingPin()` derives the shared secret as `SHA256(PIN + ":openauto-companion-v1")`. The PIN is a 6-digit number (100000–999999), giving ~900k possibilities. SHA256 is fast — an offline brute force of a captured challenge/response takes under a second.
- Files: `src/core/services/CompanionListenerService.cpp` lines 106-141
- Impact: An attacker on the local AP who captures the BT-pairing QR code or sniffs the challenge/response can crack the session key trivially. On a local WiFi AP with no outside internet exposure the blast radius is limited, but it's a design flaw.
- Fix approach: Replace with PBKDF2 or Argon2id for key derivation, or switch to a proper pairing ceremony (e.g. SRP or SPAKE2). Wishlist already notes "per-connection WiFi password rotation" as a possible path forward.

**Session key transmitted in plaintext over companion channel:**
- Issue: After HMAC auth, `hello_ack` sends the session key as `"session_key": rawKey.toHex()` with a comment noting "XOR for v1, upgrade later". The key is transmitted unencrypted over a TCP connection.
- Files: `src/core/services/CompanionListenerService.cpp` lines 279-284
- Impact: Anyone who can intercept the hello_ack message gets the session key and can forge subsequent status messages (GPS, battery, proxy). This is only practical on the AP itself (phone-to-Pi link), but it means a rogue device that connects before the phone can inject GPS or SOCKS5 proxy config.
- Fix approach: Encrypt the session key with the shared secret (AES-128-GCM or similar) before transmission.

**Companion host IP hardcoded to 10.0.0.1 in QR payload:**
- Issue: `CompanionListenerService::generatePairingPin()` builds `openauto://pair?...&host=10.0.0.1` with the AP IP hardcoded.
- Files: `src/core/services/CompanionListenerService.cpp` line 129
- Impact: If the AP interface changes (e.g. ap0 instead of wlan0, or different subnet), the QR code points to the wrong IP and pairing fails silently.
- Fix approach: Read the AP IP dynamically from the interface at QR generation time. The same pattern exists in `BluetoothDiscoveryService` for the WiFi IP lookup.

**`QmlConfig` type coercion uses exception-swallowing try-catch cascade:**
- Issue: `YamlConfig::pluginValue()` detects the scalar type (bool → int → double → string) via nested try/catch blocks that silently swallow all exceptions.
- Files: `src/core/YamlConfig.cpp` lines 607-632
- Impact: A YAML parse error in a plugin's config key silently returns an empty QVariant. No log entry, no diagnostic path. Plugin misconfiguration is invisible.
- Fix approach: Log the caught exception message and key path at Warning level before returning the fallback.

**`ApplicationController::restart()` shell command argument injection potential:**
- Issue: The restart command in `ApplicationController::restart()` constructs a shell command string using `QCoreApplication::arguments()` joined without quoting: `relaunch += " " + args[i]`. If any argument contains spaces or shell metacharacters, the command breaks or could inject shell code.
- Files: `src/ui/ApplicationController.cpp` lines 62-70
- Impact: In practice `args` are controlled (the app is launched by a systemd service), so exploitation is unlikely. But the pattern is fragile — a future launcher or wrapper that passes complex args would silently break restart.
- Fix approach: Use `QProcess::startDetached()` with a separate `QStringList` instead of building a shell string. The `kill -0` polling loop can stay in the shell command since it doesn't use args.

**Protocol library is Qt-coupled — blocks community library extraction:**
- Issue: `libs/open-androidauto/` uses Qt types throughout (QObject, QByteArray, QString, QTcpSocket, QTimer). This couples the AA protocol implementation to Qt and makes it impossible to separate into a language-neutral community library.
- Files: `libs/open-androidauto/` (entire directory)
- Impact: Long-term goal (see `docs/wishlist.md`) to extract the protocol layer into its own repo is blocked until Qt is removed. This is a significant architectural constraint.
- Fix approach: Documented in wishlist — replace QObject signals/slots with `std::function` callbacks, QByteArray/QString with STL, QTcpSocket with Boost.ASIO, QTimer with ASIO timers.

---

## Known Bugs / Incomplete Features

**Brightness slider in GestureOverlay does nothing:**
- Symptoms: Dragging the brightness slider in the 3-finger overlay updates the UI value but has no effect on display brightness.
- Files: `qml/components/GestureOverlay.qml` line 125
- Trigger: Open gesture overlay → drag brightness slider
- Workaround: None. Display brightness must be set via system tools or config.
- Fix approach: Implement `IDisplayService.setBrightness()` interface and wire it to the slider. The GPIO or sysfs `/sys/class/backlight/` path will be Pi-specific.

**Sidebar position/size changes require reconnect to take effect:**
- Symptoms: Changing the sidebar (position, width, enable/disable) in settings updates the QML layout but the AA video content area is locked at the resolution sent in the initial `VideoConfig`. The phone continues rendering for the old margins until the session is restarted.
- Files: `src/core/aa/AndroidAutoOrchestrator.cpp` (session setup), `docs/roadmap-current.md`
- Trigger: Change sidebar settings while AA is active
- Workaround: Disconnect and reconnect AA.
- Fix approach: Tracked on roadmap. Investigate `UpdateHuUiConfigRequest` (0x8012 in `UiConfigMessages.proto`) to push margin updates mid-session.

**BT device list spams "Found N paired device(s)" on every D-Bus property change:**
- Symptoms: Every change to any `org.bluez.Device1` property (e.g. signal strength updates during AA connection) triggers `refreshPairedDevices()`, which logs at Info level. In steady-state AA use this produces continuous log noise.
- Files: `src/core/services/BluetoothManager.cpp` line 715, `onDevicePropertiesChanged()` line 753
- Trigger: Active AA session with BT connected
- Workaround: None currently; roadmap item "Reduce unnecessary logging".
- Fix approach: Gate `refreshPairedDevices()` calls — only trigger on relevant property changes (`Paired`, `Trusted`, `Connected`). Change the log to `qDebug()` or only log on actual change.

---

## Security Considerations

**Web config panel has no authentication:**
- Risk: `web-config/server.py` binds to `0.0.0.0:8080` with no authentication layer. Any device on the Pi's AP (10.0.0.1) — i.e., any phone connected to the Android Auto AP — can access the config panel and change WiFi password, theme, audio routing, or toggle protocol capture.
- Files: `web-config/server.py` line 152
- Current mitigation: Panel is only reachable from devices on the AP subnet. The AP is normally only the AA phone.
- Recommendations: Add HTTP Basic Auth gated on a per-instance secret (e.g. derived from the companion shared secret). Or restrict the Flask listener to `127.0.0.1` and require SSH tunnel for legitimate admin use.

**WiFi password stored and transmitted in plaintext:**
- Risk: `IpcServer::handleGetConfig()` includes `wifi_password` in the JSON response to the web panel. Any authenticated IPC client can read the WiFi AP password. The web config panel also shows it in a settings field.
- Files: `src/core/services/IpcServer.cpp` lines 155-156, `web-config/server.py` (settings template)
- Current mitigation: IPC socket at `/tmp/openauto-prodigy.sock` requires local access.
- Recommendations: Mask the password in `get_config` response (return `"****"` unless explicitly requested). Consider the wishlist item "per-connection WiFi password rotation" as a longer-term fix.

**System pairing notification draws over app UI:**
- Risk: When a new phone pairs, the OS-level Bluetooth pairing dialog appears on top of the Prodigy UI. This is a user experience issue but also a potential confusion/phishing vector (an attacker with BT access could trigger unexpected confirmations on the car screen).
- Files: OPEN item in `docs/session-handoffs.md` line 38
- Current mitigation: None — issue is not yet addressed.
- Recommendations: Investigate suppressing the BlueZ/labwc pairing notification (polkit policy, or intercept via the `BluezAgentAdaptor` that already handles `RequestConfirmation`).

---

## Performance Bottlenecks

**`AudioService` lock ordering is fragile (ABBA deadlock potential):**
- Issue: `AudioService::~AudioService()` documents "Acquire PW lock FIRST, then mutex_ — same as setMasterVolume() to prevent ABBA deadlock." This is a lock-ordering convention enforced entirely by code comments. Any new code path that acquires the locks in the wrong order will deadlock the audio thread silently.
- Files: `src/core/services/AudioService.cpp` lines 77-98
- Cause: PipeWire's thread loop lock and the AudioService mutex_ must always be acquired in the same order. The convention is correct but fragile.
- Improvement path: Audit all callers; add a lock-order assertion macro; or redesign to eliminate the dual-lock by moving PW callbacks to dispatch through Qt's event system (already done for video, could extend to audio).

**PipeWire version divergence between dev VM and Pi:**
- Issue: Dev VM has PipeWire 1.0.5; Pi has 1.4.2. `pw_stream_set_rate()` (used for adaptive buffer sizing) is stubbed out on the VM. Any audio timing feature added for 1.4.x will silently no-op during dev.
- Files: `src/core/services/AudioService.cpp` lines 7-10
- Cause: Ubuntu 24.04 ships PipeWire 1.0.x; RPi OS Trixie ships 1.4.x.
- Improvement path: Not urgent — audio works on both. Add a version check comment where the stub matters so future features are aware.

---

## Fragile Areas

**`teardownSession()` — deletion-while-signaling pattern:**
- Files: `src/core/aa/AndroidAutoOrchestrator.cpp` lines 567-605
- Why fragile: `teardownSession()` is called from within `onSessionStateChanged()`, which is a signal handler of `session_`. It uses `deleteLater()` to defer destruction, which is correct, but the reasoning is not obvious. Any refactor that changes `deleteLater()` back to `delete` will cause a use-after-free when Qt resumes signal dispatch.
- Safe modification: Never change `session_->deleteLater()` to `delete session_` in teardown. Add a static_assert or comment if touching this path.
- Test coverage: `test_aa_orchestrator.cpp` covers stop-without-start and basic start/stop, but does not simulate mid-session teardown.

**`AndroidAutoOrchestrator::stop()` blocks the event loop for up to 2 seconds:**
- Files: `src/core/aa/AndroidAutoOrchestrator.cpp` lines 119-137
- Why fragile: `stop()` spins a nested `QEventLoop` to wait for the phone's shutdown acknowledgment. This works on the main thread but will deadlock if called from a non-event-loop context or if the phone doesn't respond and the 2s timeout fires while another operation is in progress.
- Safe modification: Only call `stop()` from the main Qt thread (currently the case). Do not add any signal handlers or slots that call `stop()` from worker threads.
- Test coverage: Not tested.

**`BluetoothManager` auto-connect — `needsFirstPairing_` never cleared on some paths:**
- Files: `src/core/services/BluetoothManager.cpp` lines 767-778
- Why fragile: If BlueZ toggles `Pairable` to false during a first-run pairing flow, the `onDevicePropertiesChanged` handler re-enables it via a QTimer. If the pairing completes before the timer fires, `needsFirstPairing_` might still be true and re-enable pairing mode unnecessarily.
- Safe modification: Verify `needsFirstPairing_` is cleared when a device is successfully paired (check `onPairingConfirmed`).
- Test coverage: `test_bluetooth_manager.cpp` exists but pairable-timeout interaction is unlikely to be tested.

**`EvdevTouchReader` grab/ungrab synchronization:**
- Files: `src/core/aa/EvdevTouchReader.cpp`, `src/core/aa/EvdevTouchReader.hpp`
- Why fragile: `EVIOCGRAB` must be toggled with AA connection state (grab on connect, ungrab on disconnect). The reader runs in a dedicated QThread. Connection state changes come from the main thread. The grab state is set via thread-safe signal/slot, but if the connection drops and reconnects rapidly, there is a potential window where touch events are routed incorrectly.
- Safe modification: Any change to grab/ungrab timing must account for the QThread-to-main-thread transition. Do not add synchronous grab/ungrab calls from the main thread.
- Test coverage: `test_input_device_scanner.cpp` covers device scanning; live evdev grab/ungrab is not unit-testable.

**`BluetoothDiscoveryService` falls back to `00:00:00:00:00:00` BSSID:**
- Files: `src/core/aa/BluetoothDiscoveryService.cpp` lines 389-392
- Why fragile: If the WiFi interface MAC cannot be read (interface not yet up, wrong name), the BSSID sent to the phone is `00:00:00:00:00:00`. Some phones use the BSSID to identify the AP for auto-connect. A null BSSID may cause the phone to fail WiFi handshake silently.
- Safe modification: Add a retry loop (interface may not be up at BT discovery time) or error early and refuse to send `WifiInfoResponse`.
- Test coverage: Not tested.

---

## Scaling Limits

**Single companion client — no multi-device support:**
- Current capacity: `CompanionListenerService` accepts exactly one client; additional connections are immediately rejected with `"already connected"`.
- Files: `src/core/services/CompanionListenerService.cpp` lines 191-198
- Limit: One phone as companion at a time.
- Scaling path: Not a practical concern — the head unit is a single-seat device. Document the constraint explicitly so future contributors don't add multi-device logic that conflicts.

**AA session handles one phone at a time:**
- Current capacity: `AndroidAutoOrchestrator` tears down any existing session before accepting a new TCP connection (lines 191-194). Only one AA session is possible.
- Files: `src/core/aa/AndroidAutoOrchestrator.cpp` lines 183-197
- Limit: Single active AA session.
- Scaling path: Not a practical concern for a car head unit. By design.

---

## Dependencies at Risk

**Qt version split (6.4 dev / 6.8 Pi):**
- Risk: Several Qt 6.5+ and 6.8+ features are conditionally compiled (`QTimeZone::UTC`, `DmaBufVideoBuffer`, `pw_stream_set_rate`). Each new Qt feature used on Pi must be guarded for dev-VM compatibility.
- Impact: Dev builds silently omit Pi-only features (DMA-BUF zero-copy video, VHT80 audio timing). Regressions that only manifest on Pi are harder to catch.
- Migration plan: Ubuntu 24.04 ships Qt 6.4 and will not bump to 6.8 without a PPA. Acceptable for now — document new `#if QT_VERSION` guards as they are added.

**`open-androidauto` submodule is a community proto repo:**
- Risk: `libs/open-androidauto/proto/` is a git submodule pointing to `https://github.com/mrmees/open-android-auto`. Any breaking proto change in that repo requires a coordinated submodule bump in this repo.
- Impact: Accidental submodule drift breaks the build. Proto changes need careful versioning.
- Migration plan: The submodule is pinned by commit hash; do not `git submodule update --remote` without reviewing the diff.

---

## Missing Critical Features

**No CI automation:**
- Problem: No CI pipeline (GitHub Actions, etc.) runs builds or tests automatically on push.
- Blocks: Regressions can land on `main` undetected. The 47-test suite only runs when a dev manually runs `ctest`.
- Note: Roadmap item "CI automation for builds and tests" is explicitly tracked in `docs/roadmap-current.md`.

**No multi-display or resolution abstraction:**
- Problem: Display width/height (`1024x600`) and touch range (`0-4095`) are configured via `YamlConfig` but the defaults and coordinate mapping assume the DFRobot 1024x600 display. Some paths (e.g. companion QR code scale `int scale = 8`) are hardcoded for a specific pixel size.
- Files: `src/core/services/CompanionListenerService.cpp` line 164, `src/core/aa/EvdevTouchReader.cpp`
- Blocks: Deployment to other Pi display hardware without YAML config changes and possible source edits.
- Note: Roadmap item "Multi-display / resolution support" is tracked.

**Display brightness control not implemented:**
- Problem: No `IDisplayService` interface or implementation exists. The gesture overlay brightness slider is wired to nothing.
- Files: `qml/components/GestureOverlay.qml` line 125
- Blocks: In-car brightness adjustment without SSH or config file edits.

**HFP call audio does not survive AA stream teardown:**
- Problem: If the AA session drops while a phone call is active, there is no fallback to keep audio routed through the HFP AG profile on the head unit.
- Files: `src/plugins/phone/PhonePlugin.cpp`, `src/core/services/BluetoothManager.cpp`
- Blocks: Reliable phone calls — currently tracked as "General HFP call audio handling" in `docs/roadmap-current.md`.

---

## Test Coverage Gaps

**AA session lifecycle (connect → active → disconnect) not tested:**
- What's not tested: The full `AndroidAutoOrchestrator` connection lifecycle with an actual (or mock) AA session. Tests only cover initial state, basic TCP listen start/stop, and no-op calls on disconnected state.
- Files: `tests/test_aa_orchestrator.cpp`
- Risk: Session teardown bugs, reconnect races, and watchdog interactions are completely untested.
- Priority: High

**Video decode pipeline not integration-tested:**
- What's not tested: `VideoDecoder` with actual H.264/H.265 data, codec switching, hardware fallback path, and frame delivery to a `QVideoSink`.
- Files: `tests/test_video_decode_queue.cpp`, `tests/test_video_frame_pool.cpp`
- Risk: Codec switch bugs, pixel format handling (YUVJ420P vs YUV420P), and DRM-PRIME path are only tested on-device.
- Priority: Medium

**EVIOCGRAB / touch routing not testable in CI:**
- What's not tested: Live evdev device open, EVIOCGRAB toggle, and multi-touch slot tracking.
- Files: `tests/test_input_device_scanner.cpp` (only covers device scanning, not touch reading)
- Risk: Touch misrouting after reconnect or grab/ungrab edge cases are only caught on Pi.
- Priority: Low (inherently hardware-dependent)

**PhonePlugin actual call control not tested:**
- What's not tested: BlueZ HFP AT command layer, dial(), hangup(), sendDTMF() over the air.
- Files: `src/plugins/phone/PhonePlugin.cpp`, no corresponding integration test
- Risk: These are stubs — there is nothing to test yet. But when implemented, no test harness exists.
- Priority: High (when implementing HFP call control)

---

*Concerns audit: 2026-03-01*
