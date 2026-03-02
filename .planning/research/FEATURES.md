# Feature Landscape

**Domain:** Automotive head unit UI/UX -- settings reorganization and visual refresh (v0.4.3)
**Researched:** 2026-03-02
**Overall confidence:** HIGH (grounded in Google Design for Driving guidelines, OEM patterns, existing codebase analysis)

---

## Table Stakes

Features users expect from any car touchscreen interface. Missing = product feels broken or amateur.

| Feature | Why Expected | Complexity | Existing Control | Notes |
|---------|--------------|------------|-----------------|-------|
| Large touch targets (76dp+ / 80px+) | Google Design for Driving mandates 76x76dp minimum. Aftermarket units use 80-100px for frequent actions. Drivers can't aim precisely. | Low | `UiMetrics.touchMin` = 56*scale (~56px). `UiMetrics.rowH` = 80*scale (~80px). | Row height is fine; `touchMin` (used in SegmentedButton, EQ bypass) needs bump to ~76px. Sliders need fat thumb handles. |
| Visual press feedback on every tappable element | Without haptics, visual feedback is the only confirmation a tap registered. Google specifies 330ms ripple. Research shows 40% less glance time with proper feedback. | Med | **Missing.** No press states on SettingsListItem, Tile, NavStrip buttons, or FullScreenPicker rows. Only Tile has hover (mouse-only). | Single biggest UX gap. Every tappable thing needs a pressed color state or opacity shift. |
| Dark-first color scheme | Cars operate in low light. 82% of users prefer dark mode. Every OEM and AA/CarPlay defaults to dark. | Low | **Done.** ThemeService provides day/night. Default is dark. | Already table stakes. Ensure new components inherit theme colors. |
| Full-width settings rows with icon + label + value/chevron | Universal pattern: Android settings, iOS settings, Tesla, all aftermarket units. Users know how to navigate this. | Low | **Done.** `SettingsListItem` does icon + label + chevron. Settings pages use full-width Flickable + ColumnLayout. | Existing pattern is solid. Enhance with press state and consistent icon usage. |
| Grouped settings with section headers | Every settings UI groups related items. Flat lists don't scale past 5-6 items. | Low | **Done.** `SectionHeader` component exists. SettingsMenu uses section/item model. | Current 2-group structure (General + Companion) is too flat. Reorganize into 6 categories. |
| Slider with visible current value | Drivers need to see what they're changing without counting pixels on the track. | Low | **Done.** `SettingsSlider` shows numeric value right-aligned. | Works. Consider adding unit suffix (%, dB, px). |
| Toggle with clear on/off state | Binary settings need unambiguous state. Default Qt Switch is fine. | Low | **Done.** `SettingsToggle` with Qt `Switch`. | Consider slight size increase for car use. |
| Modal picker dismissable by tapping outside | Standard mobile/automotive pattern. Users expect tap-outside-to-close. | Low | **Partial.** `FullScreenPicker` is modal+dim but no explicit tap-outside handler. Qt Dialog modal=true should handle this, but behavior varies. | Verify tap-outside works on Pi. Add explicit close-on-dim-tap if not. |
| Back navigation (breadcrumb or back button) | Users must be able to get back from any settings sub-page. | Low | **Done.** StackView in SettingsMenu with back handling. NavStrip has back button. TopBar shows "Settings > SubPage". | Works. Consider making breadcrumb tappable (go directly to Settings root). |
| Readable typography at arm's length | 60cm+ viewing distance in a car. Research says 34-38px minimum for good readability. | Low | `UiMetrics.fontBody` = 20*scale (~20px). `fontTitle` = 22*scale. | Font sizes are on the small side for automotive. Consider bumping fontBody to 22-24px base, fontTitle to 26-28px. |
| Consistent iconography | Every settings category and every row-level action should have an icon. Icons aid recognition speed (critical while driving). | Low | MaterialIcon component exists. SettingsListItem has icon prop. Some settings rows (toggles, sliders) don't show icons. | Add leading icon support to SettingsToggle, SettingsSlider, FullScreenPicker, SegmentedButton. |

## Differentiators

Features that set the product apart from generic settings UIs. Not expected, but valued. Focus on automotive-specific quality.

