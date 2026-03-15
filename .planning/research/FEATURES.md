# Feature Landscape

**Domain:** Widget framework conventions, launch bar removal, DPI grid layout refinement for automotive head unit
**Researched:** 2026-03-14
**Overall confidence:** HIGH (well-established patterns from Android, Home Assistant, Qt, automotive UX, plus direct codebase inspection)

---

## Context: What Already Exists

This milestone refines and documents an already-functional widget system. The grid, widgets, and plugin architecture shipped in v0.5.2-v0.6. The work here is about formalizing conventions, removing redundant UI, and tuning layout math.

| Component | Status | v0.6.1 Work |
|-----------|--------|-------------|
| `WidgetDescriptor` | Has min/max/default cols/rows, id, displayName, iconName, qmlComponent, pluginId, contributionKind, defaultConfig | Formalize as a contract spec. Add category, description, capability flags. Document all fields. |
| `WidgetRegistry` | registerWidget, widgetsFittingSpace, descriptorsByKind | Enforce size constraints on registration. Audit widgetsFittingSpace filtering. |
| `WidgetGridModel` | Full grid: place/move/resize/remove, occupancy tracking, multi-page, YAML persistence | Verify resize clamping against descriptor min/max. Add signals for lifecycle hooks. |
| `LauncherDock` | Bottom dock with AA/BT Media/Phone/Settings buttons | Remove entirely. Launcher tiles + navbar home button replace it. |
| `UiMetrics` | DPI-based scaling from EDID, grid density auto-derived | Tune grid column/row formula across display range. |
| Widget QML (Clock, AA Status, Now Playing, Navigation, AA LiveSurface) | 5 widgets with pixel breakpoints for responsive sizing | Refactor to formalized descriptor fields. Add page visibility guards. |

---

## Table Stakes

Features users and developers expect. Missing = the widget system feels half-baked or the dock removal feels like a regression.

| Feature | Why Expected | Complexity | Dependencies | Notes |
|---------|--------------|------------|--------------|-------|
| **Formal widget manifest spec (documented WidgetDescriptor)** | Android widgets have `AppWidgetProviderInfo` XML. HA cards have type/size declarations. Every mature widget framework has a manifest spec. Without this, third-party devs are guessing at struct fields by reading source code. | LOW | WidgetDescriptor struct (exists) | Write `docs/plugin-widget-guide.md`. Document every field, valid ranges, defaults. This is the single most important deliverable for enabling community widgets. |
| **Widget size constraint enforcement on resize** | Android hard-stops resize at `minResizeWidth`/`maxResizeWidth`. HA clips cards to declared span range. Users expect that dragging a resize handle stops at reasonable bounds, not that a clock widget can be stretched to 6x4. | LOW | WidgetGridModel.resizeWidget(), WidgetDescriptor min/max fields | Verify `resizeWidget()` checks `descriptor.minCols/maxCols/minRows/maxRows` before accepting. If it doesn't, add clamping. Small but critical correctness item. |
| **Widget size constraint enforcement on placement** | When placing a widget via the picker, the widget should be placed at its `defaultCols x defaultRows` size. If the available space is smaller than `minCols x minRows`, placement should fail gracefully rather than creating a broken widget. | LOW | WidgetGridModel.placeWidget(), WidgetRegistry.widgetsFittingSpace() | `widgetsFittingSpace()` already filters by available space. Verify the picker uses it. Verify `placeWidget()` rejects placements smaller than min. |
| **Dock removal with equivalent access** | Removing the LauncherDock (AA/BT Media/Phone/Settings buttons) requires that every action it provides remains reachable within one tap. If users lose quick access to AA or Settings, the product feels regressed. | MED | LauncherDock.qml, launcher tiles, navbar home button, PluginModel | Launcher tiles already provide AA, BT Audio, Phone, Settings, EQ. Navbar home button (center control, tap) already returns to home/launcher. The dock is visually redundant. Removal is safe but must be verified on Pi with real touch. |
| **Grid density that looks right on tested displays** | A 7" screen at 800x480 (DPI ~133) should show fewer, larger cells than a 10" screen at 1080p (DPI ~220). The formula must produce usable widget sizes at each tested resolution. Home Assistant's grid uses fixed column counts that respond to available width. Android launchers use 4-5 columns on phones, 5-6 on tablets. | MED | UiMetrics auto-derived grid density, DisplayInfo EDID | Grid density is already auto-derived. Refinement means testing the formula output at 800x480/7", 1024x600/7", 1920x1080/10" and tuning if cell counts feel wrong. Document the formula and its expected outputs. |
| **Widget lifecycle: create and destroy** | Every widget framework (Android, Flutter, Qt, Dojo) has explicit create/destroy lifecycle. Widgets must initialize cleanly when placed and release resources when removed. | LOW | QML Loader already handles this | QML Loader's `active` property already creates/destroys widget QML. Formalizing means documenting the contract: "Your widget QML's `Component.onCompleted` is your create hook. `Component.onDestruction` is your destroy hook." No new code needed -- just documentation. |
| **Plugin-to-widget registration path** | `IPlugin::widgetDescriptors()` exists but is (based on v0.6 refactor) the formal path. Ensure PluginManager calls it after `initialize()` and registers descriptors automatically. Plugins should not call `WidgetRegistry::registerWidget()` directly. | LOW | IPlugin::widgetDescriptors() (exists), PluginManager, WidgetRegistry | Verify this path works end-to-end. If plugins still register directly, refactor to use widgetDescriptors(). |

