# Architecture Patterns

**Domain:** Automotive-minimal UI redesign and settings reorganization for Qt 6 + QML head unit
**Researched:** 2026-03-02

## Current Architecture Summary

The existing QML architecture is well-structured and most of it survives this milestone. The key components:

```
Shell.qml (ColumnLayout)
  TopBar          (8% height, title + clock)
  Content Area    (fill, one of:)
    LauncherMenu  (GridLayout of Tiles via LauncherModel)
    SettingsMenu  (StackView: flat list -> individual pages)
    Plugin Views  (loaded by C++ PluginViewHost)
  NavStrip        (12% height, RowLayout of icon buttons)
```

**Navigation model:** `ApplicationController.navigateTo(int)` for built-in screens (0=launcher, 6=settings). `PluginModel.setActivePlugin(id)` for plugins. Settings uses its own internal `StackView` for list-to-detail navigation with `ApplicationController.requestBack()` handling.

**Control library:** 7 reusable controls (`SettingsToggle`, `SettingsSlider`, `SegmentedButton`, `FullScreenPicker`, `SectionHeader`, `SettingsListItem`, `ReadOnlyField`) all using `UiMetrics` for sizing and `ThemeService` for colors. All settings controls bind to `ConfigService` via `configPath` property.

**Theme colors available:** 13 Q_PROPERTYs on ThemeService -- backgroundColor, highlightColor, controlBackgroundColor, controlForegroundColor, normalFontColor, specialFontColor, descriptionFontColor, barBackgroundColor, controlBoxBackgroundColor, gaugeIndicatorColor, iconColor, sideWidgetBackgroundColor, barShadowColor. No new colors needed for the redesign.

**Current settings structure:** SettingsMenu.qml builds a flat `ListModel` imperatively in `Component.onCompleted` with 2 section groups ("General" with 5 items, "Companion" with 1 item) plus dynamically appended plugin settings and an About entry. Each item pushes a dedicated Component onto the StackView.

## Recommended Architecture

### Strategy: Evolutionary, Not Revolutionary

The existing architecture is sound. The redesign is a **visual language change + settings restructuring**, not an architecture overhaul. No new C++ services needed. No navigation model changes. The StackView pattern in SettingsMenu already supports the needed hierarchy depth.

### Component Change Map

```
UNCHANGED (no modifications needed):
  Shell.qml             -- layout structure stays
  Wallpaper.qml         -- already works
  GestureOverlay.qml    -- unrelated to settings
  PairingDialog.qml     -- unrelated
  NotificationArea.qml  -- unrelated
  MaterialIcon.qml      -- stays as-is
  Icon.qml              -- stays as-is (unused by settings)
  NormalText.qml        -- stays as-is
  SpecialText.qml       -- stays as-is
  EqBandSlider.qml      -- functional internals stay, visual tweaks only

MODIFIED (visual refresh, same public API):
  UiMetrics.qml         -- add a few new sizing metrics
  TopBar.qml            -- visual polish (spacing, font weight)
  NavStrip.qml          -- add EQ shortcut button, visual polish
  SettingsListItem.qml  -- restyle for automotive-minimal
  SectionHeader.qml     -- restyle (bolder, consistent divider)
  SettingsToggle.qml    -- visual refresh (touch target, styling)
  SettingsSlider.qml    -- visual refresh (track/thumb styling)
  SegmentedButton.qml   -- visual refresh (touch targets)
  FullScreenPicker.qml  -- visual refresh (bottom sheet styling)
  ReadOnlyField.qml     -- visual refresh + read-only clarity
  InfoBanner.qml        -- visual refresh
  Tile.qml              -- visual refresh (flatter, less boxy)
  LauncherMenu.qml      -- visual refresh (larger tiles, spacing)

REWRITTEN (structural changes):
  SettingsMenu.qml      -- new 2-level category model + subcategory navigation

NEW FILES:
  SettingsCategoryList.qml     -- top-level category list component
  AudioSettingsCategory.qml    -- subcategory page for Audio (Output/EQ)
  AndroidAutoSettings.qml      -- merged AA page (from Connection + Video)
  ConnectivitySettings.qml     -- BT + WiFi AP (from Connection)
  SystemAboutSettings.qml      -- merged System + About

DELETED (content migrated):
  ConnectionSettings.qml       -- split into AA + Connectivity
  VideoSettings.qml            -- absorbed into AA
  SystemSettings.qml           -- merged into SystemAbout
  AboutSettings.qml            -- merged into SystemAbout
```

