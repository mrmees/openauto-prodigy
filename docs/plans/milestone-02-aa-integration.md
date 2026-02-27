# Milestone 2: AA Integration (Feb 19-21, 2026)

## What Was Built

### Firmware-Derived Protocol Features (Feb 19)

Six AA protocol features implemented based on cross-vendor firmware analysis (Alpine, Kenwood, Sony, Pioneer head units):

- **Identity fields in ServiceDiscoveryResponse** — Head unit name, manufacturer, model, software version, car model, car year, left-hand drive, and hardware profile are now read from YamlConfig instead of being hardcoded. These populate the "About this car" screen on the phone.
- **Multi-resolution video advertisement** — VideoService now advertises a primary resolution (configurable: 480p/720p/1080p) plus a mandatory 480p fallback, with configurable FPS (30/60) and DPI. Previously only 720p was hardcoded. Phones select from the advertised list; production head units always include a 480p fallback.
- **Night mode sensor** — New NightModeProvider abstraction with three implementations: TimeBased (configurable day/night hours), GpioWatcher (reads a GPIO pin for ambient light sensor), and None (always day). Runs on a 60-second polling timer. Sends NightData sensor events to the phone via the existing SensorServiceStub.
- **Video focus state machine** — Proper VideoFocusIndication tracking with FOCUSED/UNFOCUSED/PROJECTED states. Sends focus gain on video start and focus release on exit-to-car. Previously there was no focus management, which could cause the phone to stop sending video frames.
- **GPS sensor advertisement** — SensorServiceStub now advertises GPS_LOCATION capability in the channel descriptor. The actual GPS data path was stubbed (no GPS hardware on the Pi), but advertising the capability prevents some phones from showing a "no GPS" warning.
- **Microphone input via PipeWire** — AVInputServiceStub extended with a PipeWire capture stream (16kHz mono 16-bit PCM, `openauto-mic`). Captures from user-selected input device (or PipeWire default). Sends captured audio to the phone via AVMediaIndication for Google Assistant and phone calls. Configurable mic gain multiplier.

### Infrastructure Improvements (Feb 19)

Three hardware-portability features:

- **Touch auto-discovery** — New `InputDeviceScanner` class scans `/dev/input/event0-31` for devices with `INPUT_PROP_DIRECT` capability (identifies touchscreens). Replaces the hardcoded `/dev/input/event4` path. Config override available via `touch.device` YAML key; empty string triggers auto-detect.
- **WiFi AP networking via install script** — Install script now detects wireless interfaces via `/sys/class/net/*/wireless`, lets user pick one if multiple exist, and writes `systemd-networkd` + `hostapd` configs. systemd-networkd's built-in DHCP server replaces the previous dnsmasq dependency. WiFi interface stored in `wifi.interface` config key, read by BluetoothDiscoveryService for IP advertisement.
- **Boot-to-app systemd service** — Cleaned up systemd service file (removed unnecessary `Wants=pipewire.service bluetooth.service`, added `WorkingDirectory`). Auto-start is now opt-in during install (user prompted yes/no).

### AA Sidebar (Feb 21)

Configurable sidebar alongside Android Auto video using protocol-level margin negotiation:

- **Protocol-level margins** — Uses `VideoConfig.margin_width` / `margin_height` fields to make the phone render its UI into a smaller centered region within the standard resolution. The head unit crops the black bars and fills the reclaimed pixels with a sidebar strip. Technique documented by Emil Borconi (HeadUnit Reloaded / Crankshaft).
- **Margin calculation** — For a given sidebar width (default 150px) and display resolution (1024x600), computes the optimal margin values to make the AA content fill the viewport without visible black bars. Applied to both the primary resolution and the 480p fallback.
- **Sidebar QML component** — Vertical strip with volume up/down buttons, vertical volume slider, and home button. Themed with the existing ThemeService colors.
- **RowLayout integration** — AndroidAutoMenu.qml uses a RowLayout with video area + sidebar. Layout direction reverses when sidebar position is "left". Video uses `PreserveAspectCrop` fill mode to clip margin bars.
- **Evdev touch zones for sidebar** — Since EVIOCGRAB prevents Qt from receiving touches during AA, sidebar touch handling is done entirely in EvdevTouchReader via evdev coordinate hit zones. QML MouseAreas are visual-only fallback for dev/testing. Each action slot (volume up, volume down, home) occupies a rectangular segment of the sidebar's evdev coordinate range.
- **Configuration** — `video.sidebar.enabled`, `video.sidebar.width`, `video.sidebar.position` in YAML config. Settings UI provides toggle, position dropdown, and width spinbox. Changes require app restart (margins locked at AA session start).

