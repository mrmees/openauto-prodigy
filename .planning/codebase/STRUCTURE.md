# Codebase Structure

**Analysis Date:** 2025-03-01

## Directory Layout

```
openauto-prodigy/
├── src/                          # C++ source code
│   ├── main.cpp                  # Entry point (app init, services, plugin registration)
│   ├── core/                     # Core infrastructure
│   │   ├── Configuration.hpp/cpp # Legacy INI config (deprecated, kept for compatibility)
│   │   ├── YamlConfig.hpp/cpp    # YAML config (modern, single source of truth)
│   │   ├── InputDeviceScanner.hpp/cpp  # Auto-detect input devices (touch screens)
│   │   ├── plugin/               # Plugin system infrastructure
│   │   │   ├── IPlugin.hpp       # Plugin interface (abstract)
│   │   │   ├── IHostContext.hpp  # Service locator interface
│   │   │   ├── HostContext.hpp/cpp  # Service locator implementation
│   │   │   ├── PluginManager.hpp/cpp # Plugin lifecycle orchestrator
│   │   │   ├── PluginManifest.hpp/cpp # Plugin metadata
│   │   │   ├── PluginDiscovery.hpp/cpp # Scan disk for plugins
│   │   │   └── PluginLoader.hpp/cpp # Load .so and extract metadata
│   │   ├── services/             # Shared services (via IHostContext)
│   │   │   ├── ConfigService.hpp/cpp       # YAML config access
│   │   │   ├── ThemeService.hpp/cpp        # Day/night theme colors
│   │   │   ├── IAudioService.hpp/cpp       # Audio interface
│   │   │   ├── AudioService.hpp/cpp        # PipeWire integration
│   │   │   ├── IBluetoothService.hpp/cpp   # Bluetooth interface
│   │   │   ├── BluetoothManager.hpp/cpp    # BlueZ D-Bus Adapter/Device management
│   │   │   ├── IEventBus.hpp/cpp           # Cross-app event publisher
│   │   │   ├── ActionRegistry.hpp/cpp      # Named action dispatch
│   │   │   ├── NotificationService.hpp/cpp # Toast notifications
│   │   │   ├── IpcServer.hpp/cpp           # Unix socket server (web config)
│   │   │   ├── CompanionListenerService.hpp/cpp # TCP server for companion app
│   │   │   ├── SystemServiceClient.hpp/cpp # IPC client to openauto-system daemon
│   │   │   └── IThemeService.hpp           # Theme interface
│   │   ├── audio/                # Audio device management
│   │   │   └── PipeWireDeviceRegistry.hpp/cpp # Hot-plug device enumeration
│   │   ├── aa/                   # Android Auto protocol implementation
│   │   │   ├── AndroidAutoOrchestrator.hpp/cpp # AA orchestrator (TCP, session, channels)
│   │   │   ├── VideoDecoder.hpp/cpp           # FFmpeg multi-codec decode
│   │   │   ├── VideoFramePool.hpp/cpp         # QVideoFrame allocation pool
│   │   │   ├── DmaBufVideoBuffer.hpp/cpp      # Zero-copy Qt 6.8 video buffer
│   │   │   ├── TouchHandler.hpp/cpp           # Touch event → AA InputEventIndication
│   │   │   ├── EvdevTouchReader.hpp/cpp       # Direct evdev multi-touch input
│   │   │   ├── NightModeProvider.hpp/cpp      # Night mode abstraction
│   │   │   ├── TimedNightMode.hpp/cpp        # Time-based night mode
│   │   │   ├── ThemeNightMode.hpp/cpp        # Theme-based night mode
│   │   │   ├── GpioNightMode.hpp/cpp         # GPIO-based night mode
│   │   │   ├── CodecCapability.hpp/cpp       # Video codec capabilities
│   │   │   ├── ServiceDiscoveryBuilder.hpp/cpp # AA SDP record building
│   │   │   └── PerfStats.hpp                 # Performance diagnostics
│   │   ├── bt/                   # Bluetooth (future use, mostly in plugins)
│   │   ├── hw/                   # Hardware abstractions (future)
│   │   ├── system/               # System integration (future)
│   │   ├── mirror/               # Screen mirroring (future)
│   │   ├── obd/                  # OBD-II integration (future)
│   │   └── api/                  # External API client stubs (future)
│   ├── plugins/                  # Static (compiled-in) plugins
│   │   ├── android_auto/         # Android Auto plugin
│   │   │   ├── AndroidAutoPlugin.hpp/cpp
│   │   │   └── [QML: AndroidAutoMenu.qml, Sidebar.qml]
│   │   ├── bt_audio/             # Bluetooth Audio (A2DP/AVRCP)
│   │   │   ├── BtAudioPlugin.hpp/cpp
│   │   │   └── [QML: BtAudioView.qml]
│   │   └── phone/                # Phone (HFP dialer)
│   │       ├── PhonePlugin.hpp/cpp
│   │       └── [QML: PhoneView.qml]
│   └── ui/                       # Qt controller layer (bridges C++ ↔ QML)
│       ├── ApplicationController.hpp/cpp     # Screen/app navigation (deprecated, settings only)
│       ├── PluginModel.hpp/cpp               # QAbstractListModel for nav strip
│       ├── PluginRuntimeContext.hpp/cpp      # Scoped QML context per plugin
│       ├── PluginViewHost.hpp/cpp            # Load QML components into Shell
│       ├── LauncherModel.hpp/cpp             # Tile grid model
│       ├── AudioDeviceModel.hpp/cpp          # Output/input device ComboBox model
│       ├── CodecCapabilityModel.hpp/cpp      # Codec selection model
│       ├── PairedDevicesModel.hpp/cpp        # Bluetooth paired devices
│       └── NotificationModel.hpp/cpp         # Toast notification queue
│
├── qml/                          # Qt QML UI
│   ├── main.qml                  # Root window
│   ├── components/               # Shell components (not plugin-specific)
│   │   ├── Shell.qml             # Main layout (TopBar + Content + NavStrip)
│   │   ├── TopBar.qml            # Status bar (clock, signal, battery stub)
│   │   ├── BottomBar.qml         # Bottom bar (unused, legacy)
│   │   ├── NavStrip.qml          # Plugin nav icons
│   │   ├── Clock.qml             # Digital clock display
│   │   ├── Wallpaper.qml         # Background
│   │   ├── GestureOverlay.qml    # 3-finger tap overlay
│   │   ├── NotificationArea.qml  # Toast notifications
│   │   ├── PairingDialog.qml     # Bluetooth pairing confirmation
│   │   ├── FirstRunBanner.qml    # "No devices paired" banner on launcher
│   │   └── ExitDialog.qml        # Quit confirmation
│   ├── controls/                 # Reusable UI controls
│   │   ├── NormalText.qml        # Regular body text
│   │   ├── SpecialText.qml       # Highlight/accent text
│   │   ├── Icon.qml              # Image-based icon
│   │   ├── MaterialIcon.qml      # Material Design icon (font-based)
│   │   ├── Tile.qml              # Clickable tile (launcher, settings)
│   │   ├── SettingsToggle.qml    # On/off switch
│   │   ├── SettingsSlider.qml    # Slider for numeric settings
│   │   ├── SettingsListItem.qml  # List item in settings
│   │   ├── SectionHeader.qml     # Section title in settings
│   │   ├── ReadOnlyField.qml     # Read-only text display
│   │   ├── InfoBanner.qml        # Info message banner
│   │   ├── SegmentedButton.qml   # Tab-like buttons
│   │   ├── FullScreenPicker.qml  # Modal picker (resolution, codec)
│   │   └── UiMetrics.qml         # Singleton with responsive sizing
│   └── applications/             # Plugin-specific views + built-in screens
│       ├── launcher/
│       │   └── LauncherMenu.qml  # Plugin tile grid
│       ├── android_auto/         # AA plugin views
│       │   ├── AndroidAutoMenu.qml  # Fullscreen AA video + sidebar
│       │   └── Sidebar.qml          # Quick controls overlay (sidebar)
│       ├── bt_audio/
│       │   └── BtAudioView.qml      # Track metadata + playback controls
│       ├── phone/
│       │   ├── PhoneView.qml        # Dialer interface
│       │   └── IncomingCallOverlay.qml # Incoming call notification
│       ├── home/
│       │   └── HomeMenu.qml         # Home screen (stub, future)
│       └── settings/             # Built-in settings screens (not plugins)
│           ├── SettingsMenu.qml
│           ├── AudioSettings.qml     # Speaker, mic device selection
│           ├── VideoSettings.qml     # Resolution, codec, FPS
│           ├── DisplaySettings.qml   # Brightness, theme, sidebar
│           ├── ConnectionSettings.qml # WiFi SSID/password, TCP port
│           ├── SystemSettings.qml    # Device name, model, time
│           ├── CompanionSettings.qml # Companion app pairing
│           └── AboutSettings.qml     # Version, build info
│
├── libs/
│   ├── open-androidauto/         # AA protocol library (git submodule, DO NOT EDIT)
│   │   ├── include/oaa/
│   │   │   ├── Session/AASession.hpp      # Protocol state machine
│   │   │   ├── Transport/TCPTransport.hpp # TCP transport
│   │   │   ├── Messenger/ProtocolLogger.hpp # Protocol debugging
│   │   │   └── HU/Handlers/               # Channel handlers (video, audio, input, etc.)
│   │   ├── src/
│   │   ├── proto/                # Android Auto .proto definitions (git submodule)
│   │   └── tests/
│   └── qrcodegen/                # QR code generation (single file, MIT licensed)
│
├── tests/                        # Unit tests (47 tests)
│   ├── test_*.cpp                # Individual test files
│   │   ├── test_configuration.cpp # Config loading
│   │   ├── test_yaml_config.cpp  # YAML parsing
│   │   ├── test_plugin_manager.cpp # Plugin lifecycle
│   │   ├── test_theme_service.cpp # Theme loading
│   │   ├── test_audio_service.cpp # Audio device enumeration
│   │   ├── test_aa_orchestrator.cpp # AA orchestration
│   │   ├── test_video_frame_pool.cpp # Video frame allocation
│   │   ├── test_*_channel_handler.cpp # AA protocol channels
│   │   └── ... (40+ more tests)
│   ├── data/                     # Test fixtures
│   │   ├── test_config.yaml
│   │   ├── test_config.ini
│   │   ├── test_plugin.yaml
│   │   └── themes/default/       # Test theme files
│   └── CMakeLists.txt            # Test build configuration
│
├── config/
│   └── themes/default/           # Default theme colors
│       └── theme.yaml            # Day/night color palette
│
├── resources/
│   ├── icons/                    # Application icons (SVG, PNG)
│   ├── fonts/                    # Bundled fonts (Lato)
│   └── resources.qrc             # Qt resource file (icons, fonts, QML)
│
├── web-config/                   # Flask web config panel (separate server)
│   ├── server.py                 # Flask app + IPC client
│   └── templates/                # HTML/CSS/JS for settings web UI
│
├── system-service/               # openauto-system daemon (separate binary)
│   ├── server.py                 # System-level service (WiFi AP, hostapd, etc.)
│   ├── templates/                # systemd templates, scripts
│   └── tests/
│
├── docker/                       # Docker dev environment configs
├── systemd/                      # systemd service files
├── docs/                         # Design docs and guides
│   ├── project-vision.md
│   ├── design-philosophy.md
│   ├── development.md            # Build & dependency guide
│   ├── roadmap-current.md        # Current status & priorities
│   ├── session-handoffs.md       # Development session log
│   ├── wishlist.md               # Future feature ideas
│   ├── INDEX.md                  # Docs index
│   ├── aa-protocol/              # AA protocol docs
│   │   ├── *-gotchas.md
│   │   ├── video-resolutions.md
│   │   └── ...
│   ├── pi-config/                # RPi-specific setup guides
│   └── OpenAutoPro_archive_information/ # Reverse-engineered OAP docs
│
├── CMakeLists.txt                # Top-level CMake (Qt, protobuf, submodules)
├── toolchain-pi4.cmake           # Cross-compilation toolchain (aarch64 ARM)
└── install.sh                    # Interactive RPi installer (Bash)
```

