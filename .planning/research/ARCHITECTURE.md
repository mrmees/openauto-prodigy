# Architecture Patterns

**Domain:** Widget framework refinement, launch bar removal, DPI grid layout for v0.6.1
**Researched:** 2026-03-14
**Confidence:** HIGH (direct codebase analysis -- all components exist and running on dev/0.6)

## Current Architecture Snapshot (v0.6 Baseline)

v0.6 shipped the architecture formalization: typed provider interfaces, core-owned services, `WidgetRegistry` accessible via `IHostContext`, `SettingsInputBoundary`. This research covers the v0.6.1 changes that build on top.

### Relevant Components

| Component | File(s) | What It Does Now |
|-----------|---------|------------------|
| `WidgetDescriptor` | `src/core/widget/WidgetTypes.hpp` | Static metadata: id, displayName, iconName, qmlComponent, pluginId, contributionKind, size constraints (min/max/default cols/rows), defaultConfig |
| `GridPlacement` | `src/core/widget/WidgetTypes.hpp` | Per-instance: instanceId, widgetId, col, row, colSpan, rowSpan, opacity, page, visible |
| `DashboardContributionKind` | `src/core/widget/WidgetTypes.hpp` | Enum: `Widget`, `LiveSurfaceWidget` |
| `WidgetRegistry` | `src/core/widget/WidgetRegistry.{hpp,cpp}` | QMap store. `registerWidget`, `unregisterWidget`, `descriptor`, `allDescriptors`, `widgetsFittingSpace`, `descriptorsByKind` |
| `WidgetGridModel` | `src/ui/WidgetGridModel.{hpp,cpp}` | QAbstractListModel. Cell occupancy, place/move/resize/remove, page management, clamping on grid dimension change |
| `DisplayInfo` | `src/ui/DisplayInfo.{hpp,cpp}` | Window size + screen inches (EDID) -> computedDpi. Derives gridColumns/gridRows via `updateGridDimensions()` |
| `LauncherModel` | `src/ui/LauncherModel.{hpp,cpp}` | QAbstractListModel backed by `YamlConfig::launcherTiles()`. Tiles: {id, label, icon, action} |
| `UiMetrics` | `qml/controls/UiMetrics.qml` | QML singleton. DPI/reference -> scale factors. All layout tokens (spacing, fonts, radii, tile sizes) |
| `HomeMenu.qml` | `qml/applications/home/HomeMenu.qml` | ColumnLayout: SwipeView (widget grid pages) + PageIndicator + LauncherDock |
| `LauncherDock.qml` | `qml/components/LauncherDock.qml` | RowLayout of tiles from LauncherModel, ~90px tall. Actions: `plugin:id` or `navigate:settings` |
| `WidgetHost.qml` | `qml/components/WidgetHost.qml` | Glass card bg + Loader + long-press MouseArea |
| `IPlugin` | `src/core/plugin/IPlugin.hpp` | `widgetDescriptors()` returns `QList<WidgetDescriptor>`. Lifecycle: initialize/onActivated/onDeactivated/shutdown |

### Current Data Flow: Grid Dimensions

```
DisplayInfo::updateGridDimensions()
  marginPx = 40           (2 * 20px reference margin)
  dockPx = 56             (reference dock height -- HARDCODED)
  usableW = windowWidth_ - marginPx
  usableH = windowHeight_ - marginPx - dockPx
  targetCellW = 150       (fixed pixels)
  targetCellH = 125       (fixed pixels)
  newCols = clamp(usableW / targetCellW, 3, 8)
  newRows = clamp(usableH / targetCellH, 2, 6)

main.cpp wiring:
  widgetGridModel->setGridDimensions(displayInfo->gridColumns(), displayInfo->gridRows())
  connect(displayInfo, gridDimensionsChanged, [&] {
      widgetGridModel->setGridDimensions(displayInfo->gridColumns(), displayInfo->gridRows())
  })

HomeMenu.qml:
  cellWidth = pageView.width / WidgetGridModel.gridColumns
  cellHeight = pageView.height / WidgetGridModel.gridRows
```

### Current Data Flow: LauncherDock

```
YamlConfig::launcherTiles() -> QList<QVariantMap> {id, label, icon, action}
  |
LauncherModel (QAbstractListModel, context property "LauncherModel")
  |
LauncherDock.qml (Repeater over LauncherModel, RowLayout, fixed height)
  |-- onClick: PluginModel.setActivePlugin("org.openauto.android-auto") etc.
  |-- or: ApplicationController.navigateTo(6) for settings

HomeMenu.qml ColumnLayout:
  SwipeView (Layout.fillHeight)
  PageIndicator
  LauncherDock (Layout.fillWidth, height: UiMetrics.tileH * 0.5 + UiMetrics.spacing)
```

