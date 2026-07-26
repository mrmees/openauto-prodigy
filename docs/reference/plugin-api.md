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
| `apiVersion()` | `int` | Plugin API version (currently `2`). Bumped 1→2 in the B2 teardown (2026-07-14): the `IHostContext` vtable changed when `companionListenerService()` was removed. The host accepts a plugin only if its `apiVersion()` exactly matches — a stale v1 `.so` is rejected, not mis-dispatched. |

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
| `iconSource()` | `QUrl` | Optional image URL retained as plugin UI metadata. |
| `iconText()` | `QString` | Optional Material icon codepoint (for example `"\ue8b5"`) retained as plugin UI metadata. Default: empty. |
| `settingsComponent()` | `QUrl` | QRC URL of settings panel. Empty = no settings. |

### Capabilities

| Method | Return | Description |
|--------|--------|-------------|
| `requiredServices()` | `QStringList` | Service IDs this plugin requires (reserved for future). |
| `wantsFullscreen()` | `bool` | Requests the plugin's full application surface; the shell hides the navbar while it is active. |

## IHostContext Services

Plugins receive an `IHostContext*` in `initialize()`. All service pointers are valid until `shutdown()`.

The host context currently exposes:

| Accessor | Interface / Type |
|---|---|
| `audioService()` | `IAudioService` |
| `bluetoothService()` | `IBluetoothService` |
| `configService()` | `IConfigService` |
| `themeService()` | `IThemeService` |
| `displayService()` | `IDisplayService` |
| `eventBus()` | `IEventBus` |
| `actionRegistry()` | `ActionRegistry` |
| `notificationService()` | `INotificationService` |
| `equalizerService()` | `IEqualizerService` |
| `projectionStatusProvider()` | `IProjectionStatusProvider`, nullable |
| `navigationProvider()` | `INavigationProvider`, nullable |
| `mediaStatusProvider()` | `IMediaStatusProvider`, nullable |
| `callStateProvider()` | `ICallStateProvider`, nullable |
| `overlayService()` | `OverlayService`, nullable |

`log(level, message)` is the host-owned diagnostic path and is thread-safe.

**Note:** The tables below document the primary plugin-facing methods for each service. Some services expose additional internal methods (lifecycle, auto-connect, device management, etc.) that are used by core code. For the complete API surface, refer to the interface headers in `src/core/services/`.

### ConfigService (`IConfigService`)

Read/write YAML configuration values. Plugin-scoped methods isolate each plugin's config under its ID namespace.

| Method | Thread Safety | Description |
|--------|---------------|-------------|
| `value(key)` | Thread-safe | Read a top-level config value by dotted key (e.g. `"display.brightness"`) |
| `setValue(key, value)` | Main thread | Write a top-level config value |
| `pluginValue(pluginId, key)` | Thread-safe | Read a plugin-scoped config value |
| `setPluginValue(pluginId, key, value)` | Main thread | Write a plugin-scoped config value |
| `save()` | Main thread | Flush config to disk |

**Stability:** Stable

### ThemeService (`IThemeService`)

The plugin-facing C++ interface exposes `currentThemeId()`, `color(name)`,
`fontFamily()`, `iconPath(relativePath)`, `setTheme(themeId)`, and
`applyAATokens(dayTokens, nightTokens)`. Theme lookup methods are thread-safe;
theme mutation runs on the main thread.

The concrete `ThemeService` root-context object also exposes the following QML
properties. These are useful to plugin and widget QML, but they are not virtual
members of `IThemeService`. Colors switch automatically between day and night
palettes.

**Primary group:**

| Property | Type | Description |
|----------|------|-------------|
| `primary` | `QColor` | Brand accent color |
| `onPrimary` | `QColor` | Text/icons on primary |
| `primaryContainer` | `QColor` | Primary container fill |
| `onPrimaryContainer` | `QColor` | Text/icons on primary container |

**Secondary group:**

| Property | Type | Description |
|----------|------|-------------|
| `secondary` | `QColor` | Secondary accent |
| `onSecondary` | `QColor` | Text/icons on secondary |
| `secondaryContainer` | `QColor` | Secondary container fill |
| `onSecondaryContainer` | `QColor` | Text/icons on secondary container |

**Tertiary group:**

| Property | Type | Description |
|----------|------|-------------|
| `tertiary` | `QColor` | Tertiary accent |
| `onTertiary` | `QColor` | Text/icons on tertiary |
| `tertiaryContainer` | `QColor` | Tertiary container fill |
| `onTertiaryContainer` | `QColor` | Text/icons on tertiary container |

