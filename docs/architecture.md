# Architecture

System map: what the components are, where the boundaries sit, and how data moves at runtime. For build instructions see [development.md](development.md); for the full doc map see [INDEX.md](INDEX.md).

## Main Components

### Composition root (`src/main.cpp`)

Wires the entire system:

- Loads config (`~/.openauto/config.yaml`, with INI migration fallback) via `YamlConfig` (schema-backed, defaults + deep merge).
- Initializes shared services (theme, audio, config, event bus, actions, notifications, companion listener, IPC, External API).
- Registers and initializes plugins through `PluginManager`.
- Exposes models/services to QML context, boots the QML shell, and binds plugin view hosting.

### Core services (`src/core/services/`)

Shared host services reached through `IHostContext`: `ConfigService`, `ThemeService`, `AudioService` (PipeWire streams, volume, audio focus/ducking), `PipeWireDeviceRegistry`, `EventBus`, `ActionRegistry`, `NotificationService`, `MediaStatusService` (playing-wins arbitration across AA/BT/local sources), `PhoneStateService`, `OverlayService`, `EqualizerService`, `DisplayService`, `BluetoothManager`, `CompanionListenerService`, `IpcServer`.

### External API (`src/core/api/`)

`ApiServer` + `ApiSession`/`ApiTransport`/`ApiFramer` serve External API v1 (protobuf over TCP/WebSocket) with `PairingManager` auth. Publishers push provider state; request handlers invoke actions.

### AA runtime (`src/core/aa/`)

- `AndroidAutoOrchestrator` — TCP listener/session lifecycle, channel handler registration, watchdogs, reconnect, focus transitions.
- `VideoDecoder` — codec detection + FFmpeg decode (hw/sw auto-selected), feeding `QVideoFrame` output via `VideoFramePool` / `DmaBufVideoBuffer`.
- `EvdevTouchReader` + `TouchHandler` — direct evdev multi-touch into AA input channel messages; 3-finger gesture detection.
- `BluetoothDiscoveryService`, night-mode providers, navigation/media data bridges.

### Protocol library (`libs/prodigy-oaa-protocol/`)

In-tree static library implementing the AA protocol: transport (`TCPTransport`, `ReplayTransport`), framing/messenger/encryption, session state machine (`AASession`), head-unit channel handlers (`oaa::hu::*`) for video, audio, AV input, input, sensor, Bluetooth, WiFi, navigation, media status, phone status. Protobuf definitions come from the [open-android-auto](https://github.com/mrmees/open-android-auto) community repo — a hands-off git submodule at `libs/prodigy-oaa-protocol/proto/`.

### Plugin layer (`src/plugins/`)

Static plugins compiled into the binary, implementing `IPlugin` (see [reference/plugin-api.md](reference/plugin-api.md)):

- `android_auto` — projection lifecycle, activation/deactivation hooks, touch integration, AA focus controls.
- `bt_audio` — BlueZ D-Bus monitoring for A2DP media transport/player state and AVRCP controls.
- `phone` — BlueZ D-Bus monitoring for HFP device/call state and incoming-call UI integration.
- `media_player` — local file playback (`PlaybackEngine`, `PlayQueue`, `FolderModel`, `MediaArtProvider`).
- `equalizer` — EQ control over `EqualizerService`.

Dynamic plugins load from `~/.openauto/plugins/` (manifest + `.so`). Apps open via dashboard launcher widgets — there is no nav strip.

### UI layer (`qml/` + `src/ui/`)

QML shell and app views are packaged into the binary via `qt_add_qml_module` (+ qmlcache) — UI changes ship by rebuilding, not by copying QML files. `src/ui/` models/controllers bridge runtime state to QML: `PluginModel`, `PluginViewHost`, `PluginRuntimeContext`, `DashboardManager`, `AudioDeviceModel`, `CodecCapabilityModel`, `NotificationModel`, `ApplicationController`, and friends.

### Web config (`web-config/`)

Flask app (`web-config/server.py`) serves dashboard/settings/theme/plugin pages. Requests forward to Qt over a Unix domain socket (`/tmp/openauto-prodigy.sock`).

## Boundaries

These are load-bearing; crossing one is a bug even if it works today:

- **Protocol threads never touch Qt UI.** AA protocol work runs on ASIO threads; anything UI-bound crosses via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` to the Qt main thread.
- **Plugins reach services only via `IHostContext`.** No direct includes of service internals, no global singletons.
- **Web config writes only through `IpcServer`.** The Qt side is the single writer for runtime config/theme/audio state; Flask never mutates state itself.
- **The External API binds providers/services, never EventBus topics, D-Bus paths, or AA protocol internals.** Its proto contract (`proto/api/`) is frozen additive-only.
- **All mutation goes through `ActionRegistry` or explicit invokables** — no side-channel setters exposed to remote surfaces.

## Runtime Data Flow

Wireless AA session path (high-level):

1. Phone discovers the head unit over the BT/WiFi workflow (RFCOMM handshake hands out AP credentials).
2. Phone joins the Pi's WiFi AP and connects to the HU TCP port.
3. `AndroidAutoOrchestrator` creates `oaa::AASession` and registers channel handlers.
4. Video/audio/input/sensor events flow through handlers into app services.
5. Video frames decode in `VideoDecoder`, then render via QML `VideoOutput`.
6. Audio packets route through `AudioService` into PipeWire streams (media/navigation/phone, with focus/ducking).
7. Plugin/UI state updates propagate via Q_PROPERTY signals and event bus publications.

## Threading Model

| Thread | Runs | Crossing rule |
|---|---|---|
| Qt main | UI, QML, services, plugins, D-Bus | — |
| ASIO protocol threads | AA transport, session, channel handlers | `QMetaObject::invokeMethod(Qt::QueuedConnection)` into Qt main |
| Decode worker | FFmpeg video decode | emits frames to `QVideoSink` via queued signals |
| `EvdevTouchReader` (QThread) | evdev multi-touch read + gesture detection | queued signals into Qt main / AA input channel |

## Target Hardware

| Component | Details |
|-----------|---------|
| Board | Raspberry Pi 4 |
| Display | 1024x600 @ 60Hz (HDMI) |
| Touch | DFRobot USB Multi Touch (10-point, MT Type B, 0-4095 range) |
| WiFi | Built-in (used as AP for the phone connection) |
| BT | Built-in (RFCOMM for AA discovery, A2DP sink, HFP) |
| OS | RPi OS Trixie, labwc compositor |

## External Systems

- **BlueZ (D-Bus)** — device pairing, A2DP transport/player, HFP profile state.
- **PipeWire** — all audio I/O (playback streams, device registry, telephony via `org.pipewire.Telephony`; no ofono).
- **hostapd / dnsmasq** — the Pi's own WiFi AP that the phone joins (see [wireless-setup.md](wireless-setup.md) and [pi-config/](pi-config/)).
- **Phone-side AA client** — negotiates codecs/resolutions and drives the session; behavior quirks are documented in [aa-protocol/](aa-protocol/).