---

## Differentiators

Features that set the product apart. Not expected by default, but add meaningful value.

| Feature | Value Proposition | Complexity | Dependencies | Notes |
|---------|-------------------|------------|--------------|-------|
| **Widget category field** | Android uses `widgetCategory`. HA uses sections. Grouping widgets by domain (time, media, navigation, status) in the picker makes selection faster as the catalog grows beyond 5 widgets. Even with 5 widgets, categories provide useful context in the picker. | LOW | WidgetDescriptor struct | Add `QString category` field. Simple string. Initial categories: "time", "media", "navigation", "status". Picker can optionally group by category now or in a future milestone. |
| **Widget capability flags** | Android has `resizeMode` and `widgetFeatures`. Flags like `isSystem` (pinned, cannot be removed), `isResizable`, `isRemovable` let the host enforce policy without hardcoding per-widget behavior. | LOW | WidgetDescriptor struct | Add individual bools: `isSystem`, `isResizable`, `isRemovable`, `isHiddenFromPicker`. All default to the permissive direction (removable=true, resizable=true, system=false, hidden=false). WidgetHost hides the X badge when `isRemovable=false`. |
| **Breakpoint-based widget content adaptation** | Android 12+ provides different layouts at different `SizeF` ranges. Prodigy already does this -- Clock and AA Status widgets use pixel breakpoints to switch between compact/expanded content. | LOW (documentation) | Existing widget QML patterns | This is already implemented. Formalize as a documented convention: "Use `width`/`height` bindings or `colSpan`/`rowSpan` roles to conditionally show/hide content elements." Provide examples from existing widgets. |
| **Page visibility optimization** | When a widget is on a non-visible SwipeView page, it should know so it can pause timers and skip updates. Android handles this with `onEnabled`/`onDisabled`. Clock widget currently ticks every second even when off-screen. | LOW | WidgetHost.qml, HomeMenu SwipeView | WidgetHost gets a `pageVisible` bool property derived from SwipeView.isCurrentItem. Widget QML binds Timer.running to `pageVisible`. Clock saves CPU when not visible. |
| **Widget description in descriptor** | Android 12+ shows description in widget picker. On a 7" touch screen, name + icon alone may not be enough for users to know what an unfamiliar widget does. | LOW | WidgetDescriptor struct | Add `QString description`. Shown in picker if space permits. Low effort, modest value. |
| **Widget error placeholder** | When widget QML fails to load (wrong path, missing resource), WidgetHost currently shows a blank glass card. A visible placeholder with widget name + error icon tells the user something is wrong vs. just empty space. | LOW | WidgetHost.qml Loader.status | Check Loader.Error state, show fallback Item with displayName and warning icon. Fetches name from WidgetRegistry. |
| **Widget lifecycle hooks on IPlugin** | Android AppWidgetProvider has `onAppWidgetOptionsChanged` (resize callback). Plugins providing data-driven widgets (navigation) could benefit from knowing when their widget is created, resized, or destroyed in C++. | MED | IPlugin, PluginManager, WidgetGridModel signals | New virtual methods: `onWidgetCreated(instanceId, widgetId)`, `onWidgetDestroyed(instanceId)`, `onWidgetResized(instanceId, newCols, newRows)`. Default no-ops for backward compat. Only add if a concrete plugin needs C++ notification -- may be documentation-only if QML signals suffice. |
| **Documented plugin-widget contract** | Third-party devs need a single document explaining: register a WidgetDescriptor, provide a QML component, expect these context properties, follow this lifecycle. Without docs, the architecture exists but is inaccessible. | LOW | All table stakes features | Write `docs/plugin-widget-guide.md` with worked examples from built-in widgets. High value per effort. |

---

## Anti-Features