### Audio Pipeline (Feb 21)

Complete rebuild of AudioService to make AA audio actually play:

- **Thread model fix** — Replaced broken `pw_main_loop` (event loop never ran) with `pw_thread_loop` (runs PipeWire on its own internal thread). All PipeWire API calls from Qt main thread bracketed with `pw_thread_loop_lock()/unlock()`.
- **Lock-free ring buffers** — `AudioRingBuffer` wrapping `spa_ringbuffer` for each stream. Bridges ASIO threads (producer, AA audio data) to PipeWire RT thread (consumer, buffer fill). Sized for ~100ms: media (48kHz stereo) = ~19.2KB, speech/system (16kHz mono) = ~3.2KB. Silence-fill on underrun, drop-oldest on overrun.
- **Three output streams** — `openauto-media` (48kHz stereo, music), `openauto-speech` (16kHz mono, navigation/communication), `openauto-system` (16kHz mono, notifications). PipeWire mixes natively, enabling per-stream volume control and role-based ducking.
- **One capture stream** — `openauto-mic` (16kHz mono) for Google Assistant and phone calls.
- **Device enumeration** — `PipeWireDeviceRegistry` using `pw_registry` listener, filtering for `Audio/Sink`, `Audio/Source`, and `Audio/Duplex` nodes. Exposes `AudioOutputDeviceModel` and `AudioInputDeviceModel` (QAbstractListModel subclasses) to QML with a synthetic "Default (auto)" entry.
- **Device targeting** — Selected device's `node.name` set as `PW_KEY_TARGET_OBJECT` on stream creation. "auto" omits the property, letting WirePlumber route to default.
- **Hot-plug handling** — `global_remove` registry event removes entries from the model. If the selected device disappears, streams are recreated without a target (fallback to default). No auto-switch-back when device reappears.
- **Volume control** — `pw_stream_set_control(SPA_PROP_channelVolumes)` on all output streams, driven by `audio.master_volume` (0-100, cubic curve). Existing ducking logic applies per-stream multipliers.
- **Web config integration** — New IPC commands: `get_audio_devices`, `set_audio_config`, `get_audio_config`.

### Settings Page Redesign (Feb 21)

Complete rewrite from flat disabled-tile grid to functional two-level settings UI:

- **StackView navigation** — Tile grid (3x2) as initial view; tapping a tile pushes a subpage. Back arrow returns to grid. No nesting deeper than one level.
- **QML-Config bridge** — ConfigService exposed as QML context property with `Q_INVOKABLE value()` / `setValue()` methods and a `configChanged` signal. Generic path-based API avoids per-setting Q_PROPERTY boilerplate for ~45 settings.
- **Six reusable controls** — `SettingsToggle`, `SettingsSlider`, `SettingsComboBox`, `SettingsTextField`, `SettingsSpinBox`, `SectionHeader`. Each binds to a config path, handles its own read on `Component.onCompleted`, and writes on value change. Slider writes are debounced (300ms QML Timer).
- **Six category subpages** — Display (8 settings: brightness, theme, orientation, day/night config), Audio (4 settings: volume, output/input device, mic gain), Connection (7 settings: AA auto-connect, TCP port, WiFi AP, BT discoverable), Video (3 settings: FPS, resolution, DPI), System (9 settings: identity fields, hardware, plugins), About (version info, exit button).
- **Restart badge** — Pure QML; controls with `restartRequired: true` show a small restart icon next to the label.
- **Conditional visibility** — GPIO-related night mode settings only visible when night mode source is "gpio".

