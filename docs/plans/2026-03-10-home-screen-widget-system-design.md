# Home Screen Widget System Design

## Overview

Replace the current placeholder HomeMenu and launcher grid with a proper home screen featuring a responsive 3-pane widget layout and a compact launcher dock. Widgets are swappable via long-press, provided by plugins or registered standalone, and persisted in YAML config.

## Layout Structure

### Landscape (width > height)

```
+-------------------+----------------+
|                   |   Sub-pane 1   |
|                   |                |
|   Main Pane       +----------------+
|    (~60%)         |   Sub-pane 2   |
|                   |                |
+-------------------+----------------+
+------------------------------------+
|   AA   Music   Phone   Settings    |
+------------------------------------+
```

Main pane on the left (~60% width). Two sub-panes stacked vertically on the right (~40% width). Launcher dock spans full width at bottom.

### Portrait (height > width)

```
+------------------+
|                  |
|   Main Pane      |
|    (~60%)        |
|                  |
+--------+---------+
|  Sub1  |  Sub2   |
+--------+---------+
+------------------+
| AA Music Ph Set  |
+------------------+
```

Main pane on top (~60% height). Two sub-panes side-by-side below (~40% height). Dock at bottom.

### Key Layout Rules

- Breakpoint: `width > height` (simple aspect ratio check)
- Pane area fills all space between navbar edge and launcher dock
- All sizing via UiMetrics -- no hardcoded pixels
- Pane gaps: `UiMetrics.gap` between panes and between pane area and dock
- Navbar position (any edge) handled by Shell.qml -- pane layout fills `contentArea`

## Widget API

### Architecture: Descriptor + Instance

Widgets use a two-layer model mirroring the existing plugin lifecycle split:

1. **WidgetDescriptor** -- static metadata for registry, picker, and persistence
2. **WidgetInstanceContext** -- per-placement QObject runtime, created on mount, destroyed on swap

This supports the same widget type in multiple panes simultaneously without shared mutable state.

### WidgetDescriptor

```cpp
struct WidgetDescriptor {
    QString id;                    // "org.openauto.clock"
    QString displayName;           // "Clock"
    QString iconName;              // "schedule" (Material Symbols)
    WidgetSizeFlags supportedSizes; // Main | Sub
    QUrl qmlComponent;             // qrc:/widgets/clock/ClockWidget.qml
    QString pluginId;              // optional, empty for standalone
    QVariantMap defaultConfig;     // optional
};
```

### WidgetPlacement (persistence)

```cpp
struct WidgetPlacement {
    QString instanceId;   // "clock-main" (unique per placement)
    QString widgetId;     // "org.openauto.clock"
    QString pageId;       // "home"
    QString paneId;       // "main", "sub1", "sub2"
    PaneSize paneSize;    // Main or Sub (derived from paneId + layout template)
    QVariantMap config;   // optional per-instance config
};
```

### WidgetInstanceContext (QML runtime)

```cpp
class WidgetInstanceContext : public QObject {
    Q_OBJECT
    Q_PROPERTY(PaneSize paneSize READ paneSize CONSTANT)
    Q_PROPERTY(QString instanceId READ instanceId CONSTANT)
    Q_PROPERTY(IHostContext* hostContext READ hostContext CONSTANT)
};
```

Created fresh for each pane mount. Injected into a child QQmlContext. Destroyed when the widget is swapped out or the page unloads. Widgets must tolerate being destroyed and recreated.

### Plugin-Provided Widgets

IPlugin gets an optional method:

```cpp
virtual QList<WidgetDescriptor> widgetDescriptors() const { return {}; }
virtual QObject* createWidgetContext(const QString& widgetId,
                                     const WidgetPlacement& placement,
                                     QObject* parent) { return nullptr; }
```

- `widgetDescriptors()` returns metadata only -- no live objects
- `createWidgetContext()` is optional; plugins that need to inject runtime helpers (e.g., playback controller) override this
- Host context is available through the plugin's existing `initialize(IHostContext*)` -- not re-attached per widget

### Standalone Widgets

Built-in widgets (clock, etc.) implement a lightweight `IWidgetProvider`:

```cpp
class IWidgetProvider {
public:
    virtual ~IWidgetProvider() = default;
    virtual QList<WidgetDescriptor> widgets() const = 0;
};
```

Registered with `WidgetRegistry` at startup, same pattern as plugin registration.

### WidgetRegistry

Central registry that:
- Collects descriptors from plugins and standalone providers
- Resolves widget IDs to descriptors
- Provides filtered lists by supported size (for the picker)
- Validates placements on config load

## Widget Picker UX

**Trigger:** Long-press on any pane (~500ms hold).

