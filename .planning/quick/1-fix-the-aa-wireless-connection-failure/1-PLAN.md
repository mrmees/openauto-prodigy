---
phase: quick
plan: 1
type: execute
wave: 1
depends_on: []
files_modified:
  - src/core/aa/AndroidAutoOrchestrator.cpp
  - src/core/aa/BluetoothDiscoveryService.cpp
  - libs/prodigy-oaa-protocol/src/Session/AASession.cpp
  - libs/prodigy-oaa-protocol/src/Transport/TCPTransport.cpp
autonomous: false
requirements: []
must_haves:
  truths:
    - "Phone connects wirelessly to the Pi and establishes an AA session"
    - "Connection failure root cause is identified and documented"
    - "Fix is verified on Pi hardware with the S25 Ultra"
  artifacts: []
  key_links:
    - from: "BluetoothDiscoveryService"
      to: "AndroidAutoOrchestrator::onNewConnection"
      via: "BT RFCOMM handshake -> WiFi AP join -> TCP connect"
      pattern: "phoneWillConnect|onNewConnection"
---

<objective>
Diagnose and fix the AA wireless connection failure flagged as URGENT in STATE.md.

Purpose: AA connectivity is the core feature -- if it's broken, nothing else matters. This must be fixed before any further v0.5.3 widget work.
Output: Working wireless AA connection on the Pi with the S25 Ultra.
</objective>

<execution_context>
@/home/matt/.claude/get-shit-done/workflows/execute-plan.md
@/home/matt/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@src/core/aa/AndroidAutoOrchestrator.cpp
@src/core/aa/AndroidAutoOrchestrator.hpp
@src/core/aa/BluetoothDiscoveryService.cpp
@src/core/aa/BluetoothDiscoveryService.hpp
@libs/prodigy-oaa-protocol/src/Transport/TCPTransport.cpp
@libs/prodigy-oaa-protocol/src/Session/AASession.cpp
@docs/debugging-notes.md
@docs/testing-reconnect.md
</context>

<tasks>

<task type="checkpoint:decision" gate="blocking">
  <name>Task 1: Gather failure symptoms from Matt</name>
  <decision>What exactly is failing with the AA wireless connection?</decision>
  <context>
The STATE.md flags "Phase 05.1 inserted: Fix AA wireless connection failure (URGENT)" but there are no logs, error messages, or symptom descriptions in the codebase. The connection flow has multiple stages where failure could occur:

1. **BT RFCOMM** -- phone connects to Pi's SDP record, RFCOMM channel opens
2. **WiFi handshake** -- WifiStartRequest sent, phone asks for credentials, phone joins AP
3. **TCP connect** -- phone opens TCP to Pi's port 5288, onNewConnection fires
4. **Protocol negotiation** -- version exchange, TLS handshake, service discovery
5. **Session active** -- channels open, video/audio streaming

To write a targeted fix, I need to know:
- At which stage does it fail? (phone never connects BT? connects BT but not WiFi? TCP connects but protocol fails?)
- What do the Pi logs show? (`journalctl -u openauto-prodigy.service --no-pager -n 100`)
- Is it a regression from the v0.5.3 widget work, or was it already broken?
- Does it fail every time, or intermittently?
- Any phone-side error messages (AA app showing "can't connect")?
  </context>
  <options>
    <option id="bt-fail">
      <name>BT/RFCOMM failure</name>
      <pros>Narrow scope: SDP registration, RFCOMM listener, or BT pairing issue</pros>
      <cons>May need BlueZ/systemd debugging beyond app code</cons>
    </option>
    <option id="wifi-fail">
      <name>WiFi handshake failure</name>
      <pros>Narrow scope: WifiStartRequest/credentials exchange in BluetoothDiscoveryService</pros>
      <cons>Could be hostapd config or network issue</cons>
    </option>
    <option id="tcp-fail">
      <name>TCP connection or protocol failure</name>
      <pros>Narrow scope: TCPTransport, AASession, or handler wiring</pros>
      <cons>May need protocol capture analysis</cons>
    </option>
    <option id="unknown">
      <name>Not sure / need to investigate</name>
      <pros>Will SSH to Pi, pull logs, and diagnose from scratch</pros>
      <cons>Takes longer but catches the actual problem</cons>
    </option>
  </options>
  <resume-signal>Describe the failure: what you see, when it happens, any logs. Or select one of the options above.</resume-signal>
