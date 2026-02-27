# Milestone 1: Foundation (Feb 16-18, 2026)

## What Was Built

### Protocol Audit & Fixes (Feb 16-17)

Audited the openauto-prodigy source against the Android Auto protocol reference and identified 15 issues across 4 priority tiers. Completed fixes:

- **hostapd switched from 2.4GHz to 5GHz** — CYW43455 combo chip on Pi 4/5 shares antenna for WiFi and BT. Running hostapd on 2.4GHz caused severe BT coexistence interference; phones couldn't maintain both BT RFCOMM and WiFi simultaneously. Changed to channel 36 (non-DFS), added `ieee80211ac=1`, `wmm_enabled=1`. Country code (`country_code=US`, `ieee80211d=1`) required for 5GHz.

- **RFCOMM handshake rewritten to match AA wireless protocol** — The most critical code fix. Previous code used wrong message IDs and proto type names. Correct flow:
  1. HU -> Phone: `WifiStartRequest` (msgId=1) with IP + port
  2. Phone -> HU: `WifiInfoRequest` (msgId=2) — empty, requesting credentials
  3. HU -> Phone: `WifiInfoResponse` (msgId=3) with SSID, key, security_mode, access_point_type
  4. Phone -> HU: `WifiStartResponse` (msgId=6) — acknowledgement
  5. Phone -> HU: `WifiConnectionStatus` (msgId=7) — WiFi connected
  Also added handlers for optional `WifiVersionRequest`/`WifiVersionResponse` (msgId=4/5).

- **TCP port 5288 verified consistent** across code and docs.

- **VideoFocusRequest response implemented** — Phone sends `VideoFocusRequest`; HU must respond with `VideoFocusIndication` containing `FOCUSED` mode or phone may pause/stop video frames.

- **720p video resolution offered** — Changed from 480p-only (800x480) to 720p (1280x720) matching the 1024x600 target display more closely. Phone negotiates best fit from offered resolutions.

- **AnnexB start code double-prepend investigated and fixed** — Confirmed aasdk already delivers NALUs with start codes. Removed the redundant prepend in VideoDecoder.cpp.

- **Channel count corrected in docs** — Updated from "8 AA channels" to "10 AA channels": CONTROL=0, INPUT=1, SENSOR=2, VIDEO=3, MEDIA_AUDIO=4, SPEECH_AUDIO=5, SYSTEM_AUDIO=6, AV_INPUT=7, BLUETOOTH=8, WIFI=14.

- **SPS/PPS handling** — Discovered that SPS/PPS arrives via `AV_MEDIA_INDICATION` (msg ID 0x0001) and must be forwarded to the decoder.

Items identified but deferred to later milestones:
- BT adapter MAC still hardcoded to `00:00:00:00:00:00` (needs `QBluetoothLocalDevice`)
- `max_unacked=1` for video (phone waits for ACK per frame — should be ~10)
- Audio focus always responds with GAIN regardless of request type
- Sensor channel advertises NIGHT_DATA/DRIVING_STATUS but never sends data
- Input keycodes include useless KEYCODE_NONE (0), missing KEYCODE_MICROPHONE_1 (84)
- WiFi service stub logs requests without responding
- BT channel missing `supported_pairing_methods`
- BLE advertisement for auto-reconnect (nice-to-have)

### Performance Optimization Design & Implementation (Feb 17)

Designed and implemented a data-driven performance optimization pipeline for AA video streaming and touch input.

**Bottlenecks identified by code review:**
- Per-frame `QVideoFrame` allocation (30x/sec heap allocations)
- Line-by-line YUV plane copy (~41 MB/s in small memcpy chunks at 720p)
- Two deep copies of H.264 data (QByteArray copy on ASIO thread + Qt::QueuedConnection copy)
- Video decode blocking main thread (~150ms/sec at 30fps)
- Per-finger protobuf messages for touch (no batching)

**Implemented optimizations (Tasks 1-8):**

1. **PerfStats utility** — Zero-allocation rolling stats accumulator for timing instrumentation. `steady_clock::now()` at each checkpoint, rolling min/max/sum/count, logged every 5 seconds.

2. **Video pipeline instrumentation** — Timing at 5 checkpoints: enqueue (ASIO thread), decode start, decode done, copy done, display. Timestamps passed via signal from VideoService to VideoDecoder.

3. **Touch pipeline instrumentation** — Timing for QML-to-ASIO strand dispatch and protobuf send. Logged as events/sec with latency breakdown.

4. **Video frame copy elimination** — Cached `QVideoFrameFormat` (created once, reused). Double-buffered `QVideoFrame` (pre-allocate two, alternate). Stride-aware bulk `memcpy` per plane when FFmpeg linesize matches QVideoFrame bytesPerLine (common case), with line-by-line fallback.