**Flow:**
1. Pane dims slightly + subtle scale pulse to confirm edit mode
2. Picker overlay slides up from bottom -- single horizontal scrollable row of widget cards (icon + name)
3. Only widgets whose `supportedSizes` includes the target pane's size are shown
4. Tap a widget card -> swap immediately, picker dismisses
5. Tap outside picker -> cancel, no change
6. "Clear" option to leave a pane empty

**Rationale for a row:** On 1024x600 with automotive touch targets, a single row of ~80px cards is the right density. Even a healthy plugin ecosystem produces 8-12 widgets, not dozens.

**Edit scope:** Long-press affects only the pressed pane. No "jiggle all panes" mode.

**Persistence:** On selection, write the updated WidgetPlacement to YAML config immediately.

## Launcher Dock

The dock replaces LauncherMenu as the app-launching surface.

- Single horizontal row of compact tiles (icon-dominant + short label)
- Centered horizontally, evenly spaced
- Height: approximately `UiMetrics.tileH * 0.5`
- Contents driven by existing `LauncherModel` / YAML config (AA, Music, Phone, Settings)
- Tap launches plugin/settings (same as current launcher)
- No long-press on dock items (reserved for pane widgets)
- M3 Level 1 elevation (lighter than pane widgets)
- Lives outside the page container in the QML tree (globally fixed)

LauncherMenu.qml is replaced by a new HomeScreen.qml containing the pane layout + dock.

## Default Widgets

### Built-in (Standalone)

| Widget | ID | Sizes | Description |
|--------|----|-------|-------------|
| Clock | org.openauto.clock | Main, Sub | Main: large time + date + day. Sub: time + date |
| AA Status | org.openauto.aa-status | Main, Sub | Disconnected: "tap to connect" + car icon. Connected: connection info |

### Plugin-Provided

| Widget | Plugin | Sizes | Description |
|--------|--------|-------|-------------|
| Now Playing | BtAudio | Main, Sub | Album art, track, artist, controls. Sub: compact strip |
| Nav Turn | AndroidAuto | Sub | Next turn icon, distance, road name (active nav only) |
| Phone Status | Phone | Sub | Recent/missed calls, active call indicator |

### Default Layout (Out of Box)

- Main pane: Clock
- Sub-pane 1: AA Status
- Sub-pane 2: Now Playing

### Empty/Unavailable States

- Now Playing (nothing playing): "No music playing" + muted icon
- Nav Turn (no active navigation): empty state text
- Phone Status (no history): "No recent calls"
- AA Status (disconnected): connect prompt (this IS its idle state)

## Multi-Page Future-Proofing

### What We Build Now

- `PageDescriptor` in the data model with `id`, `layoutTemplate`, `order`, `flags`
- `pages[]` list in YAML config (ships with one entry: `home`)
- `pageId` field on every WidgetPlacement
- `activePageId` property in QML (hardcoded to `"home"`)
- Placements filtered through a model by activePageId
- Config schema `version` field

### What This Enables Later

- Additional pages by adding entries to `pages[]` and placements with new pageId values
- Different layout templates per page (e.g., `single-fullscreen` for a dedicated widget page)
- Swipe navigation between pages
- Page indicators in the dock area

### What We Do NOT Build Now

- Page switching UI
- Swipe gestures between pages
- Page creation/deletion
- Multiple layout template implementations (only `standard-3pane`)

### PageDescriptor

```cpp
struct PageDescriptor {
    QString id;              // "home"
    QString layoutTemplate;  // "standard-3pane"
    int order;               // 0
    QVariantMap flags;       // future behavior toggles
};
```

## Config Schema

```yaml
version: 1

pages:
  - id: home
    layoutTemplate: standard-3pane
    order: 0

placements:
  - instanceId: clock-main
    widgetId: org.openauto.clock
    pageId: home
    paneId: main
    config: {}

  - instanceId: aa-status-sub1
    widgetId: org.openauto.aa-status
    pageId: home
    paneId: sub1
    config: {}

  - instanceId: now-playing-sub2
    widgetId: org.openauto.bt-now-playing
    pageId: home
    paneId: sub2
    config: {}
```

### Validation Rules

- Unique `instanceId` across all placements
- At most one placement per `(pageId, paneId)`
- `widgetId` must resolve in WidgetRegistry
- `paneId` must be valid for the page's layoutTemplate
- Unknown `pageId` falls back safely (ignored)
- Unknown `paneId` dropped on load

## Qt 6.8 Implementation Notes

- Per-pane `Loader` component -- set source on place, clear on swap
- Child `QQmlContext` per widget instance (mirrors PluginRuntimeContext pattern)
- Compiled QML via qrc for Pi startup performance
- Event-driven widget updates -- avoid per-widget polling timers
- Do not pre-optimize component caching; let the engine handle it
- `pragma ComponentBehavior: Bound` for widget QML components
- Heavier widget subtrees use inner `Loader` for deferred loading