---

## Recommended Architecture Changes for v0.6.1

### Change 1: Remove LauncherDock -- Replace with Launcher Widget

**What changes:**
- DELETE `LauncherDock.qml`
- DELETE `LauncherModel.hpp/cpp` (C++ class)
- DELETE `LauncherModel` QML context property from `main.cpp`
- REMOVE `LauncherDock` from `HomeMenu.qml` ColumnLayout
- REMOVE `YamlConfig::launcherTiles()` and `setLauncherTiles()` (or keep for YAML migration)
- CREATE `qml/widgets/LauncherWidget.qml` -- renders plugin launch tiles within a grid cell
- REGISTER `org.openauto.launcher` as a new `WidgetDescriptor`
- UPDATE `DisplayInfo::updateGridDimensions()` -- remove `dockPx = 56` deduction
- UPDATE default layout YAML -- place launcher widget on page 0

**Why this is the right approach:**
- The dock consumes `~90px` of fixed vertical space on every home page. On 800x480, that is 18.7% of screen height stolen from widgets.
- Converting it to a widget makes it optional -- users who prefer navbar clock-tap for navigation can remove it.
- Eliminates `LauncherModel`, a parallel data model that duplicates plugin metadata already in `PluginModel`.
- Consistent mental model -- everything on the home screen is a widget, no special-case fixed UI.

**LauncherWidget design:**

```
org.openauto.launcher
  minCols=2, minRows=1
  maxCols=6, maxRows=2
  defaultCols=4, defaultRows=1
  category="system"
  userRemovable=true  (user can remove it if they want)
```

The widget reads `PluginModel` (already a global context property) for available plugins + a settings entry. Renders as adaptive icon row within its cell bounds. Pixel breakpoints determine icon-only vs icon+label layout, similar to existing widget pattern.

**Components deleted:**

| Component | File | Replacement |
|-----------|------|-------------|
| `LauncherDock.qml` | `qml/components/LauncherDock.qml` | `LauncherWidget.qml` in grid |
| `LauncherModel` | `src/ui/LauncherModel.{hpp,cpp}` | Widget reads `PluginModel` directly |
| `launcherTiles()` | `src/core/YamlConfig.{hpp,cpp}` | Not needed -- widget discovers plugins at runtime |

**Components modified:**

| Component | Change |
|-----------|--------|
| `HomeMenu.qml` | Remove `LauncherDock {}` from ColumnLayout. SwipeView + PageIndicator only. |
| `DisplayInfo.cpp` | Remove `dockPx = 56` from `updateGridDimensions()` |
| `main.cpp` | Remove `LauncherModel` creation and context property. Add `org.openauto.launcher` registration. |
| `CMakeLists.txt` | Remove `LauncherModel.cpp` from sources |
| Default YAML | Add launcher widget placement on page 0 bottom row |

**Components created:**

| Component | File | Purpose |
|-----------|------|---------|
| `LauncherWidget.qml` | `qml/widgets/LauncherWidget.qml` | Adaptive plugin launch tiles |

**Note:** `LauncherMenu.qml` (full-page launcher grid at `qml/applications/launcher/`) is NOT removed. That is the full launcher view navigated to from the navbar clock, separate from the home screen dock widget.

---

### Change 2: DPI-Aware Grid Dimensions

**What changes:**
- MODIFY `DisplayInfo::updateGridDimensions()` to use DPI-scaled cell targets instead of fixed pixel values

**Current algorithm (fixed pixels):**
```cpp
const int targetCellW = 150;
const int targetCellH = 125;
int newCols = std::clamp(usableW / targetCellW, 3, 8);
int newRows = std::clamp(usableH / targetCellH, 2, 6);
```

**Proposed algorithm (DPI-aware):**
```cpp
// Remove dockPx deduction (dock is gone)
int usableW = windowWidth_ - marginPx;
int usableH = windowHeight_ - marginPx;

// Reference cell size at 160 DPI (Android mdpi baseline, matching UiMetrics)
const double refCellW = 150.0;
const double refCellH = 125.0;

// Scale by DPI ratio -- higher DPI screens get more cells (smaller physical cells)
// Lower DPI screens get fewer cells (larger physical cells for touch targets)
double dpiRatio = computedDpi() / 160.0;
int targetCellW = static_cast<int>(std::round(refCellW * dpiRatio));
int targetCellH = static_cast<int>(std::round(refCellH * dpiRatio));

// Prevent degenerate values
targetCellW = std::max(targetCellW, 80);
targetCellH = std::max(targetCellH, 60);

int newCols = std::clamp(usableW / targetCellW, 3, 8);
int newRows = std::clamp(usableH / targetCellH, 2, 6);
```