5. **Decode moved off main thread** — New `QThread`-based decode worker. H.264 data pushed to mutex-protected queue. Worker runs parse/decode/copy. Only `setVideoFrame()` crosses back to main thread via `QMetaObject::invokeMethod(Qt::QueuedConnection)`. Signal connection changed to `Qt::DirectConnection` (ASIO thread -> worker queue is thread-safe).

6. **Touch event batching** — New `sendBatchTouchEvent(QVariantList points, int action)` sends all touch points from one QML callback as a single `InputEventIndication` protobuf with multiple `touch_location` entries. One `strand_.dispatch()` and one `SendPromise` per callback instead of per finger.

7. **Configurable video FPS** — New INI setting `[Video] fps=60` (default 60, valid 30 or 60). AA protocol supports both. Test added.

**Performance targets defined:**

| Metric | Target |
|--------|--------|
| Video frame latency (receive to display) | < 10ms p99 |
| Touch latency (QML to protobuf sent) | < 5ms p99 |
| Main thread blocked by decode | < 20ms/sec |
| Dropped frames | 0 at configured FPS |

### Architecture & UX Design (Feb 18)

Produced a comprehensive architecture design document defining the long-term structure of the project.

**Plugin architecture designed:**
- Two-tier plugins: Full (C++ .so + QML) for hardware/protocol features, QML-only for UI extensions
- `IPlugin` interface with lifecycle hooks: initialize, shutdown, QML component, icon, settings component, required services, fullscreen flag
- `IHostContext` service registry: AudioService, BluetoothService, ConfigService, ThemeService, DisplayService, EventBus
- Plugin manifest in YAML (`plugin.yaml`) with settings schema, requirements, nav strip config
- Lifecycle: Discovered -> Validated -> Loaded -> Initialized -> Running -> Shutdown (no hot-loading in v1)
- Failure isolation: `initialize()` returns false -> plugin disabled, core continues. QML errors -> placeholder shown.
- v1 constraint: plugins consume services only, do not provide services to other plugins

**Hardware profile system designed:**
- YAML files in `~/.openauto/hardware-profiles/` define platform-specific config
- Strategy classes (`IWiFiStrategy`, `IAudioBackendStrategy`) encapsulate behavioral differences
- New YAML file = new platform support (if using same tools). New strategy class = new tool support.
- Ships with `rpi4.yaml` as default

**Theme system designed:**
- Theme directories with `theme.yaml`, `colors.yaml`, optional fonts/icons
- Parent theme inheritance
- Colors exposed to plugins via `IThemeService` QML properties
- Day/night mode support

**Audio architecture designed:**
- PipeWire-based with priority routing (phone > nav > media > BT > local > system)
- `IAudioService` is a thin helper — PipeWire owns all mixing, routing, hardware output
- Plugins create PipeWire streams, write PCM directly, request/release audio focus

**Configuration system:**
- YAML-based, migrating from INI
- Single writer rule: Qt main app owns all config files
- Web config server sends change requests via Unix socket IPC
- Plugin-scoped config keyed by plugin ID

**UI layout:**
- Status bar (5-8% height), main content area (75-80%), nav strip (12-15%)
- AA takes fullscreen; 3-finger tap reveals translucent overlay with volume/brightness/home
- Adaptive to any resolution (no hardcoded pixel values)

**Settings architecture:**
- On-screen quick settings (large touch targets) auto-generated from plugin YAML schemas
- Web configuration panel at `http://10.0.0.1:8080` (Flask or similar)

### Architecture Migration Plan (Feb 18)

Defined a 37-task implementation plan across 12 phases (A through L) for migrating from monolithic to plugin-based architecture.

**Phase structure:**

| Phase | Description |
|-------|-------------|
| A (Tasks 1-3) | YAML config migration — yaml-cpp, YamlConfig with deep merge, INI fallback |
| B (Tasks 4-7.5) | Service interfaces + plugin boundary + HFP stack spike |
| C (Tasks 8-10) | Plugin manager + discovery (manifest parsing, lifecycle management) |
| D (Tasks 11-13) | QML shell refactor (Shell.qml, status bar, nav strip, content area) |
| D.5 (Tasks 14-15) | Plugin context lifecycle + AA bootstrap extraction |
| E (Tasks 16-18) | AA static plugin wrapping |
| F (Tasks 19-21) | Theme system implementation |
| G (Tasks 22-24) | Audio pipeline (PipeWire integration) |
| H (Tasks 25-27) | BT Audio plugin (A2DP sink) |
| I (Tasks 28-30) | Phone (HFP) plugin |
| J (Tasks 31-33) | Web config panel |
| K (Tasks 34-35) | 3-finger gesture overlay |
| L (Tasks 36-37) | Install script + polish |

