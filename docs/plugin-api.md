# Plugin API Reference

## Overview

OpenAuto Prodigy uses a plugin-based architecture. Plugins implement the `IPlugin` interface and are managed by `PluginManager`. Plugins access shared services through `IHostContext`.

## IPlugin Interface

All plugins must implement `src/core/plugin/IPlugin.hpp`.

### Identity

| Method | Return | Description |
|--------|--------|-------------|
| `id()` | `QString` | Unique reverse-DNS identifier (e.g. `org.openauto.android-auto`) |
| `name()` | `QString` | Human-readable display name |
| `version()` | `QString` | SemVer version string |
| `apiVersion()` | `int` | Plugin API version (currently `1`) |

### Lifecycle

| Method | Thread | Description |
|--------|--------|-------------|
| `initialize(IHostContext*)` | Main | One-time setup. Store the context pointer. Return `false` to disable. |
| `shutdown()` | Main | Final cleanup. Called once before unload. |
| `onActivated(QQmlContext*)` | Main | Plugin became visible. Expose bindings to the child QML context. |
| `onDeactivated()` | Main | Plugin hidden. Child context is destroyed by PluginRuntimeContext. |

**Lifecycle order:** `initialize()` → [`onActivated()` ↔ `onDeactivated()`]* → `shutdown()`

**Ownership:** The `QQmlContext*` passed to `onActivated()` is owned by `PluginRuntimeContext` — do not delete it. The `IHostContext*` pointer is valid from `initialize()` until `shutdown()` returns.

### UI

| Method | Return | Description |
|--------|--------|-------------|
| `qmlComponent()` | `QUrl` | QRC URL of the plugin's main QML view. Empty = no view. |
| `iconSource()` | `QUrl` | Icon URL for nav strip. Empty = use font icon. |
| `settingsComponent()` | `QUrl` | QRC URL of settings panel. Empty = no settings. |

### Capabilities

| Method | Return | Description |
|--------|--------|-------------|
| `requiredServices()` | `QStringList` | Service IDs this plugin requires (reserved for future). |
| `wantsFullscreen()` | `bool` | `true` = hide nav strip and status bar when active. |

## IHostContext Services

Plugins receive an `IHostContext*` in `initialize()`. All service pointers are valid until `shutdown()`.

### ConfigService (`IConfigService`)

Read/write YAML configuration values.

| Method | Thread Safety | Description |
|--------|---------------|-------------|
| `value(key)` | Main thread | Read a config value by dotted key (e.g. `"display.brightness"`) |
| `setValue(key, value)` | Main thread | Write a config value |

**Stability:** Stable

### ThemeService (`IThemeService`)

Day/night theme colors. All properties are Q_PROPERTY bindings usable from QML.

| Property | Type | Description |
|----------|------|-------------|
| `nightMode` | `bool` | Current mode |
| `backgroundColor` | `QColor` | Current background color |
| `textColor` | `QColor` | Current text color |
| `accentColor` | `QColor` | Current accent color |

| Method | Thread Safety | Description |
|--------|---------------|-------------|
| `toggleMode()` | Main thread | Switch between day and night |

**Stability:** Stable

### AudioService (`IAudioService`)

PipeWire stream management.

| Method | Thread Safety | Description |
|--------|---------------|-------------|
| `createStream(name, format)` | Main thread | Create a PipeWire audio stream |
| `setMasterVolume(float)` | Main thread | Set master volume (0.0–1.0) |

**Stability:** Experimental — API may change.

### EventBus (`IEventBus`)

Pub/sub event system with string-keyed topics and QVariant payloads.

| Method | Thread Safety | Description |
|--------|---------------|-------------|
| `subscribe(topic, callback)` | Thread-safe | Subscribe to a topic. Returns subscription ID. |
| `unsubscribe(id)` | Thread-safe | Remove a subscription. |
| `publish(topic, payload)` | Thread-safe | Publish an event. Callbacks invoked on main thread. |

**Delivery:** All callbacks are invoked via `Qt::QueuedConnection` on the main thread.

**Stability:** Stable

### ActionRegistry

Named command dispatch. Actions are synchronous.

| Method | Thread Safety | Description |
|--------|---------------|-------------|
| `registerAction(id, handler)` | Main thread only | Register a named action handler. |
| `unregisterAction(id)` | Main thread only | Remove an action handler. |
| `dispatch(id, payload)` | Main thread only | Execute an action. Returns `false` if unknown. |
| `registeredActions()` | Main thread only | List all registered action IDs. |

**Built-in actions:**

| Action ID | Description |
|-----------|-------------|
| `app.quit` | Quit the application |
| `app.home` | Return to home (deactivate current plugin) |
| `theme.toggle` | Toggle day/night mode |

**Stability:** Stable

### NotificationService (`INotificationService`)

Post and dismiss notifications.

| Method | Thread Safety | Description |
|--------|---------------|-------------|
| `post(QVariantMap)` | Main thread | Post a notification. Returns notification ID. |
| `dismiss(id)` | Main thread | Remove a notification by ID. |

**Notification fields:**

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `kind` | `QString` | Yes | `"toast"`, `"incoming_call"`, `"status_icon"` |
| `message` | `QString` | Yes | Display text |
| `sourcePluginId` | `QString` | No | Plugin that posted it |
| `priority` | `int` | No | 0–100 (default 50). Higher = more prominent. |
| `ttlMs` | `int` | No | Auto-dismiss after N ms. 0 = persistent. |

**Stability:** Experimental

## Error Handling

