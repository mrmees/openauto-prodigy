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
| `value(key)` | Main thread | Read a config value by dotted key (e.g. `"display.width"`) |
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
| `app.home` | Return to launcher (deactivate current plugin) |
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

## Static vs Dynamic Plugins

**Static plugins** are compiled into the binary and registered in `main.cpp` via `PluginManager::registerStaticPlugin()`.

**Dynamic plugins** are loaded from `~/.openauto/plugins/<plugin-id>/`:
- Must contain `plugin.yaml` manifest
- Must contain a shared library (`.so`) exporting `IPlugin*`
- Discovered by `PluginManager::discoverPlugins()`

Dynamic plugin loading is implemented but untested. Prefer static plugins for now.