**Critical path:** A -> B -> D + C -> D.5 -> E (AA must keep working throughout)

**Key migration decisions:**
- YAML config uses deep merge: mappings recurse, sequences and scalars override, missing keys preserve defaults
- No `pluginConfigs_` QMap — all plugin config lives under `root_["plugin_config"][pluginId][key]` (single source of truth, no split-brain)
- INI-to-YAML auto-migration on first boot
- Service interfaces are pure abstract (header-only, no implementation) with documented thread affinity, ownership, and error handling contracts
- `IPlugin::wantsFullscreen()` drives shell behavior (not hardcoded AA check)
- ConfigService wraps YamlConfig with plugin-scoped access
- HFP stack evaluated via spike (ofono vs BlueZ native) before committing to Phone plugin implementation

### Protocol Exploration Tooling (Feb 16-18 planning, executed later)

Designed and built protocol exploration infrastructure for systematic AA protocol investigation:

- **ProtocolLogger** — Singleton class hooked into aasdk's Messenger send/receive paths. Writes TSV output (TIME, DIR, CHANNEL, MESSAGE, SIZE, PAYLOAD_PREVIEW) to `/tmp/oap-protocol.log`. Static `channelName()` and `messageName()` lookup methods map IDs to human-readable names. Lifecycle tied to AA session (open on TCP connect, close on disconnect).

- **capture.sh** — Wrapper script around reconnect.sh that collects Pi-side protocol log + phone-side logcat. Colored output, ADB integration, interactive capture with user prompt.

- **merge-logs.py** — Aligns Pi + phone logs into a unified timeline. Pi anchor: first "HU->Phone VERSION_REQUEST". Phone anchor: first "Socket connected". Normalizes timestamps by offset. 7 pytest tests.

- **Protocol probes executed:**
  - Probe 1: Version bump v1.1 -> v1.7 — phone always responds v1.7 regardless; larger ServiceDiscoveryRequest. Kept v1.7.
  - Probe 2: 48kHz speech audio — success, frame size doubled, phone captures at 48kHz. Kept.
  - Probe 3: Night mode sensor — works, phone reads SENSOR_EVENT_INDICATION. Discovered phone expects palette version 2 but we report 0.
  - Probe 4: Palette v2 — BLOCKED, no theme_version field in any known proto definitions. Needs raw proto capture from production HU.
  - Probe 5: hide_clock field 12 — DEAD on modern AA, no effect. CarDisplayUiFeatures is the modern mechanism.
  - Probe 6: 1080p video — WORKS, phone renders at 1920x984 (140 DPI). CPU-intensive on Pi 4 software decode.
  - Probe 7: Extra sensors — phone requests COMPASS and ACCELEROMETER when advertised, ignores GYROSCOPE. Different reporting mode suffix for IMU sensors.
  - Probe 8: VOICE/ALARM audio channel — SKIPPED, channel IDs 9-13 unmapped in aasdk. Would break session without channel-map discovery first.

- **AA Protocol Reference document assembled** — ~400-line comprehensive reference compiled from all probe findings covering session lifecycle, channel architecture, service discovery, video pipeline, audio pipeline, input/sensors, and open questions.

## Key Design Decisions

1. **Wireless only** — USB AA adds complexity (AOAP, libusb, USB permissions) for an increasingly irrelevant use case. The Pi's built-in WiFi/BT supports wireless natively. This simplifies the entire transport layer.

2. **Plugin-first architecture** — Every user-facing feature is a plugin, including core AA. The shell and plugin system are the product; AA is the killer app. This enables community extensions without forking.

3. **YAML over INI for configuration** — INI was inherited from the original codebase. YAML supports hierarchical config, plugin-scoped sections, deep merge with hardware profiles, and is more readable. yaml-cpp chosen as the C++ parser.

4. **PipeWire for audio** — Default on RPi OS Trixie. All audio sources are PipeWire nodes with priority-based routing. The app creates streams and manages focus; PipeWire handles mixing and hardware output. No custom audio engine.

5. **Strategy pattern for hardware abstraction** — Hardware profiles are pure YAML config. Behavioral differences (e.g., hostapd vs NetworkManager) are encapsulated in strategy classes. Adding a new platform with same tools = new YAML file only.

6. **Single config writer** — The Qt main app is the only process that writes config files. Web config server communicates via Unix socket IPC. Prevents partial writes, corruption, and race conditions.

7. **Data-driven performance optimization** — Instrument first, measure, fix in priority order, re-measure. No premature optimization. Performance targets defined upfront with clear decision criteria.

8. **Decode off main thread** — Video decode moved to a dedicated QThread. Only `setVideoFrame()` crosses to the main thread. Frees ~150ms/sec of main thread time at 30fps.