## Directory Purposes

**src/:**
- All C++ source code (application logic, not UI)

**src/core/:**
- Foundation: configuration, services, plugin infrastructure

**src/core/plugin/:**
- Plugin system: abstractions (IPlugin), orchestration (PluginManager), discovery/loading

**src/core/services/:**
- Shared infrastructure: Config, Theme, Audio, Bluetooth, IPC, Events, Actions, Notifications

**src/core/aa/:**
- Android Auto protocol: orchestration, video decode, touch input, night mode, SDP discovery

**src/core/audio/:**
- Audio device enumeration and PipeWire integration

**src/plugins/:**
- Compiled-in plugins: AndroidAuto, BtAudio, Phone (all implement IPlugin interface)

**src/ui/:**
- Qt controller layer: adapts C++ models/services for QML consumption (PluginModel, LauncherModel, etc.)

**qml/main.qml:**
- Entry point for QML engine; creates root Window

**qml/components/:**
- Shell components: TopBar, BottomBar, NavStrip, overlays, dialogs (not plugin-specific)

**qml/controls/:**
- Reusable UI primitives: buttons, toggles, text controls, icon controls

**qml/applications/:**
- Plugin views and built-in screens (launcher, settings)

**libs/open-androidauto/:**
- Android Auto protocol library (git submodule, external — do not edit in this repo)

