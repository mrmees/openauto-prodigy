# OpenAuto Prodigy

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

Open-source Android Auto head unit stack for Raspberry Pi, rebuilt as a clean-room successor to OpenAuto Pro.

## Current State (February 26, 2026)

- Wireless Android Auto works end-to-end: discovery, session negotiation, video, audio, touch, reconnect handling.
- Plugin architecture is active with built-in plugins for Android Auto, Bluetooth audio, and phone/HFP.
- Settings, theming, notifications, action dispatch, and event bus are in use across the UI and plugins.
- Web config panel is functional and talks to the Qt app through local Unix socket IPC.
- Test status in current tree: `48/48` passing (`ctest --output-on-failure` in `build/`).
- Version metadata is currently inconsistent:
- App metadata reports `0.1.0` in `CMakeLists.txt` and `src/main.cpp`.
- Default identity software version in `YamlConfig` is `0.3.0`.

## Architecture

### 1) Application Composition Root

`src/main.cpp` wires the entire system:

- Loads config (`~/.openauto/config.yaml`, with INI migration fallback).
- Initializes shared services (theme, audio, config, event bus, actions, notifications, companion listener, IPC).
- Registers and initializes plugins through `PluginManager`.
- Exposes models/services to QML context.
- Boots the QML shell and binds plugin view hosting.

### 2) Core Modules (`src/core`)

- `aa/`: Android Auto runtime orchestration.
- `AndroidAutoOrchestrator` owns TCP listener/session lifecycle, channel handler registration, watchdogs, reconnect, and focus transitions.
- `VideoDecoder` handles codec detection + FFmpeg decode path, feeding `QVideoFrame` output.
- `EvdevTouchReader` + `TouchHandler` bridge direct input events into AA input channel messages.
- `plugin/`: plugin contracts and lifecycle.
- `IPlugin`, `IHostContext`, `PluginManager`, `PluginLoader`, `PluginDiscovery`, `PluginManifest`.
- `services/`: shared host services.
- `ConfigService`, `ThemeService`, `AudioService`, `PipeWireDeviceRegistry`, `EventBus`, `ActionRegistry`, `NotificationService`, `CompanionListenerService`, `IpcServer`.
- `YamlConfig`: schema-backed YAML config with defaults + deep merge.

### 3) Protocol Library (`libs/open-androidauto`)

In-tree static library implementing the AA protocol with a Qt-native approach. Protobuf definitions come from the [open-android-auto](https://github.com/mrmees/open-android-auto) community repo (included as a git submodule).

- Transport (`TCPTransport`, `ReplayTransport`)
- Framing/messenger/encryption
- Session state machine (`AASession`)
- Head-unit channel handlers (`oaa::hu::*`) for video, audio, AV input, input, sensor, Bluetooth, WiFi, navigation, media status, phone status
- Proto generation from `libs/open-androidauto/proto/open-android-auto/proto/oaa/*.proto`

### 4) Plugin Layer (`src/plugins`)

- `android_auto`: projection lifecycle, activation/deactivation hooks, touch integration, AA focus controls.
- `bt_audio`: BlueZ D-Bus monitoring for media transport/player state and controls.
- `phone`: BlueZ D-Bus monitoring for HFP device/call state and incoming-call UI integration.

### 5) UI Layer (`qml/` + `src/ui`)

- QML shell and app views are packaged via `qt_add_qml_module`.
- `src/ui` models/controllers bridge plugin/runtime state to QML:
- `PluginModel`, `PluginViewHost`, `PluginRuntimeContext`, `LauncherModel`, `AudioDeviceModel`, `CodecCapabilityModel`, `NotificationModel`, `ApplicationController`.

### 6) Web Config Path (`web-config/`)

- Flask app (`web-config/server.py`) serves dashboard/settings pages.
- Requests are forwarded to Qt over local socket (`/tmp/openauto-prodigy.sock` by default).
- Qt `IpcServer` is the single writer for runtime config/theme/audio state changes.

## Runtime Data Flow

Wireless AA session path (high-level):

1. Phone discovers head unit over BT/WiFi workflow.
2. Phone connects to HU TCP port.
3. `AndroidAutoOrchestrator` creates `oaa::AASession` and registers channel handlers.
4. Video/audio/input/sensor events flow through handlers into app services.
5. Video frames decode in `VideoDecoder`, then render via QML `VideoOutput`.
6. Audio packets route through `AudioService` into PipeWire streams.
7. Plugin/UI state updates propagate via Q_PROPERTY signals and event bus publications.

## Repository Layout

```text
src/
  main.cpp
  core/
    aa/
    plugin/
    services/
    YamlConfig.*
  plugins/
    android_auto/
    bt_audio/
    phone/
  ui/
qml/
  applications/
  components/
  controls/
libs/
  open-androidauto/
web-config/
tests/
docs/
```

## Build and Test

For full platform setup, see `docs/development.md`.

Quick build:

```bash
git clone --recurse-submodules https://github.com/mrmees/openauto-prodigy.git
cd openauto-prodigy
mkdir -p build
cd build
cmake ..
cmake --build . -j"$(nproc)"
ctest --output-on-failure
```

## Documentation Map

- `docs/INDEX.md` - doc index
- `docs/development.md` - build/dev environments
- `docs/design-decisions.md` - rationale and tradeoffs
- `docs/config-schema.md` - YAML schema and keys
- `docs/plugin-api.md` - plugin contract and host services

## License

GNU General Public License v3.0 — see `LICENSE`.