| Feature | Value Proposition | Complexity | Existing Control | Notes |
|---------|-------------------|------------|-----------------|-------|
| Category tiles as settings entry point | Instead of a flat scrollable list of 15+ items, show 6 large tappable tiles (like a launcher). One tap to category, then a short list within. Tesla, Pioneer, and Android Automotive all use this pattern for top-level settings. | Med | LauncherMenu has 4-column tile grid using `Tile` component. `SettingsMenu` uses ListView. | Replace SettingsMenu's ListView initial page with a 2x3 or 3x2 grid of category tiles. Reuse `Tile` component with icons. Push category ListView on tile tap. |
| Inline status indicators on settings rows | Show current value/state without entering the setting. "WiFi: 5GHz Ch36", "BT: 2 devices", "Volume: 75%". Aftermarket units show this. | Med | FullScreenPicker shows `_displayText` inline. SettingsSlider shows value. But SettingsListItem (category rows) shows no summary. | Add optional `subtitle` or `value` property to SettingsListItem for category-level summaries. |
| Animated transitions between settings levels | Smooth push/pop feels polished vs. instant swap. Every modern car UI has slide transitions. | Low | StackView already provides transitions. | May already work. Verify StackView push animation is enabled (Qt default is slide). |
| EQ accessible from nav strip | Quick access to EQ without navigating Settings > Audio > EQ. Common on aftermarket units (dedicated EQ button). | Low | NavStrip has home, plugins, day/night, settings. EqSettings exists as a component. | Add EQ icon to NavStrip utility section. On tap, push EqSettings directly. |
| Swipe-to-delete on BT devices | Natural mobile pattern for destructive list actions. Already exists in EQ presets. | Low | EQ preset rows have swipe-to-delete. BT device "Forget" is a tiny text tap target. | Port the swipe-to-delete pattern from EqSettings to ConnectionSettings paired device rows. |
| BT device connection status with tap-to-connect | Show connected/disconnected with color. Tap disconnected device to attempt reconnect. Long-press or swipe to forget. | Med | ConnectionSettings shows icon color for connected state. "Forget" text link. No tap-to-connect. | Add `onClicked` to device row for reconnect. Make "Forget" a swipe action instead of tiny text. |
| Read-only fields with visual distinction | Config values that can't be changed in the UI (TCP port, capture path) should look different from editable fields. Users shouldn't tap them expecting interaction. | Low | `ReadOnlyField` component exists but styling may not clearly distinguish from interactive rows. | Add muted text color and remove chevron. Maybe add a lock icon or "info" style. |
| Restart-required badge | Some settings need a restart. Show this clearly per-row, not as a global banner at the bottom. | Low | `restartRequired` prop exists on SettingsSlider, SettingsToggle, etc. Shows a small restart icon. | Already implemented. Consider making the icon more prominent or adding a tinted background. |

## Anti-Features

Features to explicitly NOT build for v0.4.3.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Animated/themed icons (SVG icon packs) | Scope creep. Material Icons are universal, already loaded, resolution-independent. Custom icon packs are a v2 feature. | Use Material Icons codepoints consistently. Document which icons are used where. |
| Search within settings | Only 20-30 settings total. Search is overhead for a small settings tree. Tesla has hundreds; we don't. | Good category organization eliminates the need. 6 categories with 3-6 items each. |
| Settings drag-to-reorder | No user has asked for this. Settings order should be opinionated and consistent. | Fixed order per category. |
| Inline editing of read-only fields | TCP port, capture path, etc. are set via config file or web panel. Adding inline text editing in a car UI is a distraction hazard and engineering effort for edge cases. | Keep ReadOnlyField. Direct users to web config panel for advanced changes. |
| Per-setting help text / tooltips | Adds visual clutter. Automotive UIs should be self-explanatory through clear labels and sane defaults. | Use descriptive labels. "Auto-connect to phone" not "auto_connect_aa". |
| Nested settings deeper than 2 levels | Settings > Category > [items]. Never Settings > Category > Subcategory > Items. 3+ levels is disorienting on a small screen while driving. | Flatten. If a category has subsections, use SectionHeader within a single scrollable page. |
| Confirmation dialogs for every action | Modal fatigue. Only confirm destructive actions (Forget device, Factory reset). Toggles and sliders apply immediately. | Immediate apply for non-destructive. Confirm for destructive only. |
| Horizontal scrolling in settings | Never. Vertical scroll only. Horizontal is disorienting and undiscoverable on touch. | Full-width rows, always. |
| Ripple/wave animation feedback | Google's 330ms ripple is elegant but requires custom shader or Canvas work in QML. Overkill for this milestone. | Simple opacity or background color change on pressed state. Fast, obvious, zero GPU cost. |

## Feature Dependencies