**tests/:**
- Unit tests for all major components (config, plugins, services, AA protocol)

**config/themes/:**
- Default theme color palette (YAML)

**resources/:**
- Application assets (icons, fonts, resources.qrc for Qt)

**web-config/:**
- Separate Flask server for web-based settings (communicates via Unix socket IPC)

**system-service/:**
- Separate daemon for system-level operations (WiFi AP, hostapd, system events)

**docs/:**
- Architecture docs, development guides, protocol reference

## Key File Locations

**Entry Points:**
- `src/main.cpp`: Application entry point — initializes Qt, loads services, starts event loop
- `qml/main.qml`: QML root — creates Window, loads Shell component
- `qml/components/Shell.qml`: Main layout shell — TopBar, plugin content area, NavStrip
- `install.sh`: RPi installation script — sets up system, builds app

**Configuration:**
- `src/core/YamlConfig.hpp/cpp`: YAML config implementation (modern)
- `src/core/Configuration.hpp/cpp`: Legacy INI config (backward compatibility)
- `config/themes/default/theme.yaml`: Default day/night color palette
- `~/.openauto/config.yaml`: User config file (created at runtime)

**Core Logic:**
- `src/core/plugin/PluginManager.hpp/cpp`: Plugin lifecycle orchestrator
- `src/core/plugin/IPlugin.hpp`: Plugin interface (abstract base)
- `src/core/services/ConfigService.hpp/cpp`: YAML config service
- `src/core/aa/AndroidAutoOrchestrator.hpp/cpp`: AA protocol orchestration
- `src/core/services/AudioService.hpp/cpp`: PipeWire audio integration
- `src/core/services/BluetoothManager.hpp/cpp`: BlueZ D-Bus device management

