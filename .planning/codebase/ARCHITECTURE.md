# Architecture

**Analysis Date:** 2025-03-01

## Pattern Overview

**Overall:** Multi-layered desktop application with a plugin-based architecture. Core infrastructure (configuration, services, UI shell) is separated from domain-specific plugins (Android Auto, Bluetooth Audio, Phone) via an abstract plugin interface. Qt 6 provides both the UI framework (QML) and the application event loop.

**Key Characteristics:**
- Plugin-based extensibility with static (compiled-in) and dynamic (user-loaded) plugins
- Service-oriented architecture with shared infrastructure via IHostContext
- Asynchronous protocol handling (Android Auto via open-androidauto library) bridged to Qt's main event loop
- YAML-based configuration with deep-merge support for cascading defaults
- Qt/QML for the UI, C++17 for core logic
- Qt signals/slots for inter-component communication

## Layers

**Presentation Layer:**
- Purpose: User interface — QML UI shell, plugin views, overlays, settings
- Location: `qml/`, `src/ui/`
- Contains: QML components (Shell, TopBar, NavStrip), QAbstractListModel subclasses (PluginModel, LauncherModel, AudioDeviceModel), ApplicationController
- Depends on: Core services, PluginModel, ApplicationController, Theme/Audio/Bluetooth services
- Used by: User interaction, main.qml entry point

**Plugin System Layer:**
- Purpose: Dynamic plugin loading, initialization, activation/deactivation lifecycle
- Location: `src/core/plugin/`
- Contains: IPlugin interface, PluginManager (orchestrator), PluginManifest, PluginLoader, PluginDiscovery, HostContext (service accessor)
- Depends on: Core services (Config, Theme, Audio, Bluetooth), event bus
- Used by: main.cpp registration, application startup/shutdown

**Core Services Layer:**
- Purpose: Shared infrastructure and cross-cutting concerns
- Location: `src/core/services/`
- Contains: ConfigService (YAML management), ThemeService (day/night colors), AudioService (PipeWire), IpcServer (web config), EventBus, ActionRegistry, NotificationService, BluetoothManager, CompanionListenerService, SystemServiceClient
- Depends on: Configuration/YamlConfig, external libraries (PipeWire, yaml-cpp, Qt D-Bus)
- Used by: All plugins, main app, IPC clients

**Domain-Specific Plugin Layer:**
- Purpose: Feature implementations (Android Auto, Bluetooth audio, phone)
- Location: `src/plugins/`
- Contains: AndroidAutoPlugin, BtAudioPlugin, PhonePlugin (all implement IPlugin)
- Depends on: IHostContext, their own domain services (AndroidAutoOrchestrator, BlueZ D-Bus)
- Used by: Shell (via PluginModel), user navigation

**Android Auto Protocol Layer:**
- Purpose: AA protocol implementation and HU-side handlers
- Location: `src/core/aa/`, `libs/open-androidauto/`
- Contains: AndroidAutoOrchestrator (TCP server, session orchestration), VideoDecoder (FFmpeg), TouchHandler, EvdevTouchReader, NightModeProvider, channel handlers (video, audio, input, sensor, etc.)
- Depends on: open-androidauto library (AASession, Transport, Messenger), FFmpeg, AudioService, Configuration
- Used by: AndroidAutoPlugin

**Configuration & Data Layer:**
- Purpose: Settings persistence and state management
- Location: `src/core/`
- Contains: Configuration (legacy INI), YamlConfig (modern YAML), InputDeviceScanner
- Depends on: yaml-cpp, Qt I/O
- Used by: All services and plugins

**Audio Pipeline:**
- Purpose: PipeWire integration, device management, audio routing
- Location: `src/core/audio/`, `src/core/services/AudioService.hpp`
- Contains: PipeWireDeviceRegistry, AudioService, audio ring buffers
- Depends on: PipeWire, Qt
- Used by: AndroidAutoOrchestrator (AA audio streams), BtAudioPlugin (A2DP)

**Bluetooth Integration:**
- Purpose: BlueZ D-Bus communication for device discovery, pairing, A2DP/HFP
- Location: `src/core/services/BluetoothManager.hpp`, `src/core/bt/`
- Contains: BluetoothManager (device management, pairing), PairedDevicesModel, BT channel handlers
- Depends on: Qt D-Bus, BlueZ
- Used by: BtAudioPlugin, PhonePlugin, AndroidAutoPlugin (SDP registration)