### Settings Reorganization Architecture

**Current:** Flat `ListModel` with section headers pushes to individual page Components via StackView.

**Target:** Two-level navigation. Top level shows 6 categories. Tapping a category pushes a page that either shows settings directly (for simple categories) or shows sub-items that push to detail pages.

```
SettingsMenu.qml (StackView, depth management + title sync)
  initialItem: SettingsCategoryList.qml
    "Android Auto"    -> AndroidAutoSettings.qml (absorbs Connection AA + Video)
    "Display"         -> DisplaySettings.qml (existing, visual refresh only)
    "Audio"           -> AudioSettingsCategory.qml (subcategory page)
      "Output & Input"  -> AudioSettings.qml (existing, visual refresh only)
      "Equalizer"       -> EqSettings.qml (existing, visual refresh only)
    "Connectivity"    -> ConnectivitySettings.qml (BT + WiFi AP from Connection)
    "Companion"       -> CompanionSettings.qml (existing, unchanged)
    "System / About"  -> SystemAboutSettings.qml (merges System + About)
    [Plugin settings appended dynamically, same as current]
```

**Key architectural decision:** Audio needs a subcategory page because EQ is a complex full-screen experience (10 vertical sliders + stream tabs + presets) that does not belong inline with volume sliders. All other categories can be flat (single page of controls).

#### SettingsMenu.qml Rewrite

The current SettingsMenu builds its model imperatively and maps pageIds to Components. The rewrite keeps the same StackView + back-handling pattern but replaces the flat list with a category component:

```qml
// SettingsMenu.qml (rewritten)
Item {
    id: settingsMenu

    Component.onCompleted: ApplicationController.setTitle("Settings")

    Connections {
        target: ApplicationController
        function onBackRequested() {
            if (settingsStack.depth > 1) {
                settingsStack.pop()
                updateTitle()
                ApplicationController.setBackHandled(true)
            }
        }
    }

    function updateTitle() {
        if (settingsStack.depth === 1)
            ApplicationController.setTitle("Settings")
        // Deeper titles set by openPage / openSubPage
    }

    // Direct-access function for NavStrip EQ shortcut
    function openDirectPage(pageId) {
        // Reset to category list first
        while (settingsStack.depth > 1)
            settingsStack.pop(StackView.Immediate)

        if (pageId === "eq") {
            ApplicationController.setTitle("Settings > Audio > Equalizer")
            settingsStack.push(eqPage)
        }
    }

    StackView {
        id: settingsStack
        anchors.fill: parent
        initialItem: categoryListComp
    }

    Component { id: categoryListComp; SettingsCategoryList { /* signals */ } }
    Component { id: aaPage; AndroidAutoSettings {} }
    Component { id: displayPage; DisplaySettings {} }
    Component { id: audioPage; AudioSettingsCategory { /* sub-navigation signals */ } }
    Component { id: connectivityPage; ConnectivitySettings {} }
    Component { id: companionPage; CompanionSettings {} }
    Component { id: systemAboutPage; SystemAboutSettings {} }
    Component { id: eqPage; EqSettings {} }
    Component { id: audioDetailPage; AudioSettings {} }

    function openPage(pageId, title) {
        ApplicationController.setTitle("Settings > " + title)
        settingsStack.push(pages[pageId])
    }

    property var pages: {
        "aa": aaPage, "display": displayPage, "audio": audioPage,
        "connectivity": connectivityPage, "companion": companionPage,
        "system-about": systemAboutPage
    }
}
```

The critical difference from the current implementation: `openDirectPage("eq")` allows the NavStrip to jump directly to EQ without traversing the category hierarchy. It resets the StackView to depth 1 first (preventing stale state), then pushes EQ directly.

#### SettingsCategoryList.qml (NEW)

Replaces the flat list. Each category is a full-width row with icon + label + optional status subtitle + chevron. Reuses `SettingsListItem` for visual consistency.