9. **Protocol version 1.7** — Bumped from v1.1. Phone always negotiates v1.7 regardless of what we request, but requesting v1.7 triggers a larger ServiceDiscoveryRequest with more capabilities.

10. **5GHz mandatory for WiFi AP** — BT/WiFi coexistence on the CYW43455 chip requires 5GHz. Channel 36 chosen as non-DFS in most regions.

11. **v1 simplicity constraints** — No hot-loading plugins. No plugin-to-plugin services. No pre-1.0 API stability guarantees. Ship minimal, extend easily.

## Lessons Learned

### Protocol

- **RFCOMM message IDs matter critically** — The original code used wrong message IDs and proto type names (WifiInfoRequest where WifiStartRequest was needed, etc.). The phone silently fails on incorrect IDs with no error feedback.

- **AnnexB start codes already present in aasdk** — The video decoder was double-prepending `00 00 00 01` start codes, corrupting the bitstream. Always check whether upstream already includes framing.

- **SPS/PPS arrives differently than video frames** — Sent as `AV_MEDIA_INDICATION` (no timestamp) rather than `AV_MEDIA_WITH_TIMESTAMP_INDICATION`. Must be forwarded to the decoder or it can't initialize.

- **Phone silently ignores wrong-versioned fields** — Even with incorrect ChannelDescriptor field numbers (aasdk's legacy numbering), the phone completes connection by silently discarding misaligned config and negotiating per-channel during setup. This masks bugs.

- **hide_clock (field 12) is dead on modern AA** — `CarDisplayUiFeatures` is the replacement mechanism. Old fields persist in protos but are ignored.

- **Phone expects palette version 2** — But the field isn't in any known proto definition. Falls back to default theming (cosmetic impact only).

- **GYROSCOPE ignored by phone** — Phone requests COMPASS and ACCELEROMETER when advertised but not GYROSCOPE. IMU sensors use a different reporting mode suffix (`0x10 0x03`).

### Performance

- **QVideoFrame allocation overhead is significant** — At 30fps, per-frame heap allocation of QVideoFrame + QVideoFrameFormat is measurable. Caching the format and double-buffering frames eliminates ~30 allocations/sec.

- **Stride-aware bulk copy vs line-by-line** — When FFmpeg linesize matches QVideoFrame bytesPerLine (common for standard resolutions), one memcpy per plane replaces hundreds of per-scanline copies. Massive throughput improvement.

- **Qt::QueuedConnection has real cost** — Moving decode off the main thread eliminates both the decode time AND the cross-thread queuing overhead that blocked touch events and QML rendering.

- **Touch batching matches protocol spec** — AA protocol explicitly supports multiple `touch_location` entries per `InputEventIndication`. The original per-finger sends were unnecessarily expensive.

### Architecture

- **Deep merge is not built into yaml-cpp** — Required a custom `mergeYaml()` utility. Rules: mappings recurse, sequences override entirely, scalars override, missing keys preserve defaults.

- **Plugin config split-brain risk** — Early designs had a separate `pluginConfigs_` QMap alongside the YAML tree. Eliminated in favor of single source of truth under `root_["plugin_config"][pluginId][key]`.

- **HFP stack needs spiking** — Neither ofono nor BlueZ native HF role is guaranteed to work on RPi OS Trixie. Spike validation before committing to implementation saved potential rework.

- **Pi 4 is the floor, not the ceiling** — Design decisions should always work on Pi 4 with a basic touchscreen. If it doesn't run well there, it doesn't ship.

## Source Documents

- `2026-02-16-protocol-audit-fixes.md` — 15-issue protocol audit with 4 priority tiers, implementation steps, and status tracking
- `2026-02-17-aa-performance-design.md` — Performance bottleneck analysis, optimization design (Phases A-C), performance targets
- `2026-02-17-aa-performance-plan.md` — 10-task implementation plan for instrumentation + optimizations + deployment/measurement
- `2026-02-18-architecture-and-ux-design.md` — Comprehensive architecture design: plugin system, hardware profiles, themes, audio, config, UI layout, settings, installation, v1.0 scope
- `2026-02-18-architecture-implementation-plan.md` — 37-task migration plan across 12 phases (A-L) from monolithic to plugin-based architecture
- `autorun/protocol-exploration-01.md` — ProtocolLogger creation, Messenger hooks, session lifecycle wiring
- `autorun/protocol-exploration-02.md` — capture.sh and merge-logs.py tooling scripts
- `autorun/protocol-exploration-03.md` — Pi deployment and baseline capture
- `autorun/protocol-exploration-04.md` — Probes 1-3: version bump, 48kHz speech, night mode
- `autorun/protocol-exploration-05.md` — Probes 4-8: palette v2, display features, 1080p, sensors, voice channel
- `autorun/protocol-exploration-06.md` — AA Protocol Reference document assembly from all probe findings