## Data Flow

**Application Startup:**

1. `main.cpp` initializes Qt application
2. Load YAML config (`~/.openauto/config.yaml`) + fallback to legacy INI
3. Create core services: ConfigService, ThemeService, AudioService, BluetoothManager, EventBus, ActionRegistry, NotificationService
4. Wrap services in IHostContext (service locator)
5. Register static plugins: AndroidAutoPlugin, BtAudioPlugin, PhonePlugin
6. Discover dynamic plugins from `~/.openauto/plugins/`
7. Call `pluginManager.initializeAll(hostContext)` — each plugin gets one-time setup
8. Start IPC server for web config panel
9. Start QML engine, load `qml/main.qml`
10. Wire PluginViewHost to QML's `pluginContentHost` Item
11. Show Shell with launcher (no plugin selected yet)

**User Navigation to Plugin:**

1. User taps plugin icon in NavStrip or launcher
2. NavStrip/launcher calls `PluginModel.setActivePlugin(pluginId)`
3. PluginModel deactivates current plugin (if any)
   - Calls `IPlugin.onDeactivated()` on old plugin
   - Destroys QML context from PluginRuntimeContext
4. PluginModel activates new plugin
   - Calls `IPlugin.onActivated(childContext)` on new plugin
   - Plugin exposes C++ objects to QML context
5. PluginViewHost loads plugin's QML component into `pluginContentHost`
6. Plugin view is displayed; Shell hides status bar/nav if plugin requests fullscreen

**Android Auto Connection Flow:**

1. Phone discovers Pi via Bluetooth (SDP record registered by AndroidAutoPlugin.initialize())
2. Phone initiates WiFi connection to Pi's AP
3. Phone connects TCP to Pi's `AndroidAutoOrchestrator` server (listening on configurable port)
4. `AndroidAutoOrchestrator` creates oaa::AASession + TCPTransport
5. Session initializes channel handlers (Video, Audio, Input, Sensor, etc.)
6. Video frames → VideoDecoder (FFmpeg worker thread) → QVideoFrame → QML VideoOutput
7. Touch events → EvdevTouchReader (QThread reads /dev/input) → TouchHandler → AA InputEventIndication
8. Audio streams → PipeWire (3 streams: media, navigation, phone)
9. Disconnection → AndroidAutoOrchestrator sends ShutdownRequest, tears down session, returns to listening state

**State Management:**

- **Plugin Activation State:** Maintained by PluginModel (activePluginId_), driven by user navigation
- **AA Connection State:** Maintained by AndroidAutoOrchestrator (state_ enum: Disconnected/WaitingForDevice/Connecting/Connected/Backgrounded), emitted via Q_PROPERTY
- **Theme State:** Maintained by ThemeService (nightMode property), persisted to YAML config
- **Audio State:** Maintained by AudioService (current output/input device, volume), synced to YAML on change
- **Bluetooth Pairing:** Maintained by BluetoothManager (via BlueZ D-Bus), cached in PairedDevicesModel

## Key Abstractions

**IPlugin Interface:**
- Purpose: Define the plugin contract (identity, lifecycle, UI, capabilities)
- Examples: `src/plugins/android_auto/AndroidAutoPlugin.hpp`, `src/plugins/bt_audio/BtAudioPlugin.hpp`, `src/plugins/phone/PhonePlugin.hpp`
- Pattern: Abstract base class with lifecycle hooks (initialize/shutdown/onActivated/onDeactivated) and metadata queries (id/name/version/qmlComponent)

**IHostContext (Service Locator):**
- Purpose: Provide plugins access to shared services without hard dependencies
- Examples: `src/core/plugin/IHostContext.hpp`, `src/core/plugin/HostContext.cpp`
- Pattern: Interface with virtual getters for each service (audioService(), bluetoothService(), configService(), etc.)

**PluginRuntimeContext:**
- Purpose: Scope plugin's QML bindings to a child context, isolated from other plugins
- Examples: `src/ui/PluginRuntimeContext.hpp`
- Pattern: Ephemeral context created on plugin activation, destroyed on deactivation