</task>

<task type="auto">
  <name>Task 2: Diagnose and fix the connection failure</name>
  <files>Depends on Task 1 outcome -- likely one or more of:
    src/core/aa/AndroidAutoOrchestrator.cpp
    src/core/aa/BluetoothDiscoveryService.cpp
    libs/prodigy-oaa-protocol/src/Session/AASession.cpp
    libs/prodigy-oaa-protocol/src/Transport/TCPTransport.cpp
  </files>
  <action>
Based on Matt's symptom report from Task 1:

1. SSH to Pi (`ssh matt@192.168.1.152`) and pull recent logs:
   - `journalctl -u openauto-prodigy.service --no-pager -n 200`
   - Check for error patterns: "Failed to listen", "sdp_connect failed", "No usable IPv4", socket errors
   - Check BlueZ status: `systemctl status bluetooth`, `bluetoothctl show`
   - Check hostapd: `systemctl status hostapd`, `hostapd_cli status`
   - Check network: `ip addr show wlan0`, `ss -tlnp | grep 5288`

2. Identify the failure point in the connection chain:
   - BT: SDP registered? RFCOMM listening? Phone sees the service?
   - WiFi: WifiStartRequest sent? Phone received credentials? Phone joined AP?
   - TCP: Port 5288 listening? Phone connected? Socket tuning OK?
   - Protocol: Version exchange? TLS? Service discovery?

3. Implement the fix based on diagnosis. Common failure modes:
   - SDP registration race (retry logic exists but may need adjustment)
   - IP address detection failing (wlan0 vs ap0 vs fallback)
   - Port already in use from previous session (SO_REUSEADDR, CLOEXEC)
   - Protocol regression from code changes
   - WiFi AP not started (hostapd down)
   - BT adapter blocked (rfkill)

4. Build and test:
   - Cross-compile: `cd /home/matt/claude/personal/openautopro/openauto-prodigy && ./cross-build.sh`
   - Deploy: `rsync -av build-pi/src/openauto-prodigy matt@192.168.1.152:~/openauto-prodigy/build/src/`
   - Also push QML if changed: `cd /home/matt/claude/personal/openautopro/openauto-prodigy && git push && ssh matt@192.168.1.152 'cd ~/openauto-prodigy && git pull'`
   - Restart: `ssh matt@192.168.1.152 'sudo systemctl restart openauto-prodigy.service'`
  </action>
  <verify>
    <automated>ssh matt@192.168.1.152 'journalctl -u openauto-prodigy.service --no-pager -n 50 | grep -E "Android Auto connected|TCP listener started|SDP service registered"'</automated>
  </verify>
  <done>The specific connection failure is identified, fixed, and the app starts up with BT discovery + TCP listener running. Pi logs show healthy startup.</done>
</task>

<task type="checkpoint:human-verify" gate="blocking">
  <what-built>Fix for the AA wireless connection failure, deployed to Pi.</what-built>
  <how-to-verify>
    1. Open Android Auto on the S25 Ultra
    2. Verify the phone discovers the Pi via Bluetooth
    3. Confirm the phone connects to the Pi's WiFi AP
    4. Confirm the AA projection appears on the Pi display
    5. Test touch input works (tap something on the AA screen)
    6. If it fails, share the output of: `journalctl -u openauto-prodigy.service --no-pager -n 100`
  </how-to-verify>
  <resume-signal>Type "approved" if AA connects and works, or describe what's still broken.</resume-signal>
</task>

</tasks>

<verification>
- AA wireless connection succeeds on Pi with S25 Ultra
- No regression in existing functionality (video, touch, audio)
</verification>

<success_criteria>
Phone connects to Pi wirelessly and AA session is fully active (video + touch + audio).
</success_criteria>

<output>
After completion, create `.planning/quick/1-fix-the-aa-wireless-connection-failure/1-SUMMARY.md`
</output>