```
Press feedback on controls -----> All tappable components (prerequisite for everything else)
                                   |
Category tile grid ----------------> Tile component (exists), SettingsMenu refactor
                                   |
Settings reorganization -----------> New category pages (6), SettingsMenu model update
   |
   +-- EQ in Audio settings ------> EqSettings component (exists), new parent page
   |
   +-- BT devices in Connectivity -> ConnectionSettings BT section (exists), enhance with swipe
   |
   +-- Video in Android Auto -----> VideoSettings content moves to new AA settings page
                                   |
Icon support in all controls ------> SettingsToggle, SettingsSlider, SegmentedButton, FullScreenPicker
                                   |
NavStrip EQ shortcut --------------> NavStrip modification, EqSettings as standalone pushable view
```

## MVP Recommendation

### Must-have for v0.4.3 (defines the milestone):

1. **Press feedback on all tappable elements** -- Table stakes. Without this, the UI feels dead. Add a pressed color state (highlight at 20% opacity or similar) to SettingsListItem, Tile, NavStrip buttons, FullScreenPicker rows, and EQ controls. This is the single change that will make the biggest UX difference for the least effort.

2. **Settings reorganization into 6 categories with tile grid entry** -- The headline feature of this milestone. Replace the flat ListView with a 2x3 tile grid. Categories: Android Auto, Display, Audio, Connectivity, Companion, System/About. Each tile taps into a single-level scrollable settings page.

3. **Move Video settings into Android Auto category** -- Video resolution, FPS, DPI, codec selection, and sidebar settings are all AA-specific. They don't belong in a generic "Video" category. This is a rearrangement, not new features.

4. **Move EQ into Audio category** -- EQ is an audio feature. Currently it's a separate nav entry. Embed it as a section within Audio settings (or a sub-page of Audio). Keep the nav strip shortcut as a bonus.

5. **Consistent icons on all settings rows** -- Add leading icon to SettingsToggle, SettingsSlider, SegmentedButton, FullScreenPicker. Use existing MaterialIcon component.

### Should-have (improves quality, do if time allows):

6. **BT device swipe-to-forget** -- Port swipe pattern from EQ presets. Replace tiny "Forget" text.

7. **Inline status on category tiles** -- "5GHz Ch36", "2 paired", etc. on the tile grid.

8. **Font size bump for automotive readability** -- Increase fontBody base from 20 to 22-24px. Test on Pi at arm's length.

9. **NavStrip EQ shortcut button** -- Quick access icon in the utility section of NavStrip.

### Defer to future milestone:

- Touch target size audit (touchMin bump from 56 to 76) -- needs testing across all views
- Animated transitions tuning
- Read-only field visual overhaul
- Settings value preview on category rows

## Proposed Settings Reorganization

### Current (8 flat sections):
```
General:     Display | Audio | Connection | Video | System
Companion:   Companion
Plugins:     [dynamic]
             About
```

### Proposed (6 category tiles):
```
Android Auto        Display             Audio
[smart_display]     [brightness_7]      [volume_up]
- Resolution        - Brightness        - Master Volume
- FPS               - Theme             - Output Device
- DPI               - Wallpaper         - Input Device
- Codecs/Decoder    - Orientation       - Mic Gain
- Sidebar           - Day/Night source  - EQ (sub-page or
- Auto-connect      - Night start/end     inline section)
- Protocol capture  - GPIO night pin

Connectivity        Companion           System
[settings_ethernet] [phone_android]     [settings]
- WiFi Channel      - [companion        - [system settings]
- WiFi Band           settings]         - About
- BT Name                               - Logs
- Accept Pairings                       - Factory Reset
- Paired Devices
- TCP Port
```

### Rationale:
- **Android Auto** groups everything the user cares about for their primary use case (AA projection). Video settings make no sense without AA context. Auto-connect and protocol capture are also AA-specific.
- **Display** is about the head unit's own appearance (brightness, theme, wallpaper), not AA video.
- **Audio** combines output/input hardware with EQ. These are inherently related.
- **Connectivity** is WiFi AP + Bluetooth -- both are wireless communication config.
- **Companion** stays isolated because it's a separate app's config.
- **System/About** is the junk drawer, but a small one. About moves here from its own top-level entry.

## Existing Controls Inventory

Components available for reuse (no new development needed):