**Error group:**

| Property | Type | Description |
|----------|------|-------------|
| `error` | `QColor` | Error state color |
| `onError` | `QColor` | Text/icons on error |
| `errorContainer` | `QColor` | Error container fill |
| `onErrorContainer` | `QColor` | Text/icons on error container |

**Background and Surface:**

| Property | Type | Description |
|----------|------|-------------|
| `background` | `QColor` | App background |
| `onBackground` | `QColor` | Text on background |
| `surface` | `QColor` | General surface |
| `onSurface` | `QColor` | Primary text on surfaces |
| `surfaceVariant` | `QColor` | Variant surface |
| `onSurfaceVariant` | `QColor` | Secondary text, icons |
| `surfaceDim` | `QColor` | Dimmed surface |
| `surfaceBright` | `QColor` | Bright surface |
| `surfaceContainerLowest` | `QColor` | Lowest-elevation container |
| `surfaceContainerLow` | `QColor` | Low-elevation container |
| `surfaceContainer` | `QColor` | Default container (widget glass cards) |
| `surfaceContainerHigh` | `QColor` | High-elevation container |
| `surfaceContainerHighest` | `QColor` | Highest-elevation container |

**Outline:**

| Property | Type | Description |
|----------|------|-------------|
| `outline` | `QColor` | Borders, dividers |
| `outlineVariant` | `QColor` | Subtle borders |

**Inverse:**

| Property | Type | Description |
|----------|------|-------------|
| `inverseSurface` | `QColor` | High-contrast surface |
| `inverseOnSurface` | `QColor` | Text on inverse surface |
| `inversePrimary` | `QColor` | Primary on inverse surface |

**Utility and Derived:**

| Property | Type | Description |
|----------|------|-------------|
| `scrim` | `QColor` | Overlay scrim |
| `shadow` | `QColor` | Shadow color |
| `success` | `QColor` | Success state (derived, computed) |
| `onSuccess` | `QColor` | Text on success (derived, computed) |
| `warning` | `QColor` | Warning state (derived, computed) |
| `onWarning` | `QColor` | Text on warning (derived, computed) |
| `surfaceTintHigh` | `QColor` | Derived high-elevation tinted surface. |
| `surfaceTintHighest` | `QColor` | Derived highest-elevation tinted surface. |

**Mode control:**

| Property | Type | Description |
|----------|------|-------------|
| `nightMode` | `bool` | Current display mode (read/write). When `forceDarkMode` is on, the UI always shows dark but `nightMode` still tracks the real sensor/time state. |
| `forceDarkMode` | `bool` | HU override — forces dark palette for the UI. Default: on. |

Use `realNightMode()` (C++ only) to get the actual day/night state for AA sensor reporting, independent of `forceDarkMode`.

| Method | Thread Safety | Description |
|--------|---------------|-------------|
| `toggleMode()` | Main thread | Switch between day and night |
| `setTheme(themeId)` | Main thread | Switch to a different theme by ID |

**Stability:** Stable

### AudioService (`IAudioService`)

PipeWire stream management with audio focus and device selection.

| Method | Thread Safety | Description |
|--------|---------------|-------------|
| `createStream(name, priority, sampleRate, channels, targetDevice, bufferMs)` | Main thread | Create a PipeWire audio stream. Priority 0-100. Returns `AudioStreamHandle*`. |
| `destroyStream(handle)` | Main thread | Destroy a previously created stream |
| `writeAudio(handle, data, size)` | Any thread | Write PCM audio data. Returns bytes written or -1. |
| `setMasterVolume(int)` | Thread-safe | Set master volume (0-100) |
| `masterVolume()` | Thread-safe | Get current master volume (0-100) |
| `requestAudioFocus(handle, type)` | Thread-safe | Request audio focus (`Gain`, `GainTransient`, `GainTransientMayDuck`) |
| `releaseAudioFocus(handle)` | Thread-safe | Release audio focus, restore ducked streams |
| `setOutputDevice(deviceName)` | Main thread | Set default output device (`"auto"` for default) |
| `setInputDevice(deviceName)` | Main thread | Set default input device |
| `outputDevice()` | Main thread | Get current output device name |
| `inputDevice()` | Main thread | Get current input device name |
| `openCaptureStream(name, sampleRate, channels, bitDepth)` | Main thread | Open a PipeWire microphone stream; returns null on failure. |
| `closeCaptureStream(handle)` | Main thread | Close a capture stream. |
| `setCaptureCallback(handle, callback)` | Main thread setup; callback on PipeWire thread | Receive captured PCM buffers. |

