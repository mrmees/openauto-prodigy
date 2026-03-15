# Widget Developer Guide

This guide walks you through creating a widget for the OpenAuto Prodigy home screen. By the end, you will have a working widget that appears in the widget picker, responds to grid sizing, and follows the project's theming and layout conventions.

For the full plugin lifecycle and IHostContext services reference, see [plugin-api.md](plugin-api.md). This guide focuses specifically on widget development.

---

## Prerequisites

- Qt 6.8, CMake, C++17
- Project builds and runs successfully (see [development.md](development.md))
- Familiarity with QML basics

---

## Widget Concepts

A widget in OpenAuto Prodigy is a lightweight QML component that lives in a cell-based grid on the home screen. The system is built from three layers:

**WidgetDescriptor** — Static metadata declaring the widget's identity, category, size constraints, and QML component URL. Defined in C++ and registered at startup. This is the "what" of a widget.

**WidgetInstanceContext** — A QObject injected into each widget instance's QML at runtime. Provides layout information (cell dimensions, span), page visibility, and access to provider interfaces (media status, projection status, navigation). This is the "where and when" of a widget.

**Grid Model** — The runtime layout engine. Manages placement (column, row, page), span (colSpan, rowSpan), drag/drop, resize, and persistence to YAML config. You generally don't interact with this directly from widget QML.

### Categories

Widgets are grouped into four categories, displayed in this fixed order in the widget picker:

| Category | Order | Purpose | Example |
|----------|-------|---------|---------|
| `status` | 0 | System/connection state | Clock, AA Status |
| `media` | 1 | Audio/media information | Now Playing |
| `navigation` | 2 | Turn-by-turn / map data | Navigation |
| `launcher` | 3 | App launchers | Settings, Android Auto |

---

## How Widgets Get Loaded

There are two paths for getting a widget into the system. Both ultimately provide `WidgetDescriptor` instances to the `WidgetRegistry`.

### Path 1: Local Customizer (Supported)

You are building from source and adding a widget to your own install. This is the tested, reliable path.

1. **Write an IPlugin subclass** that overrides `widgetDescriptors()` to return your widget descriptors
2. **Create the QML file** implementing the widget UI
3. **Register the plugin** as a static plugin in `main.cpp` via `PluginManager::registerStaticPlugin()`
4. **Add CMake setup** for the QML file with `QT_QML_SKIP_CACHEGEN TRUE`
5. **Rebuild** — the widget appears in the picker