## Key Design Decisions

### Protocol & Video

- **Margin-based sidebar over custom resolution** — AA only supports fixed resolutions (480p/720p/1080p). Rather than trying to negotiate a non-standard resolution (which phones reject), use the `margin_width`/`margin_height` protocol fields to instruct the phone to render in a smaller region. This is a proven technique used by other AA head unit implementations.
- **Sidebar changes require restart** — Margins are advertised during ServiceDiscoveryResponse and locked for the session. No mid-session renegotiation exists in the AA protocol. Attempting a controlled AA reconnect on toggle was deferred to a future version.
- **Mandatory 480p fallback** — Production AA SDKs always include a 480p video config entry. Omitting it caused connection failures on some phone models. Both the primary resolution and fallback get margin values calculated independently.

### Audio Architecture

- **`pw_thread_loop` over `pw_main_loop`** — `pw_main_loop_run()` blocks, requiring a dedicated thread with no easy cross-thread access. `pw_thread_loop` runs PipeWire internally and provides lock/unlock API for safe access from Qt's main thread. This is the standard pattern for GUI applications using PipeWire.
- **`spa_ringbuffer` over custom lock-free queue** — PipeWire's own SPA library provides a battle-tested SPSC ring buffer. Using it avoids reinventing lock-free data structures and ensures compatibility with PipeWire's RT thread model.
- **Three separate output streams over one mixed stream** — PipeWire natively mixes streams routed to the same sink. Separate streams give per-stream volume control, proper audio role tagging (WirePlumber policies), and built-in ducking without manual mix logic.
- **Device selection requires restart** — Live PipeWire stream re-routing (disconnect + destroy + recreate with new target) didn't work reliably in testing. Brief audio gap (~50-100ms) was acceptable in theory but caused state corruption. Deferred to future work.
- **`node.name` as device identifier** — Stable within a PipeWire session and usually stable across reboots for the same USB port. Known limitation: different port = different name. Composite identity (VID+PID+serial) deferred.

### Settings & Configuration

- **StackView over Loader** — StackView provides push/pop transitions and back navigation for free. Loader would require manual state management.
- **Generic path API over per-setting Q_PROPERTYs** — With ~45 settings, per-setting properties would be massive boilerplate. The path-based system already has schema validation.
- **ConfigService over ApplicationController** — Keeps config concerns separate from app lifecycle control. ApplicationController already has too many responsibilities.
- **Section headers over nested pages** — At 1024x600 resolution, a third navigation level would be confusing. Scrollable sections within a single subpage is the right compromise for the screen size.
- **Debounced slider writes** — 300ms timer prevents writing YAML to disk on every pixel of a slider drag.

### Infrastructure

- **systemd-networkd over dnsmasq** — systemd-networkd has a built-in DHCP server, eliminating a dependency. Configuration is declarative (`.network` files) rather than daemon config.
- **`INPUT_PROP_DIRECT` scan over udev rules** — Scanning `/dev/input/event*` for the `INPUT_PROP_DIRECT` property correctly identifies touchscreens regardless of device enumeration order. No udev rules needed; works on any Linux system.
- **Auto-start opt-in** — Many users want to SSH in and debug before enabling boot-to-app. Making it a prompt during install avoids "how do I stop it from auto-starting" support questions.

## Lessons Learned

### EVIOCGRAB vs QML Touch

During AA, `EVIOCGRAB` is active to route touch events directly to the AA protocol (bypassing Qt/Wayland). This means QML `MouseArea` components in the sidebar receive zero touch events on the Pi. The sidebar's touch handling must be done entirely via evdev coordinate hit zones in `EvdevTouchReader`. The QML `MouseArea` elements serve as a fallback for dev/VM testing only. This dual-path approach (evdev for Pi, QML for dev) is functional but creates a maintenance burden where touch behavior must be kept in sync across two systems.