Features to explicitly NOT build in this milestone.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| **Widget store / marketplace** | Adds cloud dependency, account management, distribution complexity. Violates offline-only constraint. Solo maintainer cannot moderate a store. | Document how to install plugin `.so` + `plugin.yaml` manually. Community shares widgets via GitHub repos. |
| **Widget-to-widget communication** | Message buses between widgets add coupling and debugging complexity. No comparable system (Android, HA, KDE Plasma) allows direct widget-to-widget communication -- they all use shared state/services. | Widgets read from shared services (MediaStatusService, AndroidAutoRuntimeBridge, ThemeService) via context properties. This is the correct and established pattern. |
| **Animated widget resize transitions** | Smooth size animation during drag-resize is janky on Pi GPU (already discovered in v0.5.3 -- ghost rectangle is the solution). | Keep ghost rectangle preview. Widget re-renders at new size after drop. Instant swap, no animation. |
| **Per-widget theme overrides** | Allowing each widget its own color scheme fragments visual consistency. No automotive system does this. Android widgets inherit the launcher theme. | Widgets use ThemeService. Per-widget opacity is the only visual customization knob. |
| **Custom grid cell aspect ratios** | Configurable cell aspect ratios (e.g., "wide cells") complicate occupancy math and make widget sizing unpredictable. Android uses uniform grid cells. | Cells are derived from screen dimensions / grid density. Widgets span multiple cells for non-square shapes. Uniform cells are simpler and more predictable. |
| **Widget configuration dialog per-instance** | Android has optional `configure` Activity. Building per-instance config editing UI and storage for this milestone is premature -- no widget currently needs it. `defaultConfig` field exists in WidgetDescriptor for future use. | Defer until a concrete widget needs per-instance config. The descriptor field exists; the UI does not. |
| **Drag-to-reorder pages** | Multi-page exists with SwipeView. Page reordering adds interaction complexity (long-press ambiguity with widget drag) for minimal value with 2-3 pages. | Pages auto-clean when empty. Users add/remove pages. Order is creation order. Good enough. |
| **AA LiveSurface widget embedding** | Embedding a live AA projection surface as a widget would be unique but requires secondary video surface or frame extraction from the AA video pipeline. Genuinely hard, already marked deferred in `WidgetTypes.hpp`. | Keep `DashboardContributionKind::LiveSurfaceWidget` declared. Implementation is post-v1.0 at earliest. |

---

## Feature Dependencies

```
Widget manifest spec documentation
    └──> All descriptor field additions (category, description, flags)
    └──> Plugin-widget contract documentation (docs/plugin-widget-guide.md)

WidgetDescriptor extensions (category, description, flags)
    └──> Widget picker filtering by capability (isHiddenFromPicker)
    └──> WidgetHost edit mode behavior (isRemovable, isResizable, isSystem)

Resize constraint enforcement (verify WidgetGridModel clamps to min/max)
    └──> Requires WidgetDescriptor min/max fields (already exist)
    └──> Enables safe resize behavior for all current and future widgets

Dock removal (delete LauncherDock.qml)
    └──> Requires: launcher tiles cover all dock actions (AA, BT, Phone, Settings)
    └──> Requires: navbar home button returns to launcher (already works)
    └──> Frees vertical space for widget grid (grid gets taller)
    └──> May require grid row count increase to fill reclaimed space

Grid density DPI refinement
    └──> Requires: test data from multiple display sizes
    └──> Informs: widget size constraint validation (min size must be usable at lowest density)
    └──> Dock removal may change grid height calculation (dock was fixed-height below grid)

Page visibility optimization
    └──> Requires: WidgetHost gets pageVisible property
    └──> Enables: Clock timer pause, nav update skip on non-visible pages

Plugin-to-widget registration audit
    └──> Requires: IPlugin::widgetDescriptors() consumed by PluginManager
    └──> Enables: removing direct WidgetRegistry calls from plugin initialize() bodies
```

---

## MVP Recommendation

Prioritize for v0.6.1:

1. **Widget manifest spec formalization** -- Document every WidgetDescriptor field as a formal contract. Add `category` and `description` fields (trivial struct additions). This is zero-risk, high-value work that enables everything else and is the primary deliverable for third-party developer enablement.

2. **Resize constraint enforcement audit** -- Verify `resizeWidget()` and `placeWidget()` clamp against descriptor `minCols/maxCols/minRows/maxRows`. Fix if not enforced. Small code change, critical correctness item. Every widget framework enforces declared size bounds.

3. **Launch bar removal** -- Delete `LauncherDock.qml` and its usage in `HomeMenu.qml`. Verify launcher tiles provide equivalent access to AA, BT Audio, Phone, Settings. Confirm navbar home button returns to launcher. Reclaim vertical space for the widget grid. Test on Pi with real touch.

4. **Grid density DPI refinement** -- Test grid column/row calculations at 800x480/7", 1024x600/7", 1920x1080/10". Tune the formula if cell counts produce unusably small or wastefully large cells. Document the formula and expected outputs per display.

5. **Plugin-widget registration path audit** -- Verify `IPlugin::widgetDescriptors()` is the registration path consumed by PluginManager. Remove any direct `WidgetRegistry::registerWidget()` calls from plugin `initialize()` bodies if they exist.