```qml
// SettingsCategoryList.qml
ListView {
    signal categorySelected(string pageId, string title)

    model: ListModel {
        ListElement { icon: "\ue9b2"; label: "Android Auto"; pageId: "aa" }
        ListElement { icon: "\ueb97"; label: "Display"; pageId: "display" }
        ListElement { icon: "\ue050"; label: "Audio"; pageId: "audio" }
        ListElement { icon: "\ue1a7"; label: "Connectivity"; pageId: "connectivity" }
        ListElement { icon: "\ue324"; label: "Companion"; pageId: "companion" }
        ListElement { icon: "\uf8cd"; label: "System / About"; pageId: "system-about" }
    }

    delegate: SettingsListItem {
        icon: model.icon
        label: model.label
        onClicked: categorySelected(model.pageId, model.label)
    }

    // Plugin settings appended dynamically (same PluginModel iteration as current)
}
```

No C++ model needed. The category list is 6 static items plus dynamically appended plugin settings (same pattern as the current code).

#### AudioSettingsCategory.qml (NEW)

A subcategory page listing two items that push deeper:

```qml
// AudioSettingsCategory.qml
ListView {
    signal subpageRequested(string subPageId, string title)

    model: ListModel {
        ListElement { icon: "\ue050"; label: "Output & Input"; subPageId: "audio-detail" }
        ListElement { icon: "\ue429"; label: "Equalizer"; subPageId: "eq" }
    }

    delegate: SettingsListItem {
        icon: model.icon
        label: model.label
        onClicked: subpageRequested(model.subPageId, model.label)
    }
}
```

This is the only category that needs a subcategory level. SettingsMenu handles the sub-push by listening to the signal and pushing the appropriate Component.

#### AndroidAutoSettings.qml (NEW -- merges content)

Absorbs relevant sections from current `ConnectionSettings.qml` and `VideoSettings.qml`:

| Section | Source | Controls |
|---------|--------|----------|
| Connection | ConnectionSettings | Auto-connect toggle, TCP Port |
| Protocol Capture | ConnectionSettings | Enable toggle, format selector, include media toggle, capture path |
| Video Playback | VideoSettings | FPS, Resolution, DPI |
| Video Decoding | VideoSettings | Codec enable/disable, HW/SW toggle, decoder picker |
| Sidebar | VideoSettings | Enable toggle, position, width |

Same Flickable + ColumnLayout pattern as all existing settings pages. Uses the same controls with the same configPaths.

#### ConnectivitySettings.qml (NEW -- replaces Connection)

Takes the BT and WiFi AP sections from current `ConnectionSettings.qml`:

| Section | Controls |
|---------|----------|
| WiFi Access Point | Channel picker, Band selector |
| Bluetooth | Device name, Accept Pairings toggle, paired devices list |

The paired devices Repeater + BluetoothManager integration moves over verbatim.

#### SystemAboutSettings.qml (NEW -- merges System + About)

Combines content from both:

| Section | Source | Controls |
|---------|--------|----------|
| Identity | SystemSettings | HU name, manufacturer, model, SW version, car model/year, LHD toggle |
| Hardware | SystemSettings | Hardware profile, touch device |
| About | AboutSettings | Version display, Close App button |

### Settings Content Migration Map

| Current File | Current Section | Destination |
|---|---|---|
| `ConnectionSettings.qml` | Android Auto (auto-connect, TCP port) | `AndroidAutoSettings.qml` |
| `ConnectionSettings.qml` | Protocol Capture | `AndroidAutoSettings.qml` |
| `ConnectionSettings.qml` | WiFi Access Point | `ConnectivitySettings.qml` |
| `ConnectionSettings.qml` | Bluetooth + paired devices | `ConnectivitySettings.qml` |
| `VideoSettings.qml` | Playback (FPS, res, DPI) | `AndroidAutoSettings.qml` |
| `VideoSettings.qml` | Video Decoding (codecs, decoders) | `AndroidAutoSettings.qml` |
| `VideoSettings.qml` | Sidebar | `AndroidAutoSettings.qml` |
| `AudioSettings.qml` | (all) | Unchanged, accessed via `AudioSettingsCategory` |
| `EqSettings.qml` | (all) | Unchanged, accessed via `AudioSettingsCategory` + NavStrip |
| `DisplaySettings.qml` | (all) | Unchanged, visual refresh only |
| `CompanionSettings.qml` | (all) | Unchanged |
| `SystemSettings.qml` | (all) | `SystemAboutSettings.qml` |
| `AboutSettings.qml` | (all) | `SystemAboutSettings.qml` |

### Component Boundaries