**Why DPI-aware:** On a 10" 1920x1080 screen (~203 DPI), the current fixed algorithm gives 6x4 just like a 7" 1024x600 (~170 DPI). But the 10" screen has twice the pixels. DPI-aware sizing gives it 8x5 or similar -- more cells, because the screen is physically larger with more pixels. On a small 800x480 screen (~134 DPI), it produces fewer cells with larger touch targets.

**Target hardware verification:**

| Display | Resolution | Size | DPI | Current Grid | Proposed Grid |
|---------|-----------|------|-----|-------------|---------------|
| Pi Official | 800x480 | 7" | 134 | 5x3 | 4x3 (larger cells) |
| DFRobot | 1024x600 | 7" | 170 | 6x3 | 6x4 (gains a row from dock removal) |
| Generic 10" | 1280x800 | 10.1" | 150 | 8x5 | 7x4 |
| Full HD 10" | 1920x1080 | 10.1" | 224 | 6x4 (pre-dock) | 8x6 (more cells, DPI-aware) |

**Wait -- DPI ratio inversion needed.** Actually, re-examining: if DPI is HIGH, pixels are denser, so MORE pixels are needed per physical cell. The current fixed-pixel approach already produces more columns on wider screens. The DPI scaling should go the OTHER direction -- higher DPI means each cell needs MORE pixels to maintain the same physical size.

Let me reconsider. The actual goal is: cells should be the same **physical size** across displays. A cell should be roughly 25mm wide (touchable). At 160 DPI, 25mm = 157px. At 224 DPI, 25mm = 220px.

**Corrected algorithm:**
```cpp
// Target: ~25mm wide cells, ~22mm tall cells (comfortable touch targets)
const double targetCellMm_W = 25.0;
const double targetCellMm_H = 22.0;

double dpi = computedDpi();
int targetCellW = static_cast<int>(std::round(targetCellMm_W * dpi / 25.4));
int targetCellH = static_cast<int>(std::round(targetCellMm_H * dpi / 25.4));

// Floor: cells must be at least 80x60 px
targetCellW = std::max(targetCellW, 80);
targetCellH = std::max(targetCellH, 60);

int newCols = std::clamp(usableW / targetCellW, 3, 8);
int newRows = std::clamp(usableH / targetCellH, 2, 6);
```

**Re-verification with corrected algorithm:**

| Display | Res | Size | DPI | targetCellW | targetCellH | usableW | usableH | Grid |
|---------|-----|------|-----|------------|------------|---------|---------|------|
| Pi Official | 800x480 | 7" | 134 | 132 | 116 | 760 | 440 | 5x3 |
| DFRobot | 1024x600 | 7" | 170 | 167 | 147 | 984 | 560 | 5x3 |
| Generic 10" | 1280x800 | 10.1" | 150 | 148 | 130 | 1240 | 760 | 8x5 |
| Full HD 10" | 1920x1080 | 10.1" | 224 | 220 | 194 | 1880 | 1040 | 8x5 |

This is sensible: same physical screen size gives same grid, regardless of resolution. Larger screens get more cells. The DFRobot 7" drops from 6 to 5 columns, which may be too aggressive -- the cells become wider. Could tune target to 22mm wide.

**Recommendation:** Use physical mm targets, tune the exact values empirically on the Pi. The architecture change is: replace fixed pixel targets with `mm * dpi / 25.4` computation. The specific mm values (22-25mm wide) are tunable constants.

**Integration points:**
- `DisplayInfo.cpp` only -- self-contained change
- `WidgetGridModel::setGridDimensions()` handles clamping/relayout automatically
- Existing widgets use pixel breakpoints -- they adapt to whatever cell size they get
- Unit tests: pass different DPI values, verify grid dimensions

---

### Change 3: Formalize Widget Manifest Conventions

**What changes:**
- EXTEND `WidgetDescriptor` with additional metadata fields for categorization, documentation, and picker grouping
- DOCUMENT the widget descriptor contract

**New fields on `WidgetDescriptor`:**