### PipeWire Pull Model

The original AudioService tried to push audio imperatively via `pw_stream_dequeue_buffer()`. PipeWire uses a pull model: a `process` callback fires on the RT thread when it needs data. The producer (ASIO thread receiving AA audio) writes to a ring buffer; the consumer (PipeWire RT callback) reads from it. Trying to push data into PipeWire from the wrong thread silently drops all audio.

### PipeWire Period Filling

PipeWire output callbacks must always fill the full requested period (`d.chunk->size = maxSize`), silence-filling any gap after actual audio data. Setting `chunk->size` to only the bytes read from the ring buffer causes tempo wobble (audio speeds up and slows down) because PipeWire's graph timing is fixed by quantum/rate.

### AA Margin Behavior

The `VideoConfig.margin_width` / `margin_height` fields are split evenly by the phone — half on each side. So `margin_width=231` produces ~115px black bars on left and right of a 1280x720 frame. The head unit must crop both margins to extract the content region. The `PreserveAspectCrop` fill mode in QML handles this automatically when the video's aspect ratio differs from the viewport's.

### Night Mode Provider Abstraction

Night mode needed to support three very different sources (time-based calculation, GPIO hardware pin, disabled). Rather than if/else chains in the sensor service, a provider interface with three implementations (`TimeBasedNightMode`, `GpioNightMode`, `NoneNightMode`) keeps the logic clean and extensible. The provider is selected once at startup based on config and polled on a 60-second timer.

### Config Path API Scaling

The generic `value(path)` / `setValue(path, value)` API proved far more maintainable than the alternative of adding Q_PROPERTY for each setting. With 45+ settings and more coming, the path-based system keeps ConfigService's interface stable while settings proliferate. The tradeoff is loss of type safety at the QML boundary (everything is QVariant), but schema validation in the config layer catches type mismatches.

### Device Hot-Plug Edge Cases

When a selected PipeWire device disappears (USB unplug), the fallback behavior intentionally does NOT auto-switch back when the device reappears. Users may have adjusted their setup during the absence, and auto-switching could be disruptive. A notification that the preferred device is available again is the better UX.

## Technical Reference

### Margin Calculation Formula

```
aaViewportWidth = displayWidth - sidebarWidth
screenRatio = aaViewportWidth / displayHeight

if screenRatio < remoteRatio:
    margin_width = round(remoteWidth - (remoteHeight * screenRatio))
    margin_height = 0
else:
    margin_width = 0
    margin_height = round(remoteHeight - (remoteWidth / screenRatio))
```

Example: 1024x600 display, 150px sidebar, 720p (1280x720):
- AA viewport = 874x600, screenRatio = 1.457, remoteRatio = 1.778
- margin_width = round(1280 - 720 * 1.457) = 231, margin_height = 0

### Audio Stream Specs

| Stream | AA Channel | Format | Ring Buffer |
|--------|-----------|--------|-------------|
| `openauto-media` | MEDIA_AUDIO | 48kHz stereo 16-bit | ~19.2 KB |
| `openauto-speech` | SPEECH_AUDIO | 16kHz mono 16-bit | ~3.2 KB |
| `openauto-system` | SYSTEM_AUDIO | 16kHz mono 16-bit | ~3.2 KB |
| `openauto-mic` | AV_INPUT | 16kHz mono 16-bit | ~3.2 KB |

### PipeWire Device Registry Fields

| Field | Source | Use |
|-------|--------|-----|
| `node.name` | PipeWire property | Config value, `PW_KEY_TARGET_OBJECT` |
| `node.description` | PipeWire property | UI display name |
| `media.class` | PipeWire property | Filter: `Audio/Sink`, `Audio/Source`, `Audio/Duplex` |
| `registry_id` | PipeWire session | Track add/remove events |