- Plugin methods should not throw exceptions.
- Return `false` from `initialize()` to indicate failure — the plugin will be disabled.
- Service methods return default values (empty QVariant, false, etc.) on error.
- Use `IHostContext::log()` for diagnostic messages.

## Dashboard Contributions

Plugins can contribute widgets to the home screen dashboard via `widgetDescriptors()`.

### WidgetDescriptor

Each widget descriptor declares a dashboard contribution with typed metadata:

| Field | Type | Description |
|-------|------|-------------|
| `id` | `QString` | Unique widget identifier |
| `displayName` | `QString` | Human-readable name shown in picker |
| `iconName` | `QString` | Material Icon codepoint (e.g. `"\ue8b5"`) |
| `qmlComponent` | `QUrl` | QRC URL of the widget's QML file |
| `pluginId` | `QString` | Source plugin ID (set automatically) |
| `contributionKind` | `DashboardContributionKind` | `Widget` or `LiveSurfaceWidget` |
| `defaultConfig` | `QVariantMap` | Default per-instance config |
| `minCols`, `minRows` | `int` | Minimum grid size |
| `maxCols`, `maxRows` | `int` | Maximum grid size |
| `defaultCols`, `defaultRows` | `int` | Default grid size |

### Contribution Kinds

| Kind | Description | Status |
|------|-------------|--------|
| `Widget` | Lightweight data-display widget (clock, status, now playing) | Active |
| `LiveSurfaceWidget` | Full interactive surface (embedded AA pane, camera feed) | Declared but deferred — no runtime host exists yet |

`WidgetPickerModel` excludes `LiveSurfaceWidget` entries from the picker UI until a host path is implemented. Plugins may declare `LiveSurfaceWidget` descriptors now; they will become available when the runtime support ships.

### WidgetInstanceContext

Widget QML receives a `WidgetInstanceContext` with layout and provider properties:

| Property | Type | Description |
|----------|------|-------------|
| `cellWidth` | `int` | Pixel width of the widget's grid cell |
| `cellHeight` | `int` | Pixel height of the widget's grid cell |
| `instanceId` | `QString` | Unique instance identifier |
| `widgetId` | `QString` | Widget descriptor ID |
| `projectionStatus` | `QObject*` | `IProjectionStatusProvider` — projection connection state |
| `navigationProvider` | `QObject*` | `INavigationProvider` — nav data (road, maneuver, distance) |
| `mediaStatus` | `QObject*` | `IMediaStatusProvider` — media metadata and playback controls |

Provider properties are resolved from `IHostContext` and may be `null` if the corresponding service is not registered.

## Provider Interfaces

Shell, dashboard, and widget QML access cross-cutting state through narrow provider interfaces rather than concrete plugin/bridge types. This decouples the UI from specific plugin implementations.

| Interface | Root-Context Name | Backed By | Provides |
|-----------|-------------------|-----------|----------|
| `IProjectionStatusProvider` | `ProjectionStatus` | `ProjectionStatusProvider` → `AndroidAutoOrchestrator` | `projectionState`, `statusMessage` |
| `INavigationProvider` | `NavigationProvider` | `NavigationDataBridge` | `navActive`, `roadName`, `formattedDistance`, maneuver data |
| `IMediaStatusProvider` | `MediaStatus` | `MediaStatusService` | `title`, `artist`, `album`, `playbackState`, `source`, `playPause()`/`next()`/`previous()` |
| `ICallStateProvider` | `CallStateProvider` | `PhoneStateService` | `callState`, `callerName`, `callerNumber`, `answer()`/`hangup()` |

**Design rule:** Core platform owns singleton hardware/system state. Plugins are UI wrappers that read from core services, not state owners.

### Action Registry

The `aa.sendButton` action routes AA button presses through `ActionRegistry` instead of direct orchestrator calls:

```qml
ActionRegistry.dispatch("aa.sendButton", 85)  // play/pause
```

## Core Service Ownership

| Service | Owner | Role |
|---------|-------|------|
| `PhoneStateService` | Core | Owns HFP D-Bus monitoring + call state machine. Survives regardless of PhonePlugin activation. |
| `MediaStatusService` | Core | Owns AA + BT media source merging with priority logic. Survives regardless of BtAudioPlugin activation. |
| `NavigationDataBridge` | Core | Single-source AA navigation data. Implements `INavigationProvider` directly. |
| `AndroidAutoRuntimeBridge` | Core | Owns touch device detection, EvdevTouchReader lifecycle, EvdevCoordBridge, display dimension injection, navbar thickness. |
| `GestureOverlayController` | Core | Owns three-finger overlay zone registration, slider handling, volume/brightness dispatch. |

| Plugin | Role |
|--------|------|
| `PhonePlugin` | UI wrapper — reads state from `PhoneStateService`, exposes Q_PROPERTYs for PhoneView QML |
| `BtAudioPlugin` | UI wrapper — reads state from `MediaStatusService` for its QML view. Still owns A2DP transport lifecycle. |
| `AndroidAutoPlugin` | AA protocol lifecycle — orchestrator start/stop, QML context exposure, config change watching. Delegates platform policy to `AndroidAutoRuntimeBridge`. |

## Static vs Dynamic Plugins

**Static plugins** are compiled into the binary and registered in `main.cpp` via `PluginManager::registerStaticPlugin()`.

**Dynamic plugins** are loaded from `~/.openauto/plugins/<plugin-id>/`:
- Must contain `plugin.yaml` manifest
- Must contain a shared library (`.so`) exporting `IPlugin*`
- Discovered by `PluginManager::discoverPlugins()`

Dynamic plugin loading is implemented but untested. Prefer static plugins for now.
