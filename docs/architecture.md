# Architecture

System map: what the components are, where the boundaries sit, and how data moves at runtime. For build instructions see [development.md](development.md); for the full doc map see [INDEX.md](INDEX.md).

## Main Components

### Composition root (`src/main.cpp`)

Wires the entire system:

- Loads config (`~/.openauto/config.yaml`) via `YamlConfig` (schema-backed, defaults + deep merge, versioned widget-grid migrations).
- Initializes shared services (theme, audio, Bluetooth, config, event bus, actions,
  notifications, overlays, phone/media state, IPC, and the External API).
- Registers and initializes plugins through `PluginManager`.
- Exposes models/services to QML context, boots the QML shell, and binds plugin view hosting.

### Core services (`src/core/services/`)

The plugin-facing `IHostContext` exposes interfaces for config, theme, display,
audio, Bluetooth, events, actions, notifications, equalization, and overlays. It
also exposes nullable projection, navigation, media-status, and call-state
providers. Plugins depend on those interfaces rather than concrete service
implementations.

The composition root owns additional application services that are not part of
that plugin contract. These include `MediaStatusService` (arbitration across
AA, Bluetooth, and local playback), `PhoneStateService`, `IpcServer`,
`SystemServiceClient`, `WeatherService`, and `ClockSyncService`.
`AudioService` owns PipeWire streams, audio focus/ducking, and its
`PipeWireDeviceRegistry`. `EqualizerService` fans shared per-stream settings out
to a dedicated `EqualizerEngine` for each live audio consumer.

`BluetoothManager` subscribes before asynchronously requesting one BlueZ
`GetManagedObjects` snapshot. That snapshot is the shared boundary for adapter,
paired-device, connected-device, first-run, and auto-connect state; changes
arriving during a request coalesce into one trailing refresh. The exported
DisplayYesNo `Agent1` implements BlueZ's complete method surface: confirmation
and authorization calls remain pending for user choice, display calls are
informational, and unsupported keyboard-entry calls fail explicitly.

### External API (`src/core/api/`)

`ApiServer` + `ApiSession`/`ApiTransport`/`ApiFramer` serve External API v1
(protobuf over TCP/WebSocket) with `PairingManager` auth. New pairing windows
use a versioned 120-bit random Base32 code carried by QR (grouped for the manual
fallback); stored credentials record their generation so legacy six-digit
credentials fail closed and require re-pairing. Publishers push provider state;
request handlers invoke actions.

Phone time reports feed `ClockSyncService`, which runs the ordered
`timedatectl` NTP-off/time-step/NTP-on transaction asynchronously. A successful
wall-clock step notifies the phone-status publisher so an active call's epoch
start is recomputed from the corrected clock without blocking the Qt main
thread.

### AA runtime (`src/core/aa/`)

- `AndroidAutoOrchestrator` — Qt TCP listener/session lifecycle, channel handler
  registration, watchdogs, reconnect, and focus transitions.
- `VideoDecoder` — H.264/H.265 detection and FFmpeg decode on a dedicated
  worker, with configured/automatic hardware selection and software fallback.
  Decoded frames use `VideoFramePool` or the DRM-prime
  `DmaBufVideoBuffer` path before delivery to `QVideoSink` on the Qt thread.
- `AndroidAutoRuntimeBridge`, `EvdevTouchReader`, `TouchRouter`, and
  `TouchHandler` — direct evdev multi-touch, navbar hit-zone routing, AA input
  messages, and 3-finger gesture detection. The device is grabbed only while
  projection owns input.
- `BluetoothDiscoveryService`, night-mode providers, navigation/media data bridges.
  The selected night provider is evaluated and explicitly seeds the sensor
  handler before the AA session starts. The handler retains that latest value
  independently of channel/subscription readiness, sends it when the phone
  subscribes to NIGHT_DATA, and preserves it across channel close/reopen while
  clearing only the subscription set. Providers expose whether their initial
  evaluation succeeded; a failed GPIO read preserves the last cached sensor
  value until a later valid provider update arrives.

### Protocol library (`libs/prodigy-oaa-protocol/`)