**YamlConfig (Configuration Facade):**
- Purpose: Centralized YAML config management with typed accessors for all settings
- Examples: `src/core/YamlConfig.hpp`
- Pattern: Getter/setter pairs for each config key (wifiSsid(), setWifiSsid(), etc.), internal YAML node tree

**AndroidAutoOrchestrator (AA Protocol Orchestrator):**
- Purpose: Manage the full AA protocol lifecycle (TCP listening, session creation, channel handlers)
- Examples: `src/core/aa/AndroidAutoOrchestrator.hpp`
- Pattern: QObject with connection state machine, delegates to open-androidauto library for protocol

**PluginViewHost (QML-C++ Bridge):**
- Purpose: Load plugin QML components into the main shell's content area
- Examples: `src/ui/PluginViewHost.hpp`
- Pattern: C++ object that creates/destroys QML components, reparents them to the host Item

## Entry Points

**Main Application Executable:**
- Location: `src/main.cpp`
- Triggers: User runs `openauto-prodigy` binary
- Responsibilities: Qt setup, service initialization, plugin loading, QML engine startup, event loop

**Launcher Menu (Built-in Screen):**
- Location: `qml/applications/launcher/LauncherMenu.qml`
- Triggers: App starts, no plugin selected; user navigates away from settings
- Responsibilities: List available plugins, allow user to tap icons to activate plugins or settings

**Settings Menu (Built-in Screen):**
- Location: `qml/applications/settings/SettingsMenu.qml`
- Triggers: User taps gear icon in nav strip; ApplicationController.navigateTo(ApplicationTypes::Settings)
- Responsibilities: Audio, video, display, connection, system, companion, about settings

**Android Auto Plugin:**
- Location: `src/plugins/android_auto/AndroidAutoPlugin.cpp`
- Triggers: User taps Android Auto icon in launcher; AndroidAutoPlugin::initialize() called at startup
- Responsibilities: TCP server, AA session orchestration, video/touch/audio handling

**Bluetooth Audio Plugin:**
- Location: `src/plugins/bt_audio/BtAudioPlugin.cpp`
- Triggers: User taps Bluetooth Audio icon in launcher
- Responsibilities: A2DP connection monitoring, AVRCP metadata display and playback control

**Phone Plugin:**
- Location: `src/plugins/phone/PhonePlugin.cpp`
- Triggers: User taps Phone icon in launcher
- Responsibilities: HFP dialer, incoming call detection via BlueZ D-Bus

## Error Handling

**Strategy:** Multi-layered error handling with graceful degradation.

**Patterns:**

- **Config loading failures:** Fall back to built-in defaults or legacy INI format. Missing config keys use hardcoded defaults in getters.
- **Plugin initialization failures:** Plugin.initialize() returns bool; failed plugins are logged but don't crash the app. They remain disabled.
- **Service failures:** Services are optional. If a service can't be initialized (e.g., PipeWire unavailable), audio features degrade but app continues.
- **AA protocol errors:** Session disconnections are caught, logged, and orchestrator returns to "WaitingForDevice" state without crashing.
- **QML loading errors:** QML syntax errors are logged to stderr; missing components don't stop the app.
- **BlueZ D-Bus errors:** BluetoothManager retries with backoff; if BlueZ is unavailable, Bluetooth features are disabled but app runs.

Exception handling is minimal (C++ exceptions not widely used in Qt code). Errors are communicated via:
- Return values (bool) for initialization/startup operations
- Signals (connectionStateChanged) for state changes
- Logging (qInfo/qWarning/qCritical) for diagnostics

## Cross-Cutting Concerns

**Logging:** Qt logging (qDebug/qInfo/qWarning/qCritical) to stderr and optional file log via `QLoggingCategory`.

**Validation:** Configuration values are validated at load time (YAML parsing errors caught). User input in UI is validated in QML or C++ setters.

**Authentication:** No built-in auth. Companion pairing uses a shared secret (`~/.openauto/companion.key`). AA relies on Bluetooth/WiFi discovery.

**Concurrency:** ASIO threads (from open-androidauto) handle protocol; Qt main thread handles UI. Bridges via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`. Audio I/O runs in PipeWire threads.

---

*Architecture analysis: 2025-03-01*
