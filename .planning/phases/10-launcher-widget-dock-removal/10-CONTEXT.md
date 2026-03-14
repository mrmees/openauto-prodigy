# Phase 10: Launcher Widget & Dock Removal - Context

**Gathered:** 2026-03-14
**Status:** Ready for planning

<domain>
## Phase Boundary

Remove the LauncherDock bottom bar, LauncherModel, and LauncherMenu entirely. Replace with a **reserved utility page** (always the last home screen page) that holds singleton system widgets — Settings and AA launcher. The reserved page is undeletable, its system widgets are non-removable, and new user pages insert before it (never after). Users can add their own widgets to remaining space on the reserved page. BT Audio and Phone are plumbing-only plugins with no widgets and no launcher affordance — they expose full-screen views but have no home screen presence unless future phases add content widgets for them.

</domain>

<decisions>
## Implementation Decisions

### Scope reframe from roadmap
- **No LauncherWidget** — the roadmap describes a "launcher widget with quick-launch tiles" but that's not what we're building
- Instead: a **reserved utility page** with two singleton system widgets (Settings + AA launcher)
- BT Audio and Phone are plumbing-only plugins — `BtAudioPlugin` registers no widgets (`BtAudioPlugin.cpp:428`), `PhonePlugin` only exposes a full-screen view (`PhonePlugin.cpp:332`), `IPlugin::widgetDescriptors()` defaults to empty. They have no home screen presence and no launcher affordance.
- The widget picker serves as the "app drawer" — users add content widgets to access views that have them

### Reserved utility page
- **Always the last home screen page** — when users add new pages, new pages insert before the reserved page, never after it. The add-page flow (`HomeMenu.qml:699`, `WidgetGridModel.cpp:279`) currently appends to the end — must be changed to insert at `pageCount - 1`.
- **Undeletable** — page delete logic (`HomeMenu.qml:745`, `WidgetGridModel.cpp:285`) must skip the reserved page. It never shows a delete option.
- **Singleton system widgets:** Settings and AA launcher are seeded on first boot and cannot be removed (edit mode hides the X badge). These are **singletons** — only one instance exists, placed by the system, not by the user.
- **User-customizable:** Users CAN add their own widgets to remaining space on this page — it's not fully locked, just has singleton system items
- **Page protection:** WidgetGridModel/HomeMenu must enforce both undeletable page status and singleton widget non-removal

### Settings widget
- **Appearance:** Gear icon only, centered, no label — icon-only at all sizes
- **Size:** Min 1x1, resizable (icon scales up at larger spans)
- **Default placement:** Bottom-right corner of the reserved page
- **Action:** Tap navigates to settings via full view replacement (`PluginModel.setActivePlugin("") + ApplicationController.navigateTo(6)`)
- **Singleton:** System-seeded, non-removable, hidden from widget picker
- **Category:** `launcher` (existing canonical category from Phase 09)

### AA launcher widget
- **Appearance:** AA icon only, centered, no label
- **Size:** Min 1x1, resizable
- **Default placement:** On the reserved page (position at Claude's discretion)
- **Action:** Tap activates the AA plugin view (`PluginModel.setActivePlugin("org.openauto.android-auto")`)
- **Singleton:** System-seeded, non-removable, hidden from widget picker
- **Category:** `launcher`

### Singleton widget infrastructure
- `WidgetDescriptor` needs a `singleton` flag — singleton widgets are:
  1. **System-seeded** on first boot (placed in YamlConfig defaults)
  2. **Non-removable** — edit mode hides X badge, context menu hides "Clear" option
  3. **Hidden from widget picker** — `WidgetPickerModel` filters them out entirely. Users cannot place additional copies. Only the system-seeded instance exists.
  4. **Movable/resizable** — singleton means "exactly one instance, can't be deleted," not "frozen in place"
- The reserved page's undeletable status could be derived from "contains singleton widgets" or be an explicit page-level flag — Claude's discretion on implementation
- `WidgetPickerModel` currently has no filtering for placed widgets (`HomeMenu.qml:971`, `WidgetGridModel.hpp:48`) — needs singleton exclusion logic

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
- How reserved page is identified (last page by convention, explicit flag, or derived from singleton widget presence)
- AA launcher widget position on the reserved page
- Fresh-install default seeding in YamlConfig — must explicitly place both singleton widgets on the reserved page
- Cleanup scope for stale launcher references in docs and config schema
- Whether page-insert-before-reserved is enforced in WidgetGridModel (model-level) or HomeMenu (view-level)

</decisions>

<specifics>
## Specific Ideas

- The reserved page keeps the primary home screen (page 0) focused on at-a-glance info (clock, AA status, now playing, nav)
- The widget picker IS the app drawer — this is the conceptual replacement for LauncherMenu
- "The remainder of the existing launcher icons are either going away in lieu of other widgets (android auto), or just not necessary at the moment (bluetooth media and phone)"
- Singleton widgets + reserved page is a new infrastructure concept — WidgetTypes.hpp, WidgetGridModel, WidgetPickerModel, and HomeMenu all need extension
- BT Audio and Phone are plumbing-only: full-screen views exist but no widgets registered, no home screen presence

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `LauncherDock.qml`: Contains `navigate:settings` action pattern and `PluginModel.setActivePlugin` call — both new widgets need similar navigation logic
- `WidgetDescriptor` (Phase 09): Category, description, icon fields ready for new widget registration — needs `singleton` flag addition
- `WidgetPickerModel` (Phase 09): Category grouping — needs singleton filtering (hide Settings/AA launcher from picker)
- `PluginModel`: Exposes plugin views including AA — AA launcher widget uses `setActivePlugin("org.openauto.android-auto")`

### Established Patterns
- Widget QML files need `QT_QML_SKIP_CACHEGEN TRUE` in CMake
- Widgets registered in `main.cpp` via `WidgetDescriptor` with size constraints
- `MaterialIcon` uses `icon:` property with codepoints (gear = `\ue8b8`)
- Navbar action registry (`main.cpp:641-686`) already provides settings access via short-holds

### Integration Points
- `HomeMenu.qml` line 562: `LauncherDock` instantiation — delete
- `HomeMenu.qml` line ~745: Page deletion logic — must skip reserved page
- `HomeMenu.qml` line ~699: Add-page flow — must insert before reserved page, not append
- `WidgetGridModel.cpp` line ~279: Add-page in model — same insert-before-reserved fix
- `WidgetGridModel.cpp` line ~285: Widget/page removal — must respect singleton flag and reserved page
- `WidgetTypes.hpp`: Needs `singleton` flag on WidgetDescriptor
- `HomeMenu.qml` line ~971: Widget placement from picker — no change needed (singletons hidden from picker)
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