| Component | Responsibility | Communicates With |
|-----------|---------------|-------------------|
| `SettingsMenu.qml` | StackView orchestration, back handling, title sync, direct-page access | ApplicationController, all settings pages |
| `SettingsCategoryList.qml` | Top-level category list (6 items + plugin settings) | SettingsMenu (via `categorySelected` signal) |
| `AudioSettingsCategory.qml` | Audio subcategory list (Output/Input + EQ) | SettingsMenu (via `subpageRequested` signal) |
| `AndroidAutoSettings.qml` | All AA-related config (connection, video, sidebar) | ConfigService, CodecCapabilityModel |
| `ConnectivitySettings.qml` | BT + WiFi AP config | ConfigService, BluetoothManager, PairedDevicesModel |
| `SystemAboutSettings.qml` | Identity, hardware, version, close button | ConfigService |
| NavStrip EQ button | Direct shortcut to EQ view | SettingsMenu (via `settingsView.openDirectPage`) |

### Data Flow for Settings Categories

No C++ changes needed for the settings reorganization. The category structure is purely a QML presentation concern. All settings controls continue to use `ConfigService.setValue(path, value)` with the same config paths. The reorganization is just moving QML controls between files.

```
User taps category -> SettingsMenu pushes page -> Page uses existing controls
  SettingsToggle { configPath: "..." }   -- unchanged API
  SettingsSlider { configPath: "..." }   -- unchanged API
  FullScreenPicker { configPath: "..." } -- unchanged API
```

### EQ Dual-Access Architecture

**Requirement:** EQ accessible from Audio settings AND from a NavStrip shortcut.

**Implementation:** NavStrip gets a new EQ icon button (between day/night toggle and settings gear). Tapping it navigates to settings and tells SettingsMenu to push directly to the EQ page, bypassing the category list and Audio subcategory.

```
NavStrip EQ button tap:
  1. PluginModel.setActivePlugin("")      // clear any active plugin
  2. ApplicationController.navigateTo(6)  // go to settings (makes SettingsMenu visible)
  3. settingsView.openDirectPage("eq")    // push EQ directly onto settings StackView
```

This works because `settingsView` is already a named ID in `Shell.qml` (the SettingsMenu instance). No new C++ plumbing needed -- it is pure QML-to-QML function call.

**Critical detail in `openDirectPage`:** Must reset the StackView to depth 1 before pushing, using `StackView.Immediate` to avoid animation artifacts. Otherwise, if the user was already in a settings sub-page, the EQ page stacks on top of stale state.

**Alternative considered:** Making EQ a separate built-in screen (screen type 7). Rejected because it duplicates the EQ component, adds a new ApplicationController screen type, and creates two code paths to the same view.

### Visual Language: Automotive-Minimal Pattern

The visual refresh applies a consistent language across all components. This is NOT a theme change (ThemeService colors and Q_PROPERTYs stay the same). It is a structural styling change to the QML controls.

**Principles applied to 1024x600 car touchscreen:**
- Full-width rows, no card backgrounds on individual settings items
- Subtle 1px dividers between rows (already partially present via `opacity: 0.15` pattern)
- Minimum 80px touch targets (already enforced by `UiMetrics.rowH`)
- No decorative elements (rounded cards around groups, shadows, gradients)
- Icon + label left-aligned, control right-aligned (already the pattern)
- Section headers: uppercase, small font, with bottom divider (already the pattern in `SectionHeader.qml`)
- Consistent padding: `UiMetrics.marginPage` for page edges, `UiMetrics.marginRow` for row content insets

**What changes in existing controls:**

| Control | Current State | Changes Needed |
|---------|--------------|----------------|
| `SettingsToggle` | Correct layout, default Qt Switch styling | Custom Switch background/handle via `background` and `indicator` properties |
| `SettingsSlider` | Correct layout, default Qt Slider styling | Custom track/handle styling for automotive look |
| `SegmentedButton` | Already automotive-appropriate | Possibly increase touch target height, verify contrast |
| `FullScreenPicker` | Bottom sheet pattern is good | Polish header/row styling consistency |
| `SettingsListItem` | Correct structure (icon + label + chevron + divider) | Verify icon sizing has no excess padding, match new row height |
| `SectionHeader` | Already uppercase + small + divider | Verify spacing consistency with new page margins |
| `ReadOnlyField` | Shows "Edit via web panel" hint | Clearer read-only visual distinction |
| `Tile` (launcher) | Card-style rounded rectangles with hover highlight | Flatter, simpler, reduced visual weight |
| `NavStrip` | Icon buttons in RowLayout | Add EQ button, adjust spacing for additional icon |