6. **Page visibility optimization** -- Add `pageVisible` property to WidgetHost. Clock widget binds `Timer.running` to it. Other widgets add visibility guards where appropriate. Low effort, measurable CPU savings.

7. **Plugin-widget contract documentation** -- Write `docs/plugin-widget-guide.md` covering: WidgetDescriptor fields, registration path, QML component requirements, context properties available, lifecycle (create via Component.onCompleted, destroy via Component.onDestruction), size adaptation patterns.

Defer:
- **Widget capability flags (isSystem, isRemovable, etc.)**: Add the fields when a concrete UX need arises (e.g., preventing removal of AA Status widget). Currently all widgets are equally manageable.
- **Widget lifecycle hooks on IPlugin C++**: Only add if a plugin's C++ code concretely needs create/resize/destroy notification. QML lifecycle (Component.onCompleted/onDestruction) plus property bindings handle most cases.
- **Widget error placeholder**: Nice UX improvement, not blocking. Add when convenient.
- **Widget picker category grouping**: Add the category field now, build grouped picker UI when widget catalog exceeds ~8 widgets.

---

## Comparison: Android AppWidget vs Prodigy Widget Architecture

Reference for design decisions. Prodigy should be simpler -- in-process QML widgets don't need Android's remote-process complexity.

| Concept | Android AppWidget | Prodigy Widget | Notes |
|---------|-------------------|----------------|-------|
| Manifest | XML `<appwidget-provider>` | `WidgetDescriptor` C++ struct | Android's XML is more declarative; Prodigy's is programmatic. Both work. |
| Size units | dp + grid cells (`targetCellWidth`) | Grid cells directly | Prodigy is simpler. No dp conversion needed. |
| Size constraints | minWidth/Height, minResizeW/H, maxResizeW/H | minCols/Rows, maxCols/Rows | Same concept, cell-based in Prodigy. |
| Responsive layouts | `Map<SizeF, RemoteViews>` breakpoints | QML width/height bindings with conditionals | Prodigy's approach is more natural for in-process QML. |
| Lifecycle: create | onEnabled (first), onUpdate | Component.onCompleted (QML) | Android is process-boundary; Prodigy is in-process. |
| Lifecycle: resize | onAppWidgetOptionsChanged | QML width/height binding updates | Prodigy gets this for free from QML property system. |
| Lifecycle: destroy | onDeleted, onDisabled (last) | Component.onDestruction (QML) | Same concept. |
| Update mechanism | Host-driven updatePeriodMillis | Widget-driven Timer + signal bindings | Prodigy's approach is better for in-process widgets. |
| Inter-widget comms | None (intentional) | None (intentional) | Both use shared services/state instead. |
| Configuration | Optional configure Activity | Per-instance config deferred | Android's is mature; Prodigy can add when needed. |

Key takeaway: Android's `AppWidgetProviderInfo` is the gold standard for widget manifest conventions. Prodigy's `WidgetDescriptor` already covers the essential fields (sizing, defaults). The gap is documentation and a few convenience fields (category, description), not fundamental architecture.

---

## Sources

- [Android Create a Simple Widget](https://developer.android.com/develop/ui/views/appwidgets) -- HIGH confidence, authoritative manifest spec with AppWidgetProviderInfo attributes
- [Android Responsive Widget Layouts](https://developer.android.com/develop/ui/views/appwidgets/layouts) -- HIGH confidence, breakpoint-based sizing patterns and SizeF mapping
- [AppWidgetProviderInfo API Reference](https://developer.android.com/reference/android/appwidget/AppWidgetProviderInfo) -- HIGH confidence, complete attribute list
- [Home Assistant Dashboard Grid System](https://www.home-assistant.io/blog/2024/03/04/dashboard-chapter-1/) -- MEDIUM confidence, comparable grid/section design for dashboards
- [Qt Scalability Documentation](https://doc.qt.io/qt-6/scalability.html) -- HIGH confidence, DPI scaling patterns for QML
- [KDAB: Scalable UIs in QML](https://www.kdab.com/scalable-uis-qml/) -- MEDIUM confidence, practical scaling patterns for embedded/kiosk
- [Android Automotive Car UI](https://source.android.com/docs/automotive/hmi/car_ui) -- MEDIUM confidence, car UI framework patterns
- Codebase: `src/core/widget/WidgetTypes.hpp`, `src/core/widget/WidgetRegistry.hpp`, `src/ui/WidgetGridModel.hpp`, `qml/controls/UiMetrics.qml`, `qml/components/LauncherDock.qml`, `qml/applications/home/HomeMenu.qml` -- HIGH confidence, direct source inspection

---

*Feature research for: OpenAuto Prodigy v0.6.1 Widget Framework & Layout Refinement*
*Researched: 2026-03-14*