```cpp
struct WidgetDescriptor {
    // --- Existing (unchanged) ---
    QString id;
    QString displayName;
    QString iconName;
    QUrl qmlComponent;
    QString pluginId;
    DashboardContributionKind contributionKind;
    QVariantMap defaultConfig;
    int minCols, minRows, maxCols, maxRows, defaultCols, defaultRows;

    // --- NEW for v0.6.1 ---
    QString category;            // "system", "media", "connectivity", "info"
    bool userRemovable = true;   // false = cannot be deleted (e.g., system-critical)
    QString description;         // one-liner for picker tooltip/subtitle
};
```

**Why these fields and not others:**
- `category` -- picker needs to group widgets. 4 categories is enough for the foreseeable widget set.
- `userRemovable` -- the launcher widget (which replaces the dock) should be removable because users who prefer navbar can ditch it. But this flag exists for future system-critical widgets.
- `description` -- the picker currently shows icon + name. A one-liner description helps users choose.
- NOT adding `configSchema` / `isRepositionable` / `tags` / `lifecycle` pointer -- these are future concerns. Keep the struct lean for v0.6.1.

**Integration points:**
- `WidgetTypes.hpp` -- struct change (safe: new fields have defaults)
- `WidgetGridModel::data()` -- add new roles for `CategoryRole`, `UserRemovableRole`, `DescriptionRole`
- Widget picker overlay in `HomeMenu.qml` -- minor update to show categories
- All widget registrations in `main.cpp` -- set category values
- `WidgetRegistry::widgetsFittingSpace()` -- optionally add category filter overload

---

### Change 4: Widget Lifecycle Signals (QML-Side)

**What changes:**
- ADD signals to `WidgetHost.qml` for page visibility and resize events
- Widget QML components can optionally connect to these signals

**Approach -- QML-only, minimal:**

```qml
// WidgetHost.qml additions:
signal widgetActivated()     // page became visible (SwipeView.isCurrentItem)
signal widgetDeactivated()   // page became hidden
signal widgetResized()       // cell dimensions changed (after edit mode resize)
```

**How they fire:**
- `widgetActivated` / `widgetDeactivated`: HomeMenu.qml page Loader `active` property already tracks visibility. When it changes, emit on each WidgetHost in the page.
- `widgetResized`: After `WidgetGridModel.resizeWidget()` succeeds, the Repeater delegate's `width`/`height` bindings update. WidgetHost emits `widgetResized` on `onWidthChanged` or `onHeightChanged` (debounced to avoid spam during drag).

**Why QML signals (not C++ IWidgetLifecycle):**
- All current widgets are pure QML. No widget currently needs C++ lifecycle awareness.
- QML signals are opt-in. Existing widgets that do not connect are unaffected.
- If a future widget needs C++ lifecycle, `IWidgetLifecycle` can be added then. The QML signals and C++ interface are not mutually exclusive.
- Keeps v0.6.1 scope tight -- no new C++ interfaces, no WidgetInstanceContext changes.

**Integration points:**
- `WidgetHost.qml` -- add 3 signal declarations, minor size-change handler
- `HomeMenu.qml` -- emit activate/deactivate on page visibility changes
- No C++ changes
- Document in widget convention guide

---

## Component Boundaries (Post v0.6.1)

| Component | Responsibility | Talks To |
|-----------|---------------|----------|
| `DisplayInfo` | Window size, DPI, grid dims (DPI-aware, no dock deduction) | `WidgetGridModel` (grid dims), QML `UiMetrics` (DPI/window) |
| `WidgetRegistry` | Descriptor store with categories | `WidgetGridModel` (lookups), `main.cpp` (registration), `IPlugin` (plugin descriptors) |
| `WidgetGridModel` | Placement CRUD, occupancy, pages, serialization | `HomeMenu.qml` (model binding), `YamlConfig` (persistence), `WidgetRegistry` (descriptor lookups) |
| `UiMetrics` | DPI-scaled layout tokens for all QML | All QML components |
| `WidgetHost` | Glass card + Loader + lifecycle signals | Widget QML (loaded content), `HomeMenu.qml` (long-press, lifecycle) |
| `HomeMenu.qml` | SwipeView + grid pages + edit mode + overlays (NO dock) | `WidgetGridModel`, `WidgetHost`, picker overlay |
| `LauncherWidget` (NEW) | Plugin launch tiles in grid cell | `PluginModel`, `ApplicationController` |

## Data Flow (Post v0.6.1)