**UiMetrics additions needed:**
```qml
// Potential additions (evaluate during implementation)
readonly property int categoryRowH: Math.round(88 * scale)   // taller rows for top-level categories
readonly property int dividerOpacity: 0.15                     // codify the existing literal
```

Most existing UiMetrics values are already correct. The redesign is more about applying them consistently and refining control styling than adding new metrics.

## Patterns to Follow

### Pattern 1: Settings Page Template (Flickable + ColumnLayout)
**What:** Every settings page follows the same scrollable layout structure.
**When:** All settings pages (existing and new).
**Why:** Consistency, predictable behavior, handles content overflow on 600px screen.
```qml
Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        SectionHeader { text: "Section Name" }
        // controls...
    }
}
```
Every existing settings page already follows this. New pages must too.

### Pattern 2: ConfigPath Data Binding
**What:** All settings controls read/write ConfigService via the `configPath` string property.
**When:** Any control that persists a user preference.
**Why:** Uniform pattern -- no manual config wiring needed per control.

### Pattern 3: StackView Navigation with Title Sync
**What:** SettingsMenu manages a StackView. Each push updates the title via `ApplicationController.setTitle()`. Back pops and restores the parent title.
**When:** All settings navigation.
**Why:** Consistent back-button behavior, TopBar title always reflects current location.

### Pattern 4: Signal-Based Sub-Navigation
**What:** Category pages emit signals (`categorySelected`, `subpageRequested`) rather than directly pushing onto the StackView.
**When:** Category list and subcategory pages.
**Why:** Keeps navigation logic centralized in SettingsMenu. The category/subcategory components do not need a reference to the StackView -- they just declare what was selected.

### Pattern 5: Direct Page Access via Named QML ID
**What:** NavStrip calls `settingsView.openDirectPage("eq")` using the named ID from Shell.qml.
**When:** EQ shortcut from NavStrip.
**Why:** Zero C++ changes. Simple QML-to-QML function call through the existing object tree. The `settingsView` ID is already declared in Shell.qml.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Deep Category Nesting (3+ Levels)
**What:** Going beyond 2 levels of settings navigation (Category -> Detail).
**Why bad:** On a 600px-tall car screen, each navigation level costs a tap and hides context. 3+ levels means the user loses track of where they are. The back button becomes a maze.
**Instead:** Keep it to 2 levels max. Audio is the only category with subcategories (because EQ needs its own full screen). Everything else is flat.

### Anti-Pattern 2: C++ Model for Static Category List
**What:** Building a `QAbstractListModel` subclass in C++ for the settings category list.
**Why bad:** The category list is 6 static items. A C++ model adds a .hpp, .cpp, MOC, CMakeLists entry, and registration for zero benefit.
**Instead:** QML `ListModel` or hardcoded Repeater items. The current SettingsMenu already uses this approach.

### Anti-Pattern 3: Qt Quick Controls Material Theme for Custom Styling
**What:** Using Material theme properties (`Material.accent`, `Material.background`) to style switches/sliders.
**Why bad:** Material theme is all-or-nothing and fights the automotive-minimal aesthetic. It also behaves differently between Qt 6.4 (dev VM) and 6.8 (Pi), introducing platform-specific visual bugs.
**Instead:** Style controls via `background` and `indicator`/`handle` component overrides directly on the control instances, or wrap them in custom components.

### Anti-Pattern 4: Splitting EQ Into a Separate Top-Level View
**What:** Making EQ a standalone screen outside the settings StackView (e.g., ApplicationController screen type 7).
**Why bad:** Duplicates the EQ component (or requires sharing state), creates a parallel navigation path, complicates back-button behavior, requires C++ changes to ApplicationController.
**Instead:** EQ lives inside settings. The NavStrip shortcut navigates to settings + pushes EQ in one action via `openDirectPage()`.

### Anti-Pattern 5: Hardcoded Visual Constants
**What:** Using literal pixel sizes, margins, or colors in QML files.
**Why bad:** Already a documented gotcha in CLAUDE.md. Breaks when scale factor changes.
**Instead:** Always use `UiMetrics.*` for sizes and `ThemeService.*` for colors. Add new UiMetrics properties if a needed metric does not exist.

### Anti-Pattern 6: StackView State Leaks on Direct Navigation
**What:** Pushing EQ onto the StackView without first resetting to depth 1.
**Why bad:** If the user was already 2 levels deep in settings (e.g., Display page), the EQ page stacks on top. Back button goes to Display instead of back to category list. User gets lost.
**Instead:** `openDirectPage()` must pop to depth 1 with `StackView.Immediate` before pushing the target page.