### Settings Categories

| Category | Settings Count | Notable Controls |
|----------|---------------|------------------|
| Display | 8 | Brightness slider, theme picker, day/night source with conditional GPIO fields |
| Audio | 4 | Volume slider, device dropdowns (PipeWire enumerated), mic gain |
| Connection | 7 | WiFi AP config (restart badges), TCP port, BT discoverable toggle |
| Video | 3 | Resolution/FPS/DPI (all restart-required) |
| System | 9 | Identity fields, hardware profile, touch device override |
| About | 2 | Version display, exit button |

### New C++ Classes and Files

| Class/File | Purpose |
|-----------|---------|
| `InputDeviceScanner` | Scans `/dev/input/event*` for INPUT_PROP_DIRECT touchscreens |
| `NightModeProvider` | Abstract interface for night mode source |
| `TimeBasedNightMode` | Night mode from configurable day/night hours |
| `GpioNightMode` | Night mode from GPIO pin (ambient light sensor) |
| `AudioRingBuffer` | Lock-free SPSC ring buffer wrapping `spa_ringbuffer` |
| `PipeWireDeviceRegistry` | Hot-plug device enumeration via `pw_registry` |
| `AudioOutputDeviceModel` | QAbstractListModel for sink selection UI |
| `AudioInputDeviceModel` | QAbstractListModel for source selection UI |

### New QML Files

| File | Purpose |
|------|---------|
| `Sidebar.qml` | AA sidebar strip (volume + home) |
| `DisplaySettings.qml` | Display category subpage |
| `AudioSettings.qml` | Audio category subpage |
| `ConnectionSettings.qml` | Connection category subpage |
| `VideoSettings.qml` | Video category subpage |
| `SystemSettings.qml` | System category subpage |
| `AboutSettings.qml` | About category subpage |
| `SettingsToggle.qml` | Reusable toggle control |
| `SettingsSlider.qml` | Reusable slider control (300ms debounce) |
| `SettingsComboBox.qml` | Reusable combo box control |
| `SettingsTextField.qml` | Reusable text field control |
| `SettingsSpinBox.qml` | Reusable spin box control |
| `SectionHeader.qml` | Styled section divider |

## Deferred / Out of Scope

- HFP call audio routing through PipeWire (phone plugin, separate effort)
- Per-function device routing (navigation to different speaker than music)
- Dynamic plugin loading (.so) testing
- Mid-session sidebar toggle (requires AA reconnect)
- User-configurable sidebar action slots (v2 — hardcoded volume + home for v1)
- Phone contacts sync (PBAP profile)
- Equalizer / DSP processing
- Multi-zone audio output

## Source Documents

- `2026-02-19-firmware-features-implementation.md` — Firmware-derived protocol features implementation (identity, resolution, night mode, video focus, GPS, microphone)
- `2026-02-19-infrastructure-improvements-design.md` — Infrastructure design (touch auto-detect, WiFi AP config, boot-to-app)
- `2026-02-19-infrastructure-improvements-implementation.md` — Infrastructure implementation plan (InputDeviceScanner, install script, systemd service)
- `2026-02-21-aa-sidebar-design.md` — AA sidebar design (margin math, touch zones, config)
- `2026-02-21-aa-sidebar-implementation.md` — AA sidebar implementation plan (YamlConfig, VideoService, QML, EvdevTouchReader)
- `2026-02-21-audio-pipeline-design.md` — Audio pipeline design (PipeWire thread model, ring buffers, device enumeration)
- `2026-02-21-audio-pipeline-plan.md` — Audio pipeline implementation plan (AudioRingBuffer, AudioService rebuild, device registry, settings UI)
- `2026-02-21-settings-redesign-design.md` — Settings page redesign design (StackView, ConfigService bridge, reusable controls)
- `2026-02-21-settings-redesign-plan.md` — Settings page redesign implementation plan (ConfigService Q_INVOKABLE, 6 controls, 6 subpages)