In-tree Qt-based static library implementing the AA protocol: transport
(`TCPTransport`, `ReplayTransport`), framing/messenger/encryption, session state
machine (`AASession`), and head-unit channel handlers (`oaa::hu::*`) for video,
audio, AV input, input, sensor, Bluetooth, WiFi, navigation, media status, and
phone status. Protobuf definitions come from the
[open-android-auto](https://github.com/mrmees/open-android-auto) community repo —
a hands-off git submodule at `libs/prodigy-oaa-protocol/proto/`.

### Plugin layer (`src/plugins/`)

Static plugins compiled into the binary, implementing `IPlugin` (see [reference/plugin-api.md](reference/plugin-api.md)):

- `android_auto` — projection lifecycle, activation/deactivation hooks, touch integration, AA focus controls.
- `bt_audio` — BlueZ D-Bus monitoring for A2DP media transport/player state and AVRCP controls; owns `BtAudioTap`, which routes BT music through the app's EQ (see "BT A2DP EQ tap" below).
- `bt_audio` subscribes before issuing an asynchronous, coalesced
  `GetManagedObjects` startup scan. Complete snapshots and later
  `InterfacesAdded` payloads feed the same carried-property adoption contract
  for `Device1`, `MediaTransport1`, and `MediaPlayer1`; no hot-plug handler
  performs interface introspection or a property read. Missing carried values
  remain unknown/inactive until BlueZ delivers them. Removing the tracked
  player atomically returns playback, metadata, duration, position, and
  position validity to their stopped/unknown values.
- BlueZ `MediaPlayer1.Position` and `Track.Duration` enter `bt_audio` as
  `uint32` milliseconds. Startup enumeration, player adoption, and later
  property changes widen them to `qint64` without scaling, then publish the
  same millisecond snapshot through `MediaStatusService` to shared widgets and
  External API media status. BlueZ loss, player replacement, or D-Bus
  invalidation returns missing player time state to unknown rather than
  retaining a prior session's values.
- `phone` — dialer/call UI backed by the core `PhoneStateService`; the core
  service owns BlueZ device monitoring and `org.pipewire.Telephony` call state.
  `TelephonyClient` selects one AudioGateway and accepts transport state only
  from the `AudioGatewayTransport1` interface co-located on that same object;
  other phones cannot retarget the selected transport. If that gateway leaves,
  the next cached gateway is selected deterministically without an unavailable
  edge. SCO can confirm a
  setup/settling call and can debounce the end of an active call, but SCO alone
  never synthesizes an Active call from Idle. Call objects must be children of
  the selected gateway, and an invalidated transport state is unknown rather
  than an end event. Plugin shutdown disconnects the provider before releasing
  it, so late provider signals are inert.
- `media_player` — local file playback (`PlaybackEngine`, `PlayQueue`, `FolderModel`, `MediaArtProvider`).
- `equalizer` — EQ control over `EqualizerService`.

Dynamic plugins load from `~/.openauto/plugins/` (manifest + `.so`). The shell
has no app nav strip: launcher widgets dispatch actions that activate plugin
views, while state-driven surfaces such as the incoming-call overlay appear
through their owning service.

### BT A2DP EQ tap

A WirePlumber rule (`config/50-openauto-bt-eq.conf`, installed to
`/etc/wireplumber/wireplumber.conf.d/`) retargets BlueZ A2DP input streams
(matched by `node.name = ~bluez_input.*` AND
`api.bluez5.profile = a2dp-source` — HFP SCO voice shares the name pattern
and is deliberately excluded) onto the app's capture node
`openauto-bt-eq-in`. The capture node exists while `BtAudioTap` (owned by
`BtAudioPlugin`) is up; the tap's "BT Audio" playback leg is what toggles
with A2DP transport activity. `BtAudioTap`
drains the capture through a ring buffer into a dedicated "BT Audio"
playback stream carrying its own Media-curve `EqualizerEngine` instance
(one of the per-consumer engines `EqualizerService::acquireEngine` hands
out — see Core services), so BT music now gets the same preset/gain
treatment as AA media and local playback, plays through the HU master
volume, and participates in focus arbitration like any other music source.
When the app isn't running (or the tap hasn't brought the node up yet) the
retarget has nothing to attach to and WirePlumber falls back to the
default sink — BT audio still plays, just un-EQ'd; the rule deliberately
omits `node.dont-fallback` so that fallback stays available.

### UI layer (`qml/` + `src/ui/`)

QML shell and app views are packaged into the binary via `qt_add_qml_module`
(+ qmlcache) — UI changes ship by rebuilding, not by copying QML files.
`PluginModel` and `PluginViewHost` activate plugin views. The host detaches a
view logically before retiring its QML subtree and runtime context as one
ordered unit: the subtree survives the current dispatch, is destroyed on the
next event-loop turn, and only then are its context properties deactivated.
Explicit QObject ownership makes shell teardown complete the same order
synchronously rather than stranding pending views or contexts. A
`ScreenDpiBinding` owns the replaceable window/current-screen DPI connections;
only the active screen publishes structural DPI data to `DisplayInfo`.
`DashboardManager` owns one `WidgetGridModel`/`WidgetContextFactory` pair per
dashboard and seeds the Android Auto and Settings singleton launchers on an
empty home dashboard.
`WidgetRegistry` combines built-in descriptors, plugin contributions, and —
when WebEngine is available — valid web-widget packages.

