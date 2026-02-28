# AA Troubleshooting Runbook

Living troubleshooting guide for OpenAuto Prodigy AA sessions. Covers tools, debug workflows, failure modes, and observations.

**Companion to:** `docs/skills/aa-troubleshooting/SKILL.md` (quick reference skill)
**Protocol reference:** `docs/aa-protocol-reference.md` (wire format, message IDs, channels)
**Phone-side logging:** `docs/aa-phone-side-debug.md` (logcat tags, dev settings, process architecture)
**Protocol cross-reference:** `../openauto-pro-community/docs/android-auto-protocol-cross-reference.md` (Sony HU + APK mapped together)

---

## Table of Contents

1. [Tool Inventory](#tool-inventory)
2. [Debug Workflows](#debug-workflows)
3. [Failure Mode Playbooks](#failure-mode-playbooks)
   - [Session Establishment Failures](#session-establishment-failures)
   - [Post-Handshake Stalls](#post-handshake-stalls)
   - [Video Issues](#video-issues)
   - [Audio Issues](#audio-issues)
   - [Touch Issues](#touch-issues)
   - [App Crash / Restart Issues](#app-crash--restart-issues)
4. [Protocol Logger Reference](#protocol-logger-reference)
5. [Phone-Side Debug Quick Reference](#phone-side-debug-quick-reference)
6. [Capture Data Index](#capture-data-index)
7. [Deployment Checklist](#deployment-checklist)
8. [Notes & Observations](#notes--observations)

---

## Tool Inventory

### Testing/reconnect.sh — Session Reset — BROKEN

> **WARNING:** This script has hardcoded BT MACs, unreliable ADB WiFi toggling, and unvalidated log format checks. Kept as reference for the reconnect **sequence**, but do not run as-is. See the [Manual Test Cycle](docs/skills/aa-troubleshooting/SKILL.md#manual-test-cycle-replaces-reconnectsh-for-now) in the skill for the current approach.

**Sequence (still valid conceptually):**
1. Pi BT disconnect + phone WiFi off (via ADB)
2. Kill app on Pi (graceful then SIGKILL)
3. Wait for clean state (configurable, default 10s)
4. Restart app on Pi (waits 8s for RFCOMM listener registration)
5. BT connect with A2DP UUID (retry loop: 6 attempts, 10s timeout each, 5s between)
6. Wait 20s for AA session, validate with VIDEO frame count

**Known issues:**
- `PHONE_MAC` is hardcoded — BT MACs randomize, must discover dynamically via `bluetoothctl devices Paired`
- `svc wifi disable` via ADB is unreliable for dropping the phone's AA connection
- VIDEO grep validation may not match new open-androidauto log format

### Testing/capture.sh — Protocol Capture — BROKEN

> **WARNING:** Depends on reconnect.sh. The log collection steps (3/4, 4/4) are still valid — it's the reconnect that needs fixing. See [Manual Log Capture](docs/skills/aa-troubleshooting/SKILL.md#manual-log-capture-replaces-capturesh-for-now) in the skill.

**Output in `Testing/captures/<name>/`:**
- `pi-protocol.log` — TSV protocol messages from ProtocolLogger
- `phone-logcat-raw.log` — Full phone logcat dump
- `phone-logcat.log` — Filtered for AA tags (CAR.*, GH.*, WIRELESS, PROJECTION, WPP)

### Testing/merge-logs.py — Timeline Merger

Merges Pi protocol log + phone logcat into a single chronological timeline.

**Usage:**
```bash
python3 Testing/merge-logs.py \
    Testing/captures/my-test/pi-protocol.log \
    Testing/captures/my-test/phone-logcat-raw.log \
    Testing/captures/my-test/timeline.md
```

**How it works:**
- Pi log: TSV with float seconds (from ProtocolLogger)
- Phone log: logcat timestamps (MM-DD HH:MM:SS.mmm)
- Alignment: finds `VERSION_REQUEST` on Pi side and `Socket connected` on phone side
- Clock offset calculated and applied to phone timestamps
- Output: markdown with `[PI]` / `[PHONE]` annotations, sorted by time

### Unit Tests (ctest)

44 C++ tests covering protocol, config, plugins, audio, display, touch.

```bash
cd build && ctest --output-on-failure
```

**Known:** 1 pre-existing failure in `test_tcp_transport` (unrelated to current work).

**Key test files for AA troubleshooting:**
- `tests/test_oaa_integration.cpp` — open-androidauto library integration
- `libs/open-androidauto/tests/test_protocol_logger.cpp` — Protocol capture logging (TSV + JSONL)
- `tests/test_video_channel_handler.cpp` — Video stream + flow control
- `tests/test_audio_channel_handler.cpp` — Audio stream handlers
- `tests/test_sensor_channel_handler.cpp` — Sensor (GPS, night mode)
- `tests/test_aa_orchestrator.cpp` — AA session lifecycle
- `tests/test_service_discovery_builder.cpp` — Capability advertisement
- `libs/open-androidauto/tests/test_control_channel.cpp` — Version/SSL/service discovery
- `libs/open-androidauto/tests/test_session_fsm.cpp` — Connection state machine
- `libs/open-androidauto/tests/test_messenger.cpp` — Message routing

---

## Debug Workflows

### Workflow: "Something broke, where do I start?"

1. **Is the app actually running?**
   ```bash
   ssh matt@192.168.1.149 'pgrep -f openauto-prodigy'
   ssh matt@192.168.1.149 'ps -o pid,lstart,cmd -p $(pgrep -f openauto-prodigy)'
   ```

2. **What's on screen right now?**
   ```bash
   ssh matt@192.168.1.149 'WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 grim /tmp/screenshot.png' && scp matt@192.168.1.149:/tmp/screenshot.png /tmp/
   ```

3. **Are you looking at current logs?**
   ```bash
   ssh matt@192.168.1.149 'ls -la /tmp/oap.log /tmp/oap-protocol.log'
   ssh matt@192.168.1.149 'tail -50 /tmp/oap.log'
   ```

4. **Check protocol log (if session attempted):**
   ```bash
   ssh matt@192.168.1.149 'cat /tmp/oap-protocol.log'
   ```

5. **Check phone logcat (AA-filtered):**
   ```bash
   ./platform-tools/adb logcat -d | grep -E 'CAR\.|GH\.|WIRELESS|PROJECTION|WPP' | tail -50
   ```

6. **If needed, do a full capture** — see [Manual Log Capture](docs/skills/aa-troubleshooting/SKILL.md#manual-log-capture-replaces-capturesh-for-now) in the skill.

### Workflow: "I changed code, now test it"

1. **Build locally (sanity check):**
   ```bash
   cd build && cmake --build . -j$(nproc) && ctest --output-on-failure
   ```

2. **Deploy to Pi (source only, never binaries):**
   ```bash
   rsync -avz --relative src/ qml/ libs/open-androidauto/ CMakeLists.txt \
       matt@192.168.1.149:/home/matt/openauto-prodigy/
   ```
   **WARNING:** Never rsync `libs/open-androidauto/build/` — x86 .so files will overwrite ARM64 builds.

3. **Build on Pi:**
   ```bash
   ssh matt@192.168.1.149 'cd /home/matt/openauto-prodigy/build && cmake --build . -j3'
   ```

4. **If CMake doesn't detect changes:** Touch changed files with future timestamps:
   ```bash
   ssh matt@192.168.1.149 'touch -t 203001010000 /home/matt/openauto-prodigy/src/path/to/changed/file.cpp'
   ```

5. **Restart the app:**
   ```bash
   ssh matt@192.168.1.152 '~/openauto-prodigy/restart.sh'
   # If process is stuck, use --force-kill
   ```

6. **Reconnect phone** — see [Manual Test Cycle](docs/skills/aa-troubleshooting/SKILL.md#manual-test-cycle-replaces-reconnectsh-for-now) in the skill (steps 1, 7-10).

### Workflow: "I need a screenshot of the Pi display"

```bash
ssh matt@192.168.1.149 'WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/run/user/1000 grim /tmp/screenshot.png'
scp matt@192.168.1.149:/tmp/screenshot.png /tmp/pi-screenshot.png
```

---

## Failure Mode Playbooks

> **Step 0 for ALL failure modes:** Confirm the app is running and visible first. See [Step 0](docs/skills/aa-troubleshooting/SKILL.md#step-0-is-the-app-actually-running-and-visible-always-do-this-first) in the skill.

### Session Establishment Failures

**Symptom:** Phone never connects to AA. No protocol log entries. App running but idle.

**Check BT pairing:**
```bash
# Find paired phone (don't hardcode MAC — BT MACs randomize)
ssh matt@192.168.1.149 'bluetoothctl devices Paired'

# Check specific device info
ssh matt@192.168.1.149 'bluetoothctl info <PHONE_MAC>'
```
- Look for `Paired: yes`, `Connected: yes`
- If not paired: `bluetoothctl pair <PHONE_MAC>`
- If "key-missing" error: phone BT MAC changed (randomization, factory reset). Remove old pairing and re-pair.

**Check WiFi AP:**
```bash
ssh matt@192.168.1.149 'ip addr show wlan0'
```
- Must show `10.0.0.1` — if missing, `wlan0` IP didn't survive reboot
- Check hostapd: `ssh matt@192.168.1.149 'systemctl status hostapd'`
- WiFi SSID/password in app config must match `hostapd.conf` exactly

**Check RFCOMM listener:**
```bash
ssh matt@192.168.1.149 'ss -tlnp | grep openauto'
```
- App needs ~8s after launch to register RFCOMM listener
- If no listener: check app log for BT initialization errors

**Check phone side:**
- Phone Settings → Connected devices → Connection preferences → Android Auto
- Should show "OpenAutoProdigy — Wireless Android Auto available"
- If not listed: phone hasn't discovered the Pi via BT. Check BT pairing.

### Post-Handshake Stalls

**Symptom:** Protocol log shows VERSION → TLS → AUTH → SERVICE_DISCOVERY, channels open, but phone won't start video projection.

This was a **known blocker under the old aasdk stack** (as of 2026-02-23). The phone entered `STATE_WAITING_FOR_USER_AUTHORIZATION` and never progressed. Status under open-androidauto is unknown — needs revalidation.

**What to check in protocol log:**
1. All 9 channels should open (3,4,5,6,1,2,8,14,7)
2. `AV_SETUP_REQUEST` should arrive for video/audio channels
3. `AV_SETUP_RESPONSE` should be sent for each
4. `SENSOR_START_REQUEST` × 3 should arrive and be responded to
5. `INPUT_BINDING_REQUEST` should arrive
6. Pings should be flowing (session alive)

**What to check in phone logcat:**
```bash
./platform-tools/adb logcat -d | grep -E 'AUTHORIZATION|WirelessStartup|FirstActivity|consent'
```
- `STATE_WAITING_FOR_USER_AUTHORIZATION` — phone waiting for user to accept
- `AUTHORIZATION_COMPLETE` — fires but setup may tear down anyway
- `sendSensorRequest timed-out` — likely symptom, not cause
- No consent dialog appearing = the phone's AA UI never fully launches

**Known observations (from aasdk era — revalidate):**
- Phone "Connected vehicles" page shows: Accepted=None, Rejected=None
- `WirelessStartupActivity` and `FirstActivityImpl` launch then immediately self-destruct
- Phone shows "Connecting to Android Auto" notification indefinitely

### Video Issues

**Symptom:** AA session active (pings flowing) but no video on display.

**Black screen — check decoder pixel format:**
- Some phones output `AV_PIX_FMT_YUVJ420P` (fmt=12) instead of `AV_PIX_FMT_YUV420P` (fmt=0)
- Must accept both formats or frames are silently discarded
- Moto G Play 2024 → YUVJ420P; Samsung S25 Ultra → YUV420P

**No video frames at all:**
1. Check `VIDEO_FOCUS_INDICATION(FOCUSED)` was sent after `AV_SETUP_RESPONSE`
2. Check `max_unacked` in video flow control (should be ≥10)
3. Check protocol log for `AV_MEDIA_INDICATION` on channel 3 (video)
4. Verify video resolution config: 720p is default, 1080p works but may need more CPU

**Choppy/stuttering video:**
- FFmpeg `thread_count` must be 1 for real-time AA decode — `thread_count=2` causes internal buffering that breaks small P-frame phones
- Check Pi CPU usage: `ssh matt@192.168.1.149 'top -bn1 | head -5'`

**Video focus gotcha:**
- Phone aggressively re-requests `VIDEO_FOCUS_INDICATION(FOCUSED)`
- Sending `UNFOCUSED` is treated as an exit signal — don't suppress focus requests

### Audio Issues

**Symptom:** AA active with video but no sound, or choppy audio.

**Check PipeWire on Pi:**
```bash
ssh matt@192.168.1.149 'pw-cli ls Node'        # list audio nodes
ssh matt@192.168.1.149 'wpctl status'            # WirePlumber status
```

**Check audio device config:**
- App config must specify valid PipeWire device
- "Default" device label shows first registry device, not PipeWire's actual default
- Audio device switching requires app restart

**Choppy audio:**
- PipeWire `d.chunk->size` must report actual bytes read, not `maxSize` with zero-fill
- Check ring buffer utilization in audio service

**Audio channels:**
| Channel | Content | Sample Rate |
|---------|---------|-------------|
| 4 | Media (music) | 48kHz stereo |
| 5 | Speech (TTS nav) | 48kHz mono (was 16kHz, upgraded in probe-2) |
| 6 | System sounds | 16kHz mono |
| 7 | Microphone input | ≥16kHz mono (HU → phone) |

### Touch Issues

**Symptom:** Touch doesn't work during AA, or touches land in wrong place.

**EVIOCGRAB state:**
- During AA: evdev device should be grabbed (touch routed to AA, not Wayland)
- When AA disconnects: ungrab must happen (return touch to Wayland/libinput)
- Permanent grab = launcher UI unresponsive
- Check app log for EVIOCGRAB/UNGRAB messages around AA connect/disconnect

**Touch misalignment:**
- `touch_screen_config` MUST be set to video resolution (1280x720), NOT display resolution (1024x600)
- Touch coordinates are sent in video resolution space
- Evdev range is 0-4095, mapped to 1280x720

**Touch device not found:**
- App scans for `INPUT_PROP_DIRECT` devices on startup
- DFRobot USB Multi Touch: vendor 3343:5710, 10 points, MT Type B
- Check: `ssh matt@192.168.1.149 'cat /proc/bus/input/devices'`

**Sidebar touch during AA:**
- QML MouseAreas don't work during EVIOCGRAB — visual only
- Sidebar touch handled via evdev hit zones in `EvdevTouchReader`
- Sidebar config changes require app restart (margins locked at AA session start)

### App Crash / Restart Issues

**Symptom:** App crashes or won't restart cleanly.

**Port bind failure after restart:**
- TCP sockets may not set `SOCK_CLOEXEC` — forked processes (e.g. QProcess::startDetached for restart) inherit the acceptor FD, preventing port rebind
- Fix: `fcntl(fd, F_SETFD, FD_CLOEXEC)` after socket open
- Or: `SO_REUSEADDR` must be set before bind (not after)

**Plugin view crash:**
- Calling `PluginModel.setActivePlugin("")` from within a plugin's own QML view crashes
- The click handler is still on the stack when the view is destroyed
- Fix: wrap in `Qt.callLater(function() { ... })`

**Phone won't reconnect after restart:**
- Phone doesn't cleanly reconnect after app restart
- User must manually cycle BT/WiFi on phone
- TODO: Find reliable method to drop/re-establish AA connection on phone side

**`pkill` silently fails:**
- Process names >15 chars truncated in procfs
- Must use `pkill -f 'build/src/openauto-prodigy'` (full command match)

---

## Protocol Logger Reference

The ProtocolLogger hooks into the Messenger layer and logs every protobuf message exchanged.

**Output location:** `/tmp/oap-protocol.log`

**Format:** TSV (tab-separated values)
```
TIME    SOURCE          CHANNEL    MESSAGE              PAYLOAD
0.000   HU->Phone       0          VERSION_REQUEST      major=1 minor=7
0.015   Phone->HU       0          VERSION_RESPONSE     major=1 minor=7 status=MATCH
...
```

**Fields:**
- `TIME` — seconds since session start (float)
- `SOURCE` — `HU->Phone` or `Phone->HU`
- `CHANNEL` — channel ID (0-14)
- `MESSAGE` — human-readable message name
- `PAYLOAD` — protobuf summary (truncated to 500 chars for AV data)

**Key messages to look for:**
- `VERSION_REQUEST/RESPONSE` — protocol version negotiation
- `SSL_HANDSHAKE` — TLS setup (multiple rounds)
- `AUTH_COMPLETE` — authentication done
- `SERVICE_DISCOVERY_REQUEST/RESPONSE` — capability exchange
- `CHANNEL_OPEN_REQUEST/RESPONSE` — per-channel setup
- `AV_SETUP_REQUEST/RESPONSE` — audio/video stream config
- `VIDEO_FOCUS_INDICATION` — video projection state
- `PING_REQUEST/RESPONSE` — keepalive (should be regular)
- `SHUTDOWN_REQUEST/RESPONSE` — graceful disconnect

---

## Phone-Side Debug Quick Reference

Full details in `docs/aa-phone-side-debug.md`. Key points:

**Enable AA Developer Settings:**
1. Phone Settings → Apps → Android Auto
2. Tap version number 10 times
3. Three-dot menu → Developer Settings appears

**Useful dev toggles:**
- Force debug logging (verbose protocol logs)
- Save video/audio/mic capture to disk
- Audio codec selector (PCM vs AAC-LC)

**Key logcat tags:**

| Tag | What it shows |
|-----|---------------|
| `CAR.GAL.LITE` | Core protocol (GAL) |
| `CAR.BT.LITE` | Bluetooth state |
| `GH.WPP.CONN` / `GH.WPP.TCP` | WiFi Projection Protocol |
| `GH.WIRELESS.SETUP` | Wireless setup state machine |
| `GH.ConnLoggerV2` | Session event timeline |
| `GH.CarClientManager` | Car client lifecycle |

**Phone AA processes:**
- `:projection` — main projection UI
- `:car` — protocol engine
- `:shared` — shared services
- `:watchdog` — health monitoring
- `:provider` — content provider

**Filtering:**
```bash
# Live AA-only logcat
./platform-tools/adb logcat | grep -E 'CAR\.|GH\.|WIRELESS|PROJECTION|WPP'

# Dump and filter
./platform-tools/adb logcat -d | grep -E 'CAR\.|GH\.|WIRELESS|PROJECTION|WPP'

# Clear before test
./platform-tools/adb logcat -c
```

---

## Capture Data Index

**HISTORICAL — captured under old aasdk stack.** Protocol behavior at the wire level should still be valid, but Pi-side log formats and message names may differ from the new open-androidauto stack.

All captures in `Testing/captures/`. Each directory contains:
- `pi-protocol.log` — TSV protocol messages
- `phone-logcat-raw.log` — Full logcat
- `phone-logcat.log` — AA-filtered logcat
- `findings.md` (probes only) — Observations and conclusions
- `timeline.md` (if merged) — Chronological merged view

| Capture | Date | What Was Tested | Outcome |
|---------|------|-----------------|---------|
| `baseline/` | 2026-02-23 | Normal AA session reference | 49 Pi msgs, full session lifecycle captured |
| `probe-1-version-bump/` | 2026-02-23 | Protocol v1.7 negotiation | Phone responds v1.7, stores HU version |
| `probe-2-48khz-speech/` | 2026-02-23 | 48kHz speech audio | Works — frame size doubles, phone captures at 48kHz |
| `probe-3-night-mode/` | 2026-02-23 | Night mode sensor push | Phone correctly reads sensor event |
| `probe-4-palette-v2/` | 2026-02-23 | Material You (theme v2) | **BLOCKED** — field number undiscovered |
| `probe-5-8-hideclock-sensors/` | 2026-02-23 | hide_clock + COMPASS/ACCEL/GYRO | hide_clock dead; phone requests COMPASS+ACCEL, ignores GYRO |
| `probe-6-alarm-audio/` | 2026-02-23 | ALARM audio channel | Can't test — channel ID undocumented |
| `probe-7-1080p/` | 2026-02-23 | 1920x1080 video | Works — phone renders full res, triggers xlrg layout |

---

## Deployment Checklist

Before testing changes on Pi:

- [ ] Local build passes (`cmake --build . -j$(nproc)`)
- [ ] Unit tests pass (`ctest --output-on-failure`) — expect 1 known failure in `test_tcp_transport`
- [ ] Rsync **source files only** (never `build/` or `.so` files from x86)
- [ ] Pi build succeeds (`cmake --build . -j3` on Pi)
- [ ] Old app instance killed before launching new one
- [ ] 8s wait after launch before attempting BT connect (RFCOMM registration time)
- [ ] WiFi AP up (`ip addr show wlan0` shows 10.0.0.1)
- [ ] Phone BT paired and discoverable

---

## Notes & Observations

*This section is for observations, hunches, and things noticed during testing that aren't captured elsewhere. Add freely.*

### Authorization State Issue (2026-02-23, aasdk era — needs revalidation)

Under the old aasdk stack, the phone completed the entire handshake (VERSION → TLS → AUTH → SERVICE_DISCOVERY → all channels open → AV setup → sensors → input binding → WiFi credentials → pings flowing) but entered `STATE_WAITING_FOR_USER_AUTHORIZATION` and never progressed to video projection.

**What we observed (aasdk stack):**
- `AUTHORIZATION_COMPLETE` fires in logcat, but BT FSM enters auth-wait state AFTER
- `WirelessStartupActivity` and `FirstActivityImpl` launch then immediately self-destruct
- No consent dialog ever appears for user to tap Accept
- Phone "Connected vehicles" shows: Accepted=None, Rejected=None
- `sendSensorRequest timed-out` × 3 in phone logcat (likely symptom, not cause)

**Protocol fixes applied during aasdk Pi validation:**
- MessageType: ch0 → Specific(0x00), non-zero → Control(0x04)
- AASession: auto-respond audio/nav focus, intercept CHANNEL_OPEN_REQUEST on non-zero channels
- VideoChannelHandler: VIDEO_FOCUS_INDICATION(FOCUSED) after AV_SETUP_RESPONSE, max_unacked=10
- TCPTransport: hex dump logging for writes

**Open questions (carry forward to open-androidauto testing):**
- Is this a cert/identity issue? Phone may not trust the HU certificate.
- Is the phone expecting something in SERVICE_DISCOVERY_RESPONSE that we're not providing?
- Could this be an Android version-specific behavior? (Test phone: Moto G Play 2024, Android 14)
- Does DHU (Desktop Head Unit, `extras/google/auto/desktop-head-unit`, USB-only) exhibit the same state machine? Worth comparing.

---

*Last updated: 2026-02-23*