**Testing:**
- `tests/`: All unit tests (47 tests across config, plugins, services, AA protocol)
- `tests/data/`: Test fixtures (YAML files, theme files)
- `tests/CMakeLists.txt`: Test build configuration

**Plugin Implementations:**
- `src/plugins/android_auto/AndroidAutoPlugin.hpp/cpp`: AA plugin
- `src/plugins/bt_audio/BtAudioPlugin.hpp/cpp`: Bluetooth Audio plugin
- `src/plugins/phone/PhonePlugin.hpp/cpp`: Phone plugin

**UI Controllers:**
- `src/ui/PluginModel.hpp/cpp`: Nav strip model (QAbstractListModel)
- `src/ui/PluginViewHost.hpp/cpp`: QML component loader/host
- `src/ui/ApplicationController.hpp/cpp`: Built-in screen navigation (settings)

## Naming Conventions

**Files:**
- C++ headers: `.hpp` (not `.h`)
- C++ sources: `.cpp`
- QML components: `.qml`
- Test files: `test_*.cpp`
- Configuration: `*.yaml` (YAML), `*.ini` (legacy INI)

**Directories:**
- Domain modules under `src/core/`: singular (e.g., `aa/`, `audio/`, `bt/`)
- Plugin modules under `src/plugins/`: singular (e.g., `android_auto/`, `bt_audio/`)
- QML application views: descriptor + plural (e.g., `applications/launcher/`, `applications/settings/`)
- QML components: `components/` (shared UI shell)
- QML controls: `controls/` (reusable primitives)