| Control | File | Capabilities |
|---------|------|-------------|
| `SettingsListItem` | `qml/controls/SettingsListItem.qml` | Icon + label + chevron, click signal, 1px divider |
| `SettingsToggle` | `qml/controls/SettingsToggle.qml` | Label + Switch, configPath binding, restart badge |
| `SettingsSlider` | `qml/controls/SettingsSlider.qml` | Label + Slider + value readout, configPath, debounced save, moved signal |
| `SegmentedButton` | `qml/controls/SegmentedButton.qml` | Label + N-segment toggle, configPath, inner corner masking |
| `FullScreenPicker` | `qml/controls/FullScreenPicker.qml` | Label + value + bottom-sheet modal, configPath or model-driven, check icon on selected |
| `SectionHeader` | `qml/controls/SectionHeader.qml` | Section divider text |
| `ReadOnlyField` | `qml/controls/ReadOnlyField.qml` | Label + non-editable value display |
| `InfoBanner` | `qml/controls/InfoBanner.qml` | Warning/info message strip |
| `Tile` | `qml/controls/Tile.qml` | Icon + label card, hover highlight (mouse), click signal, radius 8 |
| `MaterialIcon` | `qml/controls/MaterialIcon.qml` | Material Icons font rendering at specified size/color |
| `EqBandSlider` | `qml/controls/EqBandSlider.qml` | Vertical gain slider with frequency label, reset on long-press |
| `UiMetrics` | `qml/controls/UiMetrics.qml` | Singleton: rowH=80, touchMin=56, fontBody=20, fontTitle=22 (all *scale) |

### Components Needing Modification

| Control | Change | Effort |
|---------|--------|--------|
| `SettingsListItem` | Add `pressed` color state on MouseArea | Trivial |
| `Tile` | Add `pressed` state (not just hover). Add optional `subtitle` string prop. | Low |
| `SettingsToggle` | Add optional `icon` string property + MaterialIcon before label | Low |
| `SettingsSlider` | Add optional `icon` string property + MaterialIcon before label | Low |
| `SegmentedButton` | Add optional `icon` string property + MaterialIcon before label | Low |
| `FullScreenPicker` | Add optional `icon` property. Verify tap-outside-to-close on Pi. | Low |
| `NavStrip` | Add EQ shortcut button in utility section (after day/night, before settings) | Low |
| `SettingsMenu` | Replace initial ListView with tile grid. Update model to 6 categories. Refactor page components into new category groupings. | Med |
| `ConnectionSettings` | Extract BT section into Connectivity page. Add swipe-to-forget on device rows. | Med |
| `VideoSettings` | Move content into new AndroidAutoSettings page. | Low |

### New Components Needed

| Component | Purpose | Based On |
|-----------|---------|----------|
| `AndroidAutoSettings.qml` | AA-specific settings page (video + connection AA items) | Combine VideoSettings + relevant ConnectionSettings items |
| `ConnectivitySettings.qml` | WiFi AP + BT pairing settings | Extract from ConnectionSettings |

## Sources

- [Google Design for Driving -- Sizing](https://developers.google.com/cars/design/automotive-os/design-system/sizing) -- 76dp minimum touch targets, icon sizes 44/36/24dp (HIGH confidence)
- [Google Design for Driving -- Components](https://developers.google.com/cars/design/automotive-os/components/overview) -- Component catalog: list items, buttons, dialogs, tabs, control bar (HIGH confidence)
- [Google Design for Driving -- Buttons](https://developers.google.com/cars/design/automotive-os/components/buttons) -- 330ms ripple, 156dp min width, filled vs unfilled states (HIGH confidence)
- [Snapp Automotive -- Touch Area Sizes](https://www.snappautomotive.io/blog/how-big-should-touch-areas-in-car-interfaces-be) -- 80px min, 100px for frequent actions (MEDIUM confidence)
- [Effect of Touch Button Interface on IVIS Usability](https://www.tandfonline.com/doi/full/10.1080/10447318.2021.1886484) -- Visual feedback reduces glance duration 40% (MEDIUM confidence)
- [Android Developer -- Organize Your Settings](https://developer.android.com/develop/ui/views/components/settings/organize-your-settings) -- Grouping/hierarchy patterns (HIGH confidence)
- [MDPI -- UI Design Patterns for Infotainment](https://www.mdpi.com/2071-1050/14/13/8186) -- 9 design patterns for IVI systems (MEDIUM confidence)
- [Automotive UX Basics -- UXPin](https://www.uxpin.com/studio/blog/automotive-ux/) -- Clean/minimal principle, large text, dark-first (MEDIUM confidence)
- [Tesla Owner's Manual -- Touchscreen](https://www.tesla.com/ownersmanual/model3/en_us/GUID-02F089F4-6818-4EFB-9AF1-0DA2187C5FF0.html) -- Tesla settings categories reference (MEDIUM confidence)
- [Android AOSP -- Car Settings](https://source.android.com/docs/automotive/hmi/car_settings/add_car_settings) -- Android Automotive settings patterns (HIGH confidence)
- Existing codebase analysis of all 43 QML files, all settings pages, all control components (HIGH confidence)

---
*Feature research for: UI Polish & Settings Reorganization (v0.4.3 milestone)*
*Researched: 2026-03-02*