## Suggested Build Order

Dependencies between the visual system and structural changes drive this ordering.

### Phase 1: Visual Foundation (No Structural Changes)

Restyle existing controls while settings structure is unchanged. This means the visual refresh can be tested against the current flat settings menu, and new pages inherit the polished controls from day one.

1. **UiMetrics additions** -- add any new sizing metrics needed (categoryRowH, etc.)
2. **Control visual refresh** -- restyle SettingsToggle, SettingsSlider, SegmentedButton, FullScreenPicker, SectionHeader, SettingsListItem, ReadOnlyField, InfoBanner
3. **Tile + LauncherMenu visual refresh** -- flatten tile styling, adjust grid spacing
4. **TopBar + NavStrip visual polish** -- spacing, touch feedback (but do NOT add EQ button yet)

**Rationale:** Visual-first avoids double work. If settings restructuring comes first, you apply visual changes to old files that get deleted anyway.

### Phase 2: Settings Restructuring

Build new pages and rewire navigation. Each page can be built and tested independently against the existing control library.

5. **SettingsCategoryList.qml** -- new top-level category list component
6. **SettingsMenu.qml rewrite** -- replace flat list with category navigation, add `openDirectPage()` function, wire signal handlers
7. **AndroidAutoSettings.qml** -- new merged page (Connection AA + Video content). Largest single file.
8. **ConnectivitySettings.qml** -- new page (BT + WiFi AP, plus paired devices list)
9. **SystemAboutSettings.qml** -- merge System + About content
10. **AudioSettingsCategory.qml** -- subcategory page with 2 items (Output/Input + EQ)
11. **Delete old files** -- remove ConnectionSettings.qml, VideoSettings.qml, SystemSettings.qml, AboutSettings.qml
12. **Verify all configPaths** -- ensure no config paths were dropped or mistyped during migration

**Rationale:** Category list first (step 5) so SettingsMenu rewrite (step 6) has something to show. AndroidAutoSettings (step 7) is the biggest merge and should come early to surface any issues. AudioSettingsCategory (step 10) is last because it depends on the SettingsMenu sub-navigation wiring.

### Phase 3: EQ Dual-Access + Final Polish

Wire the NavStrip shortcut and verify all navigation paths end-to-end.

13. **NavStrip EQ button** -- add EQ icon between day/night and settings
14. **Wire EQ shortcut** -- NavStrip calls `settingsView.openDirectPage("eq")`
15. **End-to-end navigation testing** -- verify: category->detail->back, NavStrip EQ from launcher, NavStrip EQ from AA (after disconnect), NavStrip EQ while already in settings, back from EQ (via NavStrip shortcut vs. Audio category), plugin settings still work

**Rationale:** EQ shortcut depends on both NavStrip modifications (Phase 1) and SettingsMenu rewrite (Phase 2). It is the final integration point.

## Integration Risk Assessment

| Change | Risk | Why | Mitigation |
|--------|------|-----|------------|
| SettingsMenu rewrite | MEDIUM | Back navigation + title sync + depth management could regress | Test all navigation paths systematically in Phase 3 step 15 |
| Content migration (Video/Connection split) | LOW | Moving QML between files, same configPaths, same controls | Verify every configPath after migration (Phase 2 step 12) |
| Visual control refresh | LOW | Same public API, only styling changes | Test on both Qt 6.4 (dev) and 6.8 (Pi) for compatibility |
| NavStrip EQ shortcut | LOW | Simple QML function call | Test: from launcher, from AA, from other settings page, rapid tapping |
| `openDirectPage` StackView state | MEDIUM | Stale StackView depth when accessed from NavStrip | Pop to depth 1 before pushing; test with various starting states |
| Deleted files still referenced | LOW | Old Component references in SettingsMenu could break | Compile-time QML errors catch missing files immediately |

## Sources

- Direct analysis of all QML files in the repository (HIGH confidence)
- `ApplicationController.hpp` for navigation API (HIGH confidence)
- `ThemeService.hpp` for available color properties (HIGH confidence)
- Qt 6 StackView, ListView, Flickable documentation (HIGH confidence -- stable Qt APIs)
- Automotive HMI best practices for touch target sizing (MEDIUM confidence -- general UX principles from training data)
