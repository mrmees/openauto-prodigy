# Phase 10: Launcher Widget & Dock Removal - Context

**Gathered:** 2026-03-14
**Status:** Ready for planning

<domain>
## Phase Boundary

Remove the LauncherDock bottom bar and LauncherModel entirely. Create a Settings widget (1x1, gear icon) as the sole replacement — users navigate to AA, BT Audio, and Phone through their existing content widgets on the home screen, not through a launcher. The full-page LauncherMenu view is also deleted. Settings widget is non-removable to guarantee settings access.

</domain>

<decisions>
## Implementation Decisions

### Scope reframe from roadmap
- **No LauncherWidget** — the roadmap describes a "launcher widget with quick-launch tiles" but that's not what we're building
- Instead: a **Settings widget** (single-action, opens settings view) is the only new widget
- AA, BT Audio, Phone views are already reachable via their content widgets (AA Status, Now Playing, etc.) — no launcher tiles needed for them
- The widget picker serves as the "app drawer" now — users add widgets to access views

### Settings widget
- **Appearance:** Gear icon only, centered, no label — icon-only at all sizes
- **Size:** Min 1x1, resizable (icon scales up at larger spans)
- **Default placement:** Bottom-right corner of **page 2** — keeps main home screen clean
- **Action:** Tap navigates to settings via full view replacement (same as current dock behavior: `PluginModel.setActivePlugin("") + ApplicationController.navigateTo(6)`)
- **Non-removable:** Edit mode does NOT show the X removal badge on the Settings widget — guarantees settings access at all times
- **Category:** `launcher` (existing canonical category from Phase 09)

### Dock removal
- Delete `LauncherDock.qml` and its instantiation in `HomeMenu.qml` (currently at z=10 overlay)
- Full vertical space now available for grid widgets with no overlay

### LauncherModel removal
- Delete `LauncherModel.hpp`, `LauncherModel.cpp`
- Remove from `CMakeLists.txt` and `main.cpp` registration
- Settings widget reads `PluginModel` directly (or doesn't need a model at all — it's a single hardcoded action)

### LauncherMenu removal
- Delete `LauncherMenu.qml` (full-page launcher grid view)
- Remove any navigation routes that point to it
- No "app drawer" concept — widget picker replaces this

### Reachability
- **Settings:** Always reachable via non-removable Settings widget on page 2
- **AA, BT Audio, Phone:** Optional — users place content widgets to access these views; removing a widget just means re-adding it via the widget picker
- **No navbar changes** — clock/home tap still goes to home screen, no long-press additions

### Navbar
- No changes to navbar behavior in this phase
- Tap clock/home = navigate to home (unchanged)

### Claude's Discretion
- Whether Settings widget needs a WidgetDescriptor registration or can be simpler
- How to implement non-removable flag (WidgetDescriptor field vs hardcoded check)
- Cleanup of any remaining LauncherModel/LauncherDock references in tests, docs, etc.
- Whether the `navigate:settings` action string pattern should be preserved or simplified for a single-action widget

</decisions>

<specifics>
## Specific Ideas

- Settings widget on page 2 keeps the primary home screen focused on at-a-glance info (clock, AA status, now playing, nav)
- The widget picker IS the app drawer — this is the conceptual replacement for LauncherMenu
- "The remainder of the existing launcher icons are either going away in lieu of other widgets (android auto), or just not necessary at the moment (bluetooth media and phone)"

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `LauncherDock.qml`: Contains the `navigate:settings` action pattern and `PluginModel.setActivePlugin` call — Settings widget needs the same navigation logic
- `WidgetDescriptor` (Phase 09): Category, description, icon fields ready for Settings widget registration
- `WidgetPickerModel` (Phase 09): Category grouping — Settings widget will appear under "Launcher" category
- `PluginModel`: Already exposes all plugin views — LauncherWidget was going to read this, but Settings widget doesn't need it (hardcoded single action)

### Established Patterns
- Widget QML files need `QT_QML_SKIP_CACHEGEN TRUE` in CMake
- Widgets registered in `main.cpp` via `WidgetDescriptor` with size constraints
- `MaterialIcon` uses `icon:` property with codepoints (gear = `\ue8b8`)

### Integration Points
- `HomeMenu.qml` line 562: `LauncherDock` instantiation — delete this
- `Shell.qml`: May reference LauncherDock or dock-related properties — verify
- `main.cpp`: LauncherModel construction and QML registration — remove
- `src/CMakeLists.txt`: LauncherModel source files — remove
- Default widget placements in `YamlConfig` or startup code — add Settings widget to page 2 defaults

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 10-launcher-widget-dock-removal*
*Context gathered: 2026-03-14*