```
DisplayInfo::updateGridDimensions()
  |-- windowSize + computedDpi -> mm-based targets -> clamp(3..8, 2..6)
  |-- NO dockPx deduction (dock removed)
  |-- emits gridDimensionsChanged -> WidgetGridModel.setGridDimensions()

HomeMenu.qml ColumnLayout:
  SwipeView (fills all available height -- no dock stealing space)
  PageIndicator

Widget grid cell:
  WidgetHost.qml wraps each widget
    |-- Loader { source: qmlComponent }
    |-- widgetActivated/deactivated signals on page visibility
    |-- widgetResized signal on cell dimension change

LauncherWidget.qml (within grid like any other widget):
  Reads PluginModel for available plugins
  Renders adaptive icon row/grid within cell bounds
  onClick: PluginModel.setActivePlugin() or ApplicationController.navigateTo()
```

---

## Patterns to Follow

### Pattern 1: Pixel-Breakpoint Responsive Widgets
**What:** Widgets use `width >= N` / `height >= N` bindings for content tiers.
**When:** Every widget. Established pattern across all 4 existing widgets.
**Example:** ClockWidget: `showDate: width >= 250`, `showDay: height >= 180`

### Pattern 2: Widget Registration in main.cpp (Built-In) or initialize() (Plugin)
**What:** Built-in widgets registered as `WidgetDescriptor` structs. Plugin widgets via `IPlugin::widgetDescriptors()` or `hostContext->widgetRegistry()->registerWidget()`.
**When:** All widgets. Single point of truth per descriptor.

### Pattern 3: WidgetHost as Composition Root
**What:** Every widget is wrapped by `WidgetHost.qml` (glass card + Loader + interaction + lifecycle).
**When:** Always. Widgets never render directly in the grid Repeater.

### Pattern 4: Launcher Widget Reads PluginModel
**What:** The launcher widget derives its tiles from `PluginModel` (already exposed as context property), not from a separate config-backed model.
**When:** The launcher widget specifically.
**Why:** Single source of truth for what plugins exist. No YAML duplication.

---

## Anti-Patterns to Avoid

### Anti-Pattern 1: Parallel Navigation Models
**What:** Having both `LauncherModel` (config-backed tiles) and `PluginModel` (plugin metadata) to launch the same plugins.
**Why bad:** Two sources of truth. Adding a plugin requires both registration and launcher config.
**Instead:** Launcher widget reads `PluginModel` directly. One source of truth.

### Anti-Pattern 2: Fixed Pixel Deductions in Grid Calculation
**What:** `dockPx = 56` hardcoded in `updateGridDimensions()`.
**Why bad:** Couples C++ grid math to specific QML layout. Dock removal requires C++ change.
**Instead:** After dock removal, only margins are deducted. Margins should be DPI-aware or at least a named constant.

### Anti-Pattern 3: Widget QML Importing Global Singletons for Navigation
**What:** Widgets calling `PluginModel.setActivePlugin()` directly.
**Current reality:** All widgets do this (AAStatusWidget, LauncherDock). It works because singletons are stable.
**For v0.6.1:** Acceptable. Document it as the convention. For future third-party plugins, consider a signal-based approach where WidgetHost mediates.

### Anti-Pattern 4: DPI-Unaware Pixel Targets
**What:** Using `targetCellW = 150` regardless of DPI.
**Why bad:** A 150px cell is 22mm on a 170 DPI screen but only 17mm on a 224 DPI screen. Touch targets shrink on high-DPI screens -- exactly backward from what you want.
**Instead:** Define cell targets in physical mm, convert to pixels using DPI.

---

## Suggested Build Order

Dependencies flow top-down. Each step is independently testable and leaves the app functional.

### Step 1: Extend WidgetDescriptor (C++ struct change)
- **Modify:** `WidgetTypes.hpp` -- add `category`, `userRemovable`, `description`
- **Modify:** `WidgetGridModel::data()` -- add `CategoryRole`, `UserRemovableRole`, `DescriptionRole`
- **Modify:** `WidgetGridModel::roleNames()` -- register new role names
- **Modify:** All registrations in `main.cpp` -- set category on each descriptor
- **Test:** Unit test for new roles
- **Risk:** None. New fields have defaults. Existing code compiles unchanged.
- **Why first:** Everything else needs the enriched descriptor.

