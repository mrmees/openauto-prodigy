# Phase 10: Launcher Widget & Dock Removal - Context

**Gathered:** 2026-03-14
**Status:** Ready for planning

<domain>
## Phase Boundary

Remove the LauncherDock bottom bar, LauncherModel, and LauncherMenu entirely. Replace with a **reserved utility page** (the last home screen page) that holds pinned system widgets — Settings and AA launcher. The reserved page is undeletable and its pinned widgets are non-removable, but users can add their own widgets to remaining space. BT Audio and Phone have no dedicated launcher affordance; they are accessible only if the user places their content widgets.

</domain>

<decisions>
## Implementation Decisions

### Scope reframe from roadmap
- **No LauncherWidget** — the roadmap describes a "launcher widget with quick-launch tiles" but that's not what we're building
- Instead: a **reserved utility page** with two pinned system widgets (Settings + AA launcher)
- BT Audio and Phone are not user-facing launcher items — they're accessible only if the user adds their content widgets (Now Playing, Phone) to the grid
- The widget picker serves as the "app drawer" — users add widgets to access views

### Reserved utility page
- **The last home screen page is reserved** — it cannot be deleted (page delete logic in HomeMenu/WidgetGridModel must skip it)
- **Pinned system widgets:** Settings and AA launcher are placed by default and cannot be removed (edit mode hides the X badge on pinned widgets)
- **User-customizable:** Users CAN add their own widgets to remaining space on this page — it's not fully locked, just has pinned items
- **Page protection:** WidgetGridModel/HomeMenu must enforce undeletable status — currently any non-zero page can be removed via `HomeMenu.qml` and `WidgetGridModel.cpp`; this needs a `pinned`/`singleton` concept in the model

### Settings widget
- **Appearance:** Gear icon only, centered, no label — icon-only at all sizes
- **Size:** Min 1x1, resizable (icon scales up at larger spans)
- **Default placement:** Bottom-right corner of the reserved page
- **Action:** Tap navigates to settings via full view replacement (`PluginModel.setActivePlugin("") + ApplicationController.navigateTo(6)`)
- **Pinned:** Cannot be removed from the grid — edit mode hides X badge
- **Category:** `launcher` (existing canonical category from Phase 09)

### AA launcher widget
- **Appearance:** AA icon only, centered, no label
- **Size:** Min 1x1, resizable
- **Default placement:** On the reserved page (position at Claude's discretion)
- **Action:** Tap activates the AA plugin view (`PluginModel.setActivePlugin("org.openauto.android-auto")`)
- **Pinned:** Cannot be removed — same pinned semantics as Settings widget
- **Category:** `launcher`

### Pinned widget infrastructure
- `WidgetDescriptor` or `WidgetPlacement` needs a `pinned` flag (or equivalent) — widgets with this flag skip the X removal badge in edit mode and cannot be cleared via context menu
- The reserved page's undeletable status could be derived from "contains pinned widgets" or be an explicit page-level flag — Claude's discretion on implementation
- Pinned widgets can still be moved/resized within the reserved page (they're pinned to existence, not to position)

### Home-return behavior
- **No change** — navigating home returns to the last-viewed page (current behavior preserved)
- The reserved page is not special in navigation — it's just another page the user can swipe to

### Dock removal
- Delete `LauncherDock.qml` and its instantiation in `HomeMenu.qml` (currently at z=10 overlay)
- Full vertical space now available for grid widgets with no overlay

### LauncherModel removal
- Delete `LauncherModel.hpp`, `LauncherModel.cpp`
- Remove from `CMakeLists.txt` and `main.cpp` registration
- Neither new widget needs LauncherModel — Settings is a hardcoded action, AA launcher uses PluginModel

### LauncherMenu removal
- Delete `LauncherMenu.qml` (full-page launcher grid view)
- Remove any navigation routes that point to it
- No "app drawer" concept — widget picker replaces this

### Settings reachability context
- Settings is ALSO reachable via navbar short-holds (clock→settings, volume→audio settings, brightness→display settings) per `main.cpp` action registry
- The Settings widget is the **primary visible entry point**, not the only safety net — navbar holds are the fallback
- This means the pinned/non-removable constraint is about discoverability, not sole access

### Doc/config cleanup
- `launcher.tiles` config key and related language in `config-schema.md`, `plugin-api.md`, `development.md` become stale — plan should include cleanup pass

### Claude's Discretion
- Whether pinned flag lives on WidgetDescriptor (type-level) or WidgetPlacement (instance-level)
- How reserved page is identified (last page by convention, explicit flag, or derived from pinned widget presence)
- AA launcher widget position on the reserved page
- Fresh-install default seeding in YamlConfig — must explicitly place both pinned widgets on the reserved page
- Cleanup scope for stale launcher references in docs and config schema

</decisions>

<specifics>
## Specific Ideas

- The reserved page keeps the primary home screen (page 0) focused on at-a-glance info (clock, AA status, now playing, nav)
- The widget picker IS the app drawer — this is the conceptual replacement for LauncherMenu
- "The remainder of the existing launcher icons are either going away in lieu of other widgets (android auto), or just not necessary at the moment (bluetooth media and phone)"
- Page protection + pinned widgets is a new infrastructure concept that doesn't exist yet — WidgetTypes.hpp and WidgetGridModel need extension

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `LauncherDock.qml`: Contains `navigate:settings` action pattern and `PluginModel.setActivePlugin` call — both new widgets need similar navigation logic
- `WidgetDescriptor` (Phase 09): Category, description, icon fields ready for new widget registration — needs `pinned` field addition
- `WidgetPickerModel` (Phase 09): Category grouping — both widgets appear under "Launcher" category
- `PluginModel`: Exposes plugin views including AA — AA launcher widget uses `setActivePlugin("org.openauto.android-auto")`

### Established Patterns
- Widget QML files need `QT_QML_SKIP_CACHEGEN TRUE` in CMake
- Widgets registered in `main.cpp` via `WidgetDescriptor` with size constraints
- `MaterialIcon` uses `icon:` property with codepoints (gear = `\ue8b8`)
- Navbar action registry (`main.cpp:641-686`) already provides settings access via short-holds

### Integration Points
- `HomeMenu.qml` line 562: `LauncherDock` instantiation — delete
- `HomeMenu.qml` line ~745: Page deletion logic — must skip reserved page
- `WidgetGridModel.cpp` line ~285: Widget/page removal — must respect pinned flag
- `WidgetTypes.hpp`: Needs `pinned` field on WidgetDescriptor or WidgetPlacement
- `main.cpp`: LauncherModel construction/registration — remove; add new widget registrations
- `src/CMakeLists.txt`: LauncherModel source files — remove
- `YamlConfig.cpp` line ~191: Default widget placements — must seed both pinned widgets on reserved page
- `config-schema.md:93`, `plugin-api.md:121`, `development.md:258`: Stale launcher references — cleanup

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 10-launcher-widget-dock-removal*
*Context gathered: 2026-03-14*