**Classes:**
- Plugin classes: `[Name]Plugin` (e.g., `AndroidAutoPlugin`, `BtAudioPlugin`)
- Service classes: `[Name]Service` (e.g., `AudioService`, `ConfigService`)
- Manager classes: `[Name]Manager` (e.g., `PluginManager`, `BluetoothManager`)
- Handler classes: `[Name]Handler` (e.g., `VideoChannelHandler` in open-androidauto)
- Model classes: `[Name]Model` (e.g., `PluginModel`, `LauncherModel`)
- Orchestrator classes: `[Name]Orchestrator` (e.g., `AndroidAutoOrchestrator`)

**Interfaces:**
- Prefixed with `I` (e.g., `IPlugin`, `IHostContext`, `IAudioService`)

**Methods/Properties:**
- Snake_case for C++ members (e.g., `activePluginId_`, `connectionState_`)
- camelCase for public methods/slots (e.g., `setActivePlugin()`, `connectionState()`)
- Q_PROPERTY macros for bindable properties

**Signals:**
- camelCase with "Changed" suffix for property notifications (e.g., `activePluginChanged`)
- Descriptive names for action signals (e.g., `requestActivation()`, `connectionStateChanged()`)

## Where to Add New Code

**New Feature:**
- **Primary code:** Add to appropriate `src/core/` subdomain (e.g., `src/core/mirror/` for screen mirroring)
- **Plugin if needed:** Create under `src/plugins/[feature_name]/` and implement `IPlugin` interface
- **Tests:** Add to `tests/test_[feature].cpp`
- **UI:** Add QML to `qml/applications/[feature]/` if it has a user-facing view

**New Static Plugin:**
1. Create `src/plugins/[name]/[Name]Plugin.hpp/cpp`
2. Implement `IPlugin` interface (id, name, initialize, shutdown, onActivated, onDeactivated, qmlComponent, requiredServices, etc.)
3. Add plugin QML view to `qml/applications/[name]/View.qml`
4. Register in `src/main.cpp`: `pluginManager.registerStaticPlugin(new [Name]Plugin(&app))`
5. Add to `src/CMakeLists.txt` source list
6. Add unit test `tests/test_[name]_plugin.cpp`

**New Component/Module:**
- **If shared across plugins:** Place in `src/core/[domain]/` with clear interface
- **If plugin-specific:** Keep in `src/plugins/[plugin]/` subdirectory
- **Service access:** Receive via `IHostContext*` passed to `IPlugin::initialize()`

**New QML Control:**
- **Reusable across multiple views:** Add to `qml/controls/` with clear name (e.g., `CustomSwitch.qml`)
- **Plugin-specific:** Add to `qml/applications/[plugin]/`
- **Built-in screen:** Add to `qml/applications/settings/`

**New Service:**
- Create in `src/core/services/[Name]Service.hpp/cpp`
- Define interface as `I[Name]Service.hpp` if it's injectable
- Add getter/setter in `IHostContext` and `HostContext` if plugins need it
- Initialize in `src/main.cpp` before plugin initialization

**Utilities:**
- **Domain-specific:** Place with domain (e.g., audio ring buffer in `src/core/audio/`)
- **Shared helpers:** Create in `src/core/` as new file (e.g., `src/core/Helpers.hpp`)

## Special Directories

**libs/open-androidauto/:**
- Purpose: Android Auto protocol library (git submodule)
- Generated: No (checked into git)
- Committed: No (external submodule, managed separately)
- **DO NOT EDIT in this repo** — changes should be made in the separate `open-android-auto` repository

**libs/qrcodegen/:**
- Purpose: QR code generation for SDP records
- Generated: No
- Committed: Yes (single file, MIT licensed)

**config/themes/default/:**
- Purpose: Default color palette for day/night modes
- Generated: No
- Committed: Yes

**~/.openauto/ (runtime, not in repo):**
- `config.yaml`: User configuration (created on first run or migrated from INI)
- `companion.key`: Companion app shared secret (generated on first pairing)
- `plugins/`: User-installed dynamic plugins (`.so` + `plugin.yaml` manifest)
- `themes/`: User-installed custom themes (theme.yaml)

**.planning/codebase/ (documentation):**
- Purpose: Generated analysis documents (ARCHITECTURE.md, STRUCTURE.md, etc.)
- Generated: Yes (by GSD tools)
- Committed: Yes (reference docs)

---

*Structure analysis: 2025-03-01*