### Step 2: DPI-Aware Grid Dimensions (C++ only)
- **Modify:** `DisplayInfo::updateGridDimensions()` -- mm-based cell targets using `computedDpi()`
- **Modify:** Remove `dockPx = 56` deduction (anticipating Step 5)
- **Test:** Unit tests with different window sizes and DPI values
- **Risk:** LOW. Clamping bounds (3..8 cols, 2..6 rows) prevent extremes. May need empirical tuning on Pi.
- **Why second:** Pure C++ change, no QML dependency. Must come before dock removal so grid rows increase to absorb the reclaimed space.

### Step 3: Create LauncherWidget (QML + registration)
- **Create:** `qml/widgets/LauncherWidget.qml`
- **Modify:** `main.cpp` -- register `org.openauto.launcher` descriptor with category="system"
- **Modify:** Default layout YAML -- place launcher widget on page 0
- **Test:** Visual verification on dev VM with `--geometry`. Plugin launch works.
- **Risk:** LOW. New widget, no existing code changed.
- **Why third:** Must exist before dock can be removed.

### Step 4: Remove LauncherDock (QML + C++ cleanup)
- **Delete:** `LauncherDock.qml`
- **Delete:** `LauncherModel.hpp/cpp`
- **Modify:** `HomeMenu.qml` -- remove `LauncherDock {}` from ColumnLayout
- **Modify:** `main.cpp` -- remove LauncherModel creation and QML context property
- **Modify:** `CMakeLists.txt` -- remove LauncherModel sources
- **Modify:** `YamlConfig` -- deprecate/remove `launcherTiles()`
- **Test:** Build succeeds. Home screen has no dock. LauncherWidget works.
- **Risk:** MEDIUM. Users with existing configs lose the dock. Migration: if no launcher widget in saved placements, auto-add one at boot.
- **Why fourth:** Dock replacement (Step 3) must be in place.

### Step 5: Widget Lifecycle Signals (QML only)
- **Modify:** `WidgetHost.qml` -- add 3 signals
- **Modify:** `HomeMenu.qml` -- emit activate/deactivate on page visibility, resize on cell change
- **Test:** Add `console.log` connections in ClockWidget, verify signals fire
- **Risk:** None. Purely additive. Existing widgets unaffected.
- **Why fifth:** Independent of Steps 1-4 but completes the framework.

### Step 6: Documentation
- **Create:** Plugin/widget convention document covering:
  - WidgetDescriptor field contract (all fields, meaning, defaults)
  - Size constraints and pixel-breakpoint conventions
  - Lifecycle signal usage
  - Navigation patterns
  - Category taxonomy
- **Why last:** Documents final state.

---

## Migration Concern: Existing User Configs

When the dock is removed, users with existing `launcher.tiles` YAML config and no launcher widget placement will lose their quick-launch buttons. Two mitigation options:

**Option A (recommended): Auto-inject at boot.** In `main.cpp`, after loading placements from YAML, check if any placement has `widgetId == "org.openauto.launcher"`. If not, and this is an upgrade from a pre-v0.6.1 config, auto-place the launcher widget on page 0 in the first available space. This is a one-time migration.

**Option B: Config schema version bump.** YAML schema v4 with a migration path from v3 that adds the launcher widget placement. Heavier machinery for a simple one-time change.

Option A is simpler and sufficient for a solo-maintainer project.

---

## Sources

All findings from direct codebase analysis on `dev/0.6` branch:
- `src/core/widget/WidgetTypes.hpp` -- WidgetDescriptor, GridPlacement, DashboardContributionKind
- `src/core/widget/WidgetRegistry.{hpp,cpp}` -- descriptor store and filtering
- `src/ui/WidgetGridModel.{hpp,cpp}` -- grid model, occupancy, page management
- `src/ui/DisplayInfo.{hpp,cpp}` -- DPI computation, grid dimension derivation
- `src/ui/LauncherModel.{hpp,cpp}` -- launcher tile model (to be removed)
- `qml/controls/UiMetrics.qml` -- DPI scaling, layout tokens, reference DPI 160
- `qml/applications/home/HomeMenu.qml` -- home screen layout with SwipeView + dock
- `qml/components/LauncherDock.qml` -- current dock implementation
- `qml/components/WidgetHost.qml` -- widget wrapper
- `qml/widgets/ClockWidget.qml`, `AAStatusWidget.qml` -- pixel-breakpoint pattern examples
- `src/core/plugin/IPlugin.hpp` -- plugin interface, widgetDescriptors()
- `src/main.cpp` -- widget registration, model wiring, DisplayInfo connections
- `.planning/PROJECT.md` -- v0.6.1 milestone definition and constraints