The application window is fullscreen in normal operation (the `--geometry`
development override is windowed). A plugin's `wantsFullscreen()` controls
whether the in-shell navbar remains visible; it does not toggle the native
window between windowed and fullscreen states. Android Auto keeps the navbar by
default and reserves a matching viewport in service discovery.

### Web config (`web-config/`)

The Flask app in `web-config/server.py` is the process-external configuration
surface. It serves status, settings, theme, audio, and plugin pages and forwards
requests to the Qt application over its Unix-domain IPC socket. The Qt process
remains the writer for runtime and persisted state.

This is separate from the optional in-process web-widget runtime. With
`HAS_WEBENGINE`, packages under `~/.openauto/webwidgets/` are scanned into the
widget catalog, served through the jailed `prodigy://widgets/` scheme, and
hosted lazily by `WebWidgetHost.qml`. Their JavaScript shim communicates through
the same public External API available to other clients.

## Boundaries

These are load-bearing; crossing one is a bug even if it works today:

- **AA transport, session, and channel objects stay on the Qt main event loop.**
  `QTcpServer`/`QTcpSocket`, `TCPTransport`, `AASession`, and the channel
  handlers are Qt objects; decoder and evdev workers return only through
  thread-safe queues or queued signals.
- **Plugins reach services only via `IHostContext`.** No direct includes of service internals, no global singletons.
- **Web config writes only through `IpcServer`.** The Qt side is the single writer for runtime config/theme/audio state; Flask never mutates state itself.
- **The External API binds providers/services, never EventBus topics, D-Bus paths, or AA protocol internals.** Its proto contract (`proto/api/`) is frozen additive-only.
- **All mutation goes through `ActionRegistry` or explicit invokables** — no side-channel setters exposed to remote surfaces.

## Runtime Data Flow

Wireless AA session path (high-level):

1. Phone discovers the head unit over the BT/WiFi workflow (RFCOMM handshake
   hands out AP credentials). `BluetoothDiscoveryService` retries a transient
   RFCOMM listener startup failure on its bounded timer and registers the
   legacy BlueZ SDP record only after the listener returns a nonzero channel;
   listener and SDP retries are separate lifecycle owners.
2. Phone joins the Pi's WiFi AP and connects to the HU TCP port.
3. `AndroidAutoOrchestrator` creates `oaa::AASession` and registers channel handlers.
4. Video/audio/input/sensor events flow through handlers into app services.
5. Compressed H.264/H.265 packets queue to `VideoDecoder`'s worker. The latest
   decoded frame returns to the Qt thread and renders through QML `VideoOutput`.
6. Audio packets route through `AudioService` into PipeWire streams (media/navigation/system, with focus/ducking). Music sources (AA media, local playback, the BT tap) all request focus at priority 50; the holder is the most-recently-started of the three (a monotonic sequence number breaks priority ties), so starting any one of them takes focus from the others. Speech/nav prompts (priority 60) still duck whatever music holds focus.
7. Plugin/UI state updates propagate via Q_PROPERTY signals and event bus publications.

## Threading Model

| Thread | Runs | Crossing rule |
|---|---|---|
| Qt main | UI, QML, services, plugins, D-Bus, AA and External API sockets/sessions/channels | owns all UI and socket-facing Qt objects |
| Decode worker (`QThread`) | FFmpeg parsing/decode and frame production | latest-frame slot + `frameReady`; sink update occurs on Qt main |
| `EvdevTouchReader` (`QThread`) | direct evdev read, gestures, navbar zones, AA pointer construction | router callbacks/queued Qt signals; AA send returns through Qt signal delivery |
| PipeWire thread loop | real-time audio process callbacks | lock-free/ring-buffer and explicitly RT-safe state only |
| Flask process | web configuration HTTP requests | newline-framed JSON over the Unix-domain IPC socket |

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
- **hostapd / systemd-networkd** — the Pi's WiFi AP and built-in DHCP service
  that the phone joins (see [wireless-setup.md](wireless-setup.md) and
  [pi-config/](pi-config/)).
- **Phone-side AA client** — negotiates codecs/resolutions and drives the session; behavior quirks are documented in [aa-protocol/](aa-protocol/).