If your widget is standalone (not backed by a plugin's services), you can skip the IPlugin subclass entirely and register descriptors directly in `main.cpp`, the way the built-in Clock and AA Status widgets are registered:

```cpp
oap::WidgetDescriptor clockDesc;
clockDesc.id = "org.openauto.clock";
clockDesc.displayName = "Clock";
clockDesc.iconName = "\ue8b5";  // Material icon codepoint
clockDesc.category = "status";
clockDesc.description = "Current time";
clockDesc.minCols = 1; clockDesc.minRows = 1;
clockDesc.maxCols = 6; clockDesc.maxRows = 4;
clockDesc.defaultCols = 2; clockDesc.defaultRows = 2;
clockDesc.qmlComponent = QUrl("qrc:/OpenAutoProdigy/ClockWidget.qml");
widgetRegistry->registerWidget(clockDesc);
```

### Path 2: External Plugin Author (Experimental)

Dynamic plugin loading from `~/.openauto/plugins/<plugin-id>/` exists in `PluginManager::discoverPlugins()` but **is untested**. Here are the actual mechanics from the code:

- The directory must contain a `plugin.yaml` manifest file
- The library filename is derived from the plugin ID: `lib<last-segment>.so`. For example, plugin ID `com.example.weather` expects `libweather.so`
- The `.so` must be a Qt plugin loaded via `QPluginLoader` and cast via `qobject_cast<IPlugin*>`. Your plugin class must:
  - Inherit from both `QObject` and `oap::IPlugin`
  - Declare `Q_OBJECT`
  - Declare `Q_PLUGIN_METADATA(IID "org.openauto.PluginInterface/1.0")` (the IID is defined in `IPlugin.hpp`)
  - Declare `Q_INTERFACES(oap::IPlugin)`
- There is no in-tree dynamic plugin example. The existing static plugins (e.g., `AndroidAutoPlugin`) show the `QObject`/`Q_OBJECT`/`Q_INTERFACES` pattern but omit `Q_PLUGIN_METADATA` since they are registered directly in `main.cpp`

Both paths end up calling `IPlugin::widgetDescriptors()` — the widget QML code is identical either way.

---

## Step-by-Step: Your First Widget

This walkthrough creates a simple "System Info" widget using the local customizer path. We will register the descriptor directly in `main.cpp` (no IPlugin subclass needed for a standalone widget).

### 1. Create the QML File

Create `qml/widgets/SystemInfoWidget.qml`:

```qml
import QtQuick
import QtQuick.Layouts

Item {
    id: sysInfoWidget
    clip: true

    // Widget contract: context injection from host
    property QtObject widgetContext: null

    // Span-based breakpoints (NOT pixel-based)
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1
    readonly property bool showDetails: colSpan >= 2

    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width - UiMetrics.spacing * 2
        spacing: UiMetrics.spacing

        NormalText {
            text: "System Info"
            font.pixelSize: UiMetrics.fontTitle
            color: ThemeService.onSurface
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        NormalText {
            visible: showDetails
            text: Qt.application.version
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.onSurfaceVariant
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // pressAndHold forwarding for host context menu (edit mode)
    MouseArea {
        anchors.fill: parent
        z: -1  // behind any interactive content
        pressAndHoldInterval: 500
        onPressAndHold: {
            if (sysInfoWidget.parent && sysInfoWidget.parent.requestContextMenu)
                sysInfoWidget.parent.requestContextMenu()
        }
    }
}
```

Key patterns to notice:

- **`property QtObject widgetContext: null`** — mandatory. The host injects this after the Loader creates the component. It will be `null` briefly at startup.
- **Null guards everywhere:** `widgetContext ? widgetContext.colSpan : 1`. Without these, you get binding errors.
- **Span-based breakpoints:** `showDetails: colSpan >= 2`, not `width > 300`. The grid is resolution-independent.
- **Theme tokens:** `ThemeService.onSurface`, `ThemeService.onSurfaceVariant` — these are available on the root QML context.
- **UiMetrics for sizing:** `UiMetrics.fontTitle`, `UiMetrics.spacing` — never hardcode pixel values.
- **Context menu forwarding:** `parent.requestContextMenu()` from a `MouseArea` at `z: -1`.

### 2. Register the QML File in CMake

In `src/CMakeLists.txt`, inside the `qt_add_qml_module` block, add your widget file alongside the existing widget entries:

```cmake
    ${CMAKE_SOURCE_DIR}/qml/widgets/SystemInfoWidget.qml
    QT_QML_SOURCE_TYPENAME "SystemInfoWidget"
    QT_RESOURCE_ALIAS "SystemInfoWidget.qml"
    QT_QML_SKIP_CACHEGEN TRUE
```

**`QT_QML_SKIP_CACHEGEN TRUE` is mandatory for all widget QML files.** See [Why Skip Cachegen](#why-skip-cachegen) below.

### 3. Register the Widget Descriptor

In `src/main.cpp`, add your descriptor registration alongside the existing built-in widgets (look for the "Register built-in standalone widgets" section):

```cpp
oap::WidgetDescriptor sysInfoDesc;
sysInfoDesc.id = "com.example.sysinfo";
sysInfoDesc.displayName = "System Info";
sysInfoDesc.iconName = "\ue88e";  // info (Material icon codepoint)
sysInfoDesc.category = "status";
sysInfoDesc.description = "Version and system details";
sysInfoDesc.minCols = 1; sysInfoDesc.minRows = 1;
sysInfoDesc.maxCols = 4; sysInfoDesc.maxRows = 2;
sysInfoDesc.defaultCols = 2; sysInfoDesc.defaultRows = 1;
sysInfoDesc.qmlComponent = QUrl("qrc:/OpenAutoProdigy/SystemInfoWidget.qml");
widgetRegistry->registerWidget(sysInfoDesc);
```

### 4. Build and Verify

```bash
cd build
cmake --build . -j$(nproc)
```

Launch the app. Long-press an empty area on the home screen to open the widget picker. Your "System Info" widget should appear under the "Status" category.

---

## Manifest Spec (WidgetDescriptor)

Every widget is described by a `WidgetDescriptor` struct defined in `src/core/widget/WidgetTypes.hpp`:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `id` | `QString` | (required) | Unique reverse-DNS identifier, e.g. `"org.openauto.clock"` |
| `displayName` | `QString` | (required) | Human-readable name shown in the widget picker |
| `iconName` | `QString` | `""` | Material icon codepoint, e.g. `"\ue8b5"` (schedule icon). Used in the picker. |
| `category` | `QString` | `""` | Category ID: `"status"`, `"media"`, `"navigation"`, or `"launcher"` |
| `description` | `QString` | `""` | Short description displayed in the widget picker |
| `qmlComponent` | `QUrl` | `QUrl()` | QRC URL to the widget QML file, e.g. `"qrc:/OpenAutoProdigy/ClockWidget.qml"` |
| `pluginId` | `QString` | `""` | Source plugin ID (set by the system when registered via a plugin) |
| `contributionKind` | `DashboardContributionKind` | `Widget` | `Widget` (lightweight display) or `LiveSurfaceWidget` (deferred, no host path yet) |
| `defaultConfig` | `QVariantMap` | `{}` | Optional per-widget default configuration |
| `minCols` | `int` | `1` | Minimum column span |
| `minRows` | `int` | `1` | Minimum row span |
| `maxCols` | `int` | `6` | Maximum column span |
| `maxRows` | `int` | `4` | Maximum row span |
| `defaultCols` | `int` | `1` | Default column span when placed |
| `defaultRows` | `int` | `1` | Default row span when placed |
| `singleton` | `bool` | `false` | If true: system-seeded, non-removable, hidden from picker. Used for launcher widgets. |

### Icon Codepoints

`iconName` uses Material icon Unicode codepoints, not text names. Common examples:

| Icon | Codepoint | Name |
|------|-----------|------|
| Clock | `"\ue8b5"` | schedule |
| Car | `"\ueff7"` | directions_car |
| Music | `"\ue030"` | music_note |
| Navigation | `"\ue55c"` | navigation |
| Settings | `"\ue8b8"` | settings |
| Info | `"\ue88e"` | info |

---

## QML Contract

### Required Property

Every widget QML file must declare:

```qml
property QtObject widgetContext: null
```

The host injects a `WidgetInstanceContext` into this property after the Loader creates the component. It will be `null` during initial construction — **always null-guard**.

### WidgetInstanceContext Properties

| Property | Type | Access | Description |
|----------|------|--------|-------------|
| `cellWidth` | `int` | WRITE + NOTIFY | Pixel width of one grid cell (reactive, updates on resize) |
| `cellHeight` | `int` | WRITE + NOTIFY | Pixel height of one grid cell (reactive, updates on resize) |
| `instanceId` | `QString` | CONSTANT | Unique instance identifier for this placement |
| `widgetId` | `QString` | CONSTANT | Widget descriptor ID |
| `colSpan` | `int` | WRITE + NOTIFY | Current column span (reactive, updates on resize) |
| `rowSpan` | `int` | WRITE + NOTIFY | Current row span (reactive, updates on resize) |
| `isCurrentPage` | `bool` | WRITE + NOTIFY | Whether this widget's page is currently visible |
| `projectionStatus` | `QObject*` | CONSTANT | `IProjectionStatusProvider` instance (may be null) |
| `navigationProvider` | `QObject*` | CONSTANT | `INavigationProvider` instance (may be null) |
| `mediaStatus` | `QObject*` | CONSTANT | `IMediaStatusProvider` instance (may be null) |

### Provider Access

Provider objects are resolved from `IHostContext` and may be `null` if the corresponding service is not registered. Always null-guard:

```qml
// Safe media status access pattern
property bool hasMedia: widgetContext && widgetContext.mediaStatus
                        ? widgetContext.mediaStatus.hasMedia : false
property string title: widgetContext && widgetContext.mediaStatus
                       ? (widgetContext.mediaStatus.title || "") : ""
```

The `IMediaStatusProvider` interface exposes:

| Property/Method | Type | Description |
|-----------------|------|-------------|
| `hasMedia` | `bool` | Whether any media source is active |
| `title` | `QString` | Current track title |
| `artist` | `QString` | Current artist |
| `album` | `QString` | Current album |
| `playbackState` | `int` | 0 = stopped, 1 = playing, 2 = paused |
| `source` | `QString` | `"Bluetooth"` or `"AndroidAuto"` |
| `appName` | `QString` | Source app name |
| `playPause()` | method | Toggle play/pause |
| `next()` | method | Skip to next track |
| `previous()` | method | Skip to previous track |

See the `NowPlayingWidget.qml` source for a complete example of provider-backed widget QML.

### ThemeService Tokens

ThemeService is available on the root QML context. Use M3 (Material Design 3) token names:

**Common tokens for widget development:**

| Token | Usage |
|-------|-------|
| `ThemeService.background` | App background |
| `ThemeService.surface` | General surface color |
| `ThemeService.surfaceContainer` | Widget glass card background |
| `ThemeService.surfaceContainerLow` | Subtle surface |
| `ThemeService.surfaceContainerHigh` | Elevated surface |
| `ThemeService.onSurface` | Primary text on surfaces |
| `ThemeService.onSurfaceVariant` | Secondary text, icons |
| `ThemeService.primary` | Accent/brand color |
| `ThemeService.onPrimary` | Text on primary color |
| `ThemeService.secondary` | Secondary accent |
| `ThemeService.error` | Error state color |
| `ThemeService.onError` | Text on error color |
| `ThemeService.outline` | Borders, dividers |
| `ThemeService.outlineVariant` | Subtle borders |
| `ThemeService.inverseSurface` | High-contrast surface |
| `ThemeService.inverseOnSurface` | Text on inverse surface |

All tokens auto-switch between day and night mode. Use `ThemeService.nightMode` (read-only from QML) to check the current mode.

**Additional derived tokens:** `ThemeService.success`, `ThemeService.onSuccess`.

For the full list of M3 tokens, see [plugin-api.md](plugin-api.md#themeservice-ithemeservice).

### UiMetrics

UiMetrics provides resolution-independent sizing. See [design-philosophy.md](design-philosophy.md) for the rationale.

**Do NOT hardcode pixel values in widget QML.** Use UiMetrics constants:

```qml
font.pixelSize: UiMetrics.fontTitle      // heading text
font.pixelSize: UiMetrics.fontBody       // body text
font.pixelSize: UiMetrics.fontSmall      // secondary text
spacing: UiMetrics.spacing               // standard spacing
size: UiMetrics.iconSize                 // standard icon size
size: UiMetrics.iconSmall               // small icon size
```

### Context Menu Forwarding

Widgets must forward long-press events to the host for edit mode (change widget, adjust opacity, clear). Add a MouseArea at `z: -1` behind widget content:

```qml
MouseArea {
    anchors.fill: parent
    z: -1  // behind any interactive MouseAreas
    pressAndHoldInterval: 500
    onPressAndHold: {
        if (myWidget.parent && myWidget.parent.requestContextMenu)
            myWidget.parent.requestContextMenu()
    }
}
```

**Important:** The MouseArea must be at `z: -1` to sit behind widget content. If you put it at `z: 1` or higher, it will steal all touch from child controls (sliders, buttons, etc.).

If your widget has its own interactive controls (like playback buttons), each control's MouseArea should also forward `onPressAndHold`:

```qml
MaterialIcon {
    icon: "\ue037"  // play_arrow
    MouseArea {
        anchors.fill: parent
        onClicked: widgetContext.mediaStatus.playPause()
        onPressAndHold: {
            if (myWidget.parent && myWidget.parent.requestContextMenu)
                myWidget.parent.requestContextMenu()
        }
    }
}
```

---

## Sizing Conventions

### Grid Model

The home screen uses a square-cell grid. Cell size is computed from the display diagonal: `cellSide = diagPx / (9.0 + bias * 0.8)`. On the target 1024x600 display, this gives approximately 7-8 columns and 4-5 rows.

Widgets occupy a rectangular region defined by `colSpan x rowSpan`. Users can resize widgets by dragging the resize handle.

### Responsive Breakpoints

**Use span-based breakpoints, not pixel-based:**

```qml
// CORRECT: span-based
readonly property bool showDetails: colSpan >= 2
readonly property bool isTall: rowSpan >= 2
readonly property bool showArtist: colSpan >= 3

// WRONG: pixel-based (breaks on different displays)
readonly property bool showDetails: width > 300
```

The grid is resolution-independent. A `colSpan` of 2 means "two cells wide" regardless of whether the display is 1024x600 or 1920x1080.

### Size Constraints

Set `minCols`/`minRows` to the smallest size where your widget is still usable. Set `maxCols`/`maxRows` to the largest size where more space adds value. The resize handle enforces these bounds.

Common patterns:

| Widget Type | Min | Max | Default |
|-------------|-----|-----|---------|
| Simple status (clock) | 1x1 | 6x4 | 2x2 |
| Media controls | 2x1 | 6x4 | 3x2 |
| Navigation turn | 2x1 | 4x2 | 3x1 |
| Launcher tile | 1x1 | 3x3 | 1x1 |

### Remap Behavior

When the grid dimensions change (display resize, density bias change), the system remaps widget placements. Widgets that no longer fit are clamped to the new grid dimensions rather than hidden — maximizing visibility.

---

## CMake Setup

### Adding a Widget QML File

In `src/CMakeLists.txt`, inside the `qt_add_qml_module` block, add three properties for each widget file:

```cmake
    ${CMAKE_SOURCE_DIR}/qml/widgets/MyWidget.qml
    QT_QML_SOURCE_TYPENAME "MyWidget"
    QT_RESOURCE_ALIAS "MyWidget.qml"
    QT_QML_SKIP_CACHEGEN TRUE
```

- **`QT_QML_SOURCE_TYPENAME`** — The QML type name (must match the filename without extension)
- **`QT_RESOURCE_ALIAS`** — The resource path alias (the filename)
- **`QT_QML_SKIP_CACHEGEN TRUE`** — **Mandatory for widgets.** See below.

### Why Skip Cachegen

Qt 6.8's `qt_add_qml_module` compiles `.qml` files to C++ at build time (cache generation). The compiled C++ replaces the raw `.qml` file in the resource system. Widgets are loaded dynamically via `Loader { source: url }`, and the Loader's URL lookup fails when cachegen has replaced the raw QML with compiled bytecode.

Setting `QT_QML_SKIP_CACHEGEN TRUE` keeps the raw `.qml` file in the Qt resource system so Loader URL resolution works.

The resource URL prefix is `qrc:/OpenAutoProdigy/` (not `qrc:/qt/qml/OpenAutoProdigy/`).

---

## Gotchas

### MouseArea z-order and pressAndHold

A `MouseArea` must accept the press event to detect `pressAndHold`. If you use `onPressed: mouse.accepted = false` to pass events through, `pressAndHold` will never fire. Solution: put the MouseArea at `z: -1` behind widget content instead.

### Opacity Signal Naming Collision

QML's built-in `Item.opacity` property auto-generates an `opacityChanged` signal. Do not name custom signals `opacityChanged` in your widget — it collides with the built-in signal.

### Loader URL Caching

Widget QML is loaded via `Loader { source: url }`. If you change a widget file but don't rebuild with the correct CMake setup (specifically `QT_QML_SKIP_CACHEGEN TRUE`), you may see stale content from the compiled cache.

### Null widgetContext at Startup

`widgetContext` is `null` when the Loader first creates your component. The host injects the context immediately after via a `Binding` element, but there is a brief window where property bindings evaluate with `null`. Always use the null-guard pattern:

```qml
readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
```

### isCurrentPage for Polling Optimization

If your widget uses a Timer or other polling mechanism, gate it on `isCurrentPage` to avoid unnecessary work when the widget's page is not visible:

```qml
Timer {
    interval: 1000
    running: isCurrentPage  // only tick when page is visible
    repeat: true
    // ...
}
```

### Cross-Build + QML Deploy Mismatch

When deploying to the Pi, the binary and QML must both be updated. Push QML changes via git (`git push` + `git pull` on the Pi). Push binary changes via `rsync`. If only one is updated, you may see stale or mismatched behavior.