**Stability:** Experimental — API may change.

### EventBus (`IEventBus`)

Pub/sub event system with string-keyed topics and QVariant payloads.

| Method | Thread Safety | Description |
|--------|---------------|-------------|
| `subscribe(topic, callback)` | Thread-safe | Subscribe to a topic. Returns subscription ID. |
| `unsubscribe(id)` | Thread-safe | Remove a subscription. Off the owner thread, waits for an executing callback to finish. |
| `publish(topic, payload)` | Thread-safe | Publish an event. Callbacks invoked on main thread. |

**Delivery:** All callbacks are invoked via `Qt::QueuedConnection` on the bus
owner thread. Unsubscribe cancels queued delivery; a callback may unsubscribe
itself without deadlocking.

**Stability:** Stable

### ActionRegistry

Named command dispatch. Actions are synchronous.

| Method | Thread Safety | Description |
|--------|---------------|-------------|
| `registerAction(id, handler)` | Main thread only | Register a named action handler. |
| `unregisterAction(id)` | Main thread only | Remove an action handler. |
| `dispatch(id, payload)` | Main thread only | Execute an action. Returns `false` if unknown. |
| `registeredActions()` | Main thread only | List all registered action IDs. |

**Plugin-facing actions:**

| Action ID | Payload | Description |
|-----------|---------|-------------|
| `app.quit` | — | Quit the application |
| `app.home` | — | Return to home (deactivate current plugin) |
| `app.launchPlugin` | `QString` plugin ID | Activate a plugin by ID |
| `app.openSettings` | — | Navigate to settings view |
| `theme.toggle` | — | Toggle day/night mode |
| `aa.sendButton` | `int` keycode | Send an AA button press to the phone |
| `aa.cluster.applyProfile` | complete map | Stage `resolution`, `dpi`, `content_width`, `content_height`, `gal_version` (`1.7` or `4.3`), and `native_turn_card_available`; an accepted real change reconnects active AA for renegotiation |
| `aa.cluster.resetProfile` | — | Restore the runtime 480p/140-DPI/300×300 CLUSTER baseline |

The `aa.cluster.*` actions are registered only when
`experimental_cluster_display` was enabled at startup. Profile overrides are
process-lifetime only. Each apply payload is validated and staged atomically;
invalid and no-op maps do not request a reconnect. Action dispatch acknowledges
that the named handler ran; it does not itself report profile acceptance. UI
callers can read the CLUSTER diagnostics; External API callers must inspect the
application log for acceptance or rejection in this phase.

GAL 1.7 is the default profile and requires only a MATCH response status. The
default-off GAL 4.3 profile requires MATCH plus a numerically equal-or-higher
reported tuple; the observed request 4.3 → response 6.0/MATCH is accepted.
The requested tuple remains the only local feature-policy input. On 4.3, the
native-turn declaration writes the CLUSTER `VideoConfig` field-11 hidden UI
availability element plus field-1 companion insets that mirror the unchanged
legacy total margins. The default 300x300 CLUSTER uses left/right 250 and
top/bottom 90 plus enum 5. It neither renders a native turn card nor selects
phone content. It is false for 1.7; native false leaves field 11 absent. The
former session-configuration value-16 `turn_data_available` control is removed
because AA 17.3 had no consumer for that bit.

Additional internal actions exist for navbar gesture routing (`navbar.volume.*`, `navbar.clock.*`, `navbar.brightness.*`, `app.minimize`, `app.restart`) but are not part of the plugin API.

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

### BluetoothService (`IBluetoothService`)

Adapter management, pairing, and paired device access.

| Method | Thread Safety | Description |
|--------|---------------|-------------|
| `adapterAddress()` | Main thread | Local adapter MAC address |
| `adapterAlias()` | Main thread | Local adapter friendly name |
| `isPairable()` / `setPairable(bool)` | Main thread | Pairable mode control |
| `isPairingActive()` | Main thread | Whether a pairing dialog is active |
| `pairingDeviceName()` / `pairingPasskey()` | Main thread | Current pairing request details |
| `confirmPairing()` / `rejectPairing()` | Main thread | Respond to pairing request |
| `pairedDevicesModel()` | Main thread | `QAbstractListModel*` of paired devices |
| `forgetDevice(address)` | Main thread | Remove a paired device. |
| `connectedDeviceName()` / `connectedDeviceAddress()` | Main thread | Current connected-device identity. |

**Stability:** Stable

### DisplayService (`IDisplayService`)

Backlight brightness control.

| Method | Thread Safety | Description |
|--------|---------------|-------------|
| `brightness()` | Thread-safe | Current backlight brightness (0-100) |
| `setBrightness(int)` | Main thread | Set backlight brightness (0-100) |

**Stability:** Stable

### EqualizerService (`IEqualizerService`)

Per-stream audio equalization with presets and manual band control.

| Method | Thread Safety | Description |
|--------|---------------|-------------|
| `activePreset(StreamId)` | Main thread | Active preset name for a stream (empty if manually adjusted) |
| `applyPreset(StreamId, name)` | Main thread | Apply a named preset (bundled or user). Falls back to Flat if not found. |
| `setGain(StreamId, band, dB)` | Main thread | Set a single band gain (clears active preset) |
| `gain(StreamId, band)` | Main thread | Get a single band gain |
| `gainsForStream(StreamId)` | Main thread | Get all 10 band gains as `std::array<float, 10>` |
| `setBypassed(StreamId, bool)` | Main thread | Enable/disable bypass for a stream |
| `isBypassed(StreamId)` | Main thread | Get bypass state |
| `bundledPresetNames()` | Main thread | List built-in preset names |
| `userPresetNames()` | Main thread | List user-created preset names |
| `saveUserPreset(StreamId, name)` | Main thread | Save current gains as a user preset. An omitted name is generated; whitespace-only or bundled names return an empty string. An existing user name retains overwrite semantics. |
| `deleteUserPreset(name)` | Main thread | Delete a user preset. Streams using it revert to Flat. |
| `renameUserPreset(old, new)` | Main thread | Rename a user preset; rejects blank, bundled, or duplicate targets. |

`StreamId` values are `Media`, `Navigation`, and `System`. `Phone` remains a
deprecated source-compatibility alias for `System`; numeric value 2 processes
AA system sounds, not HFP call audio.

**Stability:** Stable

### OverlayService

Plugins register QML overlays through the host overlay framework instead of
placing ad hoc items in the shell.

| Method | Description |
|---|---|
| `registerOverlay(descriptor)` | Register an overlay; returns `false` for a duplicate ID. |
| `unregisterOverlay(id)` | Remove the overlay and its generated actions. |
| `setVisible(id, visible)` / `toggle(id)` | Control visibility. |
| `move(id, geometry)` | Update optional `x`, `y`, `width`, and `height` geometry. |
| `isVisible(id)` | Query current visibility. |

`OverlayDescriptor` contains `id`, `sourcePluginId`, `qmlComponent`, `band`,
`visible`, and optional `geometry`. Registration bands are Notifications 1000,
User 2000, SystemModal 3000, and Gesture 4000. The shell reserves z=3500 for
its dim fixture between system-modal and gesture content.

## Error Handling

- Plugin methods should not throw exceptions.
- Return `false` from `initialize()` to indicate failure — the plugin will be disabled.
- Service methods return default values (empty QVariant, false, etc.) on error.
- Use `IHostContext::log()` for diagnostic messages.

## Dashboard Contributions

Plugins can contribute widgets to the home screen dashboard via `widgetDescriptors()`.

### WidgetDescriptor

Each widget descriptor declares a dashboard contribution with typed metadata:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `id` | `QString` | (required) | Unique widget identifier |
| `displayName` | `QString` | (required) | Human-readable name shown in picker |
| `iconName` | `QString` | `""` | Material Icon codepoint (e.g. `"\ue8b5"`) |
| `category` | `QString` | `""` | Category ID: `"status"`, `"media"`, `"navigation"`, `"launcher"` |
| `description` | `QString` | `""` | Short description for picker display |
| `qmlComponent` | `QUrl` | `QUrl()` | QRC URL of the widget's QML file |
| `pluginId` | `QString` | `""` | Source plugin ID. Must be set explicitly to your plugin's `id()` in `widgetDescriptors()`. |
| `contributionKind` | `DashboardContributionKind` | `Widget` | `Widget`, deferred `LiveSurfaceWidget`, or manifest-backed `WebWidget`. |
| `defaultConfig` | `QVariantMap` | `{}` | Default per-instance config |
| `configSchema` | `QList<ConfigSchemaField>` | `{}` | Optional host-rendered widget settings (`Enum`, `Bool`, or `IntRange`). |
| `minCols`, `minRows` | `int` | `1` | Minimum grid size |
| `maxCols`, `maxRows` | `int` | `6`, `4` | Maximum grid size |
| `defaultCols`, `defaultRows` | `int` | `1` | Default grid size |
| `singleton` | `bool` | `false` | System-seeded, non-removable, hidden from picker |

### Contribution Kinds

| Kind | Description | Status |
|------|-------------|--------|
| `Widget` | Native QML data-display or control widget | Active |
| `LiveSurfaceWidget` | Full interactive native surface (embedded AA pane, camera feed) | Declared but deferred — no runtime host exists yet |
| `WebWidget` | Manifest-backed HTML/JS content hosted by `WebWidgetHost.qml` | Active when Qt WebEngine support is built |

The widget picker accepts `Widget` and `WebWidget` descriptors that fit the
available space and have a component URL. Web widgets are discovered and
registered through the web-widget scanner. `LiveSurfaceWidget` remains
excluded until a runtime host is implemented.

### WidgetInstanceContext

Widget QML receives a `WidgetInstanceContext` with layout and provider properties:

| Property | Type | Access | Description |
|----------|------|--------|-------------|
| `cellWidth` | `int` | WRITE + NOTIFY | Pixel width of one grid cell (reactive, updates on resize) |
| `cellHeight` | `int` | WRITE + NOTIFY | Pixel height of one grid cell (reactive, updates on resize) |
| `instanceId` | `QString` | CONSTANT | Unique instance identifier |
| `widgetId` | `QString` | CONSTANT | Widget descriptor ID |
| `colSpan` | `int` | WRITE + NOTIFY | Current column span (reactive, updates on resize) |
| `rowSpan` | `int` | WRITE + NOTIFY | Current row span (reactive, updates on resize) |
| `isCurrentPage` | `bool` | WRITE + NOTIFY | Whether this widget's page is currently visible |
| `projectionStatus` | `QObject*` | CONSTANT | `IProjectionStatusProvider` — projection connection state |
| `navigationProvider` | `QObject*` | CONSTANT | `INavigationProvider` — nav data (road, maneuver, distance) |
| `mediaStatus` | `QObject*` | CONSTANT | `IMediaStatusProvider` — media metadata and playback controls |
| `effectiveConfig` | `QVariantMap` | READ + NOTIFY | Descriptor defaults merged with this instance's overrides. |

`cellWidth`, `cellHeight`, `colSpan`, `rowSpan`, and `isCurrentPage` are updated by the host via QML `Binding` elements, keeping widget layouts reactive to grid changes and page navigation.

Provider properties are resolved from `IHostContext` and may be `null` if the corresponding service is not registered.

## Provider Interfaces

Shell, dashboard, and widget QML access cross-cutting state through narrow provider interfaces rather than concrete plugin/bridge types. This decouples the UI from specific plugin implementations.

| Interface | Root-Context Name | Backed By | Provides |
|-----------|-------------------|-----------|----------|
| `IProjectionStatusProvider` | `ProjectionStatus` | `ProjectionStatusProvider` → `AndroidAutoOrchestrator` | `projectionState`, `statusMessage` |
| `INavigationProvider` | `NavigationProvider` | `NavigationDataBridge` | `navActive`, `roadName`, `formattedDistance`, maneuver data |
| `IMediaStatusProvider` | `MediaStatus` | `MediaStatusService` | Metadata, source-native playback state, normalized `isPlaying`, artwork/progress, and playback controls |
| `ICallStateProvider` | `CallStateProvider` | `PhoneStateService` | `callState`, `callerName`, `callerNumber`, `answer()`/`hangup()` |

`IMediaStatusProvider.source` returns `"AndroidAuto"`, `"Bluetooth"`, or
`"MediaPlayer"` (or an empty string when no source is active). Its
`playbackState` is deliberately source-native: Bluetooth and MediaPlayer use
0/1/2 for stopped/playing/paused, while Android Auto uses 1/2/3. QML should
normally consume the normalized `isPlaying` property. The provider also
exposes `artUrl`, `position`, `duration`, and `hasPosition`.

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
| `MediaStatusService` | Core | Merges Android Auto, Bluetooth, and local Media Player state with recency/playing priority logic. |
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
- Must contain a shared library named `lib<last-segment-of-plugin-id>.so` (e.g., plugin ID `com.example.weather` expects `libweather.so`)
- The `.so` is loaded via `QPluginLoader` and the instance is cast via `qobject_cast<IPlugin*>`
- The plugin class must inherit from both `QObject` and `oap::IPlugin`, and declare `Q_OBJECT`, `Q_PLUGIN_METADATA(IID "org.openauto.PluginInterface/1.0")`, and `Q_INTERFACES(oap::IPlugin)`
- Discovered by `PluginManager::discoverPlugins()`

Dynamic plugin loading is implemented but untested. There is no in-tree dynamic plugin example. Prefer static plugins for now.
