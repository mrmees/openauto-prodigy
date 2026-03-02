# Phase 2: Settings Restructuring - Research

**Researched:** 2026-03-02
**Domain:** QML UI restructuring, GridLayout, StackView navigation, live data binding
**Confidence:** HIGH

## Summary

Phase 2 replaces the current flat `ListView` settings menu with a 6-tile grid landing page. Each tile drills into a category page containing reorganized (but functionally unchanged) settings controls. This is purely a presentational reorganization — all config keys, control types, and C++ service bindings remain identical.

The existing codebase has everything needed: `Tile.qml` with press feedback, `StackView` with animated transitions (from Phase 1), `UiMetrics` with tile sizing properties (`tileW`, `tileH`, `tileIconSize`, `gridGap`), and all the settings sub-pages with their controls already working. The work is rearranging existing pieces — splitting current pages into new category groupings, adding a tile grid landing page, and wiring up live status subtitles from already-exposed Q_PROPERTYs.

The most complex aspect is the Connectivity and Android Auto category pages, which pull controls from the current `ConnectionSettings.qml` and `VideoSettings.qml` into new groupings. The Tile component needs a `subtitle` property for status text. No new C++ code is needed — all status data is already available via ConfigService, ThemeService, BluetoothManager, CompanionService, and AudioService.

**Primary recommendation:** Modify `SettingsMenu.qml` to replace the `ListView` initial item with a tile grid, create 6 new category page QML files that compose controls from the existing pages, and extend `Tile.qml` with a subtitle property. Reuse StackView transitions from Phase 1 as-is.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- 6 category tiles in a grid: Android Auto, Display, Audio, Connectivity, Companion, System/About
- Each tile shows a live status subtitle reflecting current configuration state
- Tapping a tile drills into its category page
- **Android Auto (SET-03):** resolution, FPS, DPI, codec enable/disable, decoder selection, auto-connect, sidebar config, protocol capture
- **Display (SET-04):** brightness, theme picker, wallpaper picker, orientation, day/night mode source
- **Audio (SET-05):** master volume, output device, mic gain, input device, EQ subsection (stream selector, presets, band sliders)
- **Connectivity (SET-06):** WiFi AP channel/band, Bluetooth device name, pairable toggle, paired devices list with connect/forget actions
- **Companion (SET-07):** connection status, GPS coordinates, platform info
- **System/About (SET-08):** app version, build info, system diagnostics
- All existing config paths preserved — reorganization is purely presentational, no config migration needed
- Read-only fields display clearly as informational — no confusion about interactivity
- `docs/settings-tree.md` is the authoritative control reference

### Claude's Discretion
- Tile grid layout details (spacing, sizing, icon selection)
- Status subtitle content and formatting per tile
- Internal section organization within each category page
- How to handle settings that don't exist yet (EQ, protocol capture) — stub or omit
- Plugin settings integration approach (settingsQml role already planned)
- StackView transition reuse from Phase 1

### Deferred Ideas (OUT OF SCOPE)
- EQ band sliders UI (Audio category will have a placeholder or section header — actual EQ is a future milestone)
- Protocol capture toggle (AA category — feature doesn't exist yet)
- Plugin settings pages (infrastructure only — no plugins provide settings yet)
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| SET-01 | Settings top-level displays 6 category tiles in a grid | Replace `ListView` initial item in `SettingsMenu.qml` with a `GridLayout` of 6 `Tile` components; use existing `UiMetrics.tileW/tileH/gridGap` |
| SET-02 | Category tiles show live status subtitles | Extend `Tile.qml` with `subtitle` property; bind to ConfigService/BluetoothManager/CompanionService/AudioService Q_PROPERTYs |
| SET-03 | Android Auto category contents | New `AASettings.qml` composing controls from current `VideoSettings.qml` (resolution, FPS, DPI, codecs, sidebar) + `ConnectionSettings.qml` (auto-connect, protocol capture) |
| SET-04 | Display category contents | Current `DisplaySettings.qml` as-is — already has correct grouping |
| SET-05 | Audio category contents | Current `AudioSettings.qml` + EQ stub section header (actual EQ deferred) |
| SET-06 | Connectivity category contents | New `ConnectivitySettings.qml` composing WiFi AP + Bluetooth controls from current `ConnectionSettings.qml` |
| SET-07 | Companion category contents | Current `CompanionSettings.qml` as-is — already has correct grouping |
| SET-08 | System/About category contents | Merge current `SystemSettings.qml` + `AboutSettings.qml` into one page |
| SET-09 | All config paths preserved | No config key changes — controls keep their `configPath` properties verbatim |
| UX-03 | Read-only fields visually distinct | Update `ReadOnlyField.qml` — remove "Edit via web panel" hint, use distinct styling (muted color, no chevron, optional info icon) |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt Quick | 6.4 / 6.8 | QML UI framework | Project target; dual-platform build |
| Qt Quick Controls | 6.4 / 6.8 | StackView, GridLayout | Already used in SettingsMenu |
| Qt Quick Layouts | 6.4 / 6.8 | GridLayout, RowLayout, ColumnLayout | Already used everywhere |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| ConfigService (C++) | In-tree | Read config values for subtitles | Tile status text |
| ThemeService (C++) | In-tree | Theme name for Display tile subtitle | Tile status text |
| BluetoothManager (C++) | In-tree | `connectedDeviceName` for Connectivity tile | Tile status text |
| CompanionService (C++) | In-tree | `connected` property for Companion tile | Tile status text |
| AudioService (C++) | In-tree | `masterVolume` for Audio tile | Tile status text |
| UiMetrics (QML) | In-tree | `tileW`, `tileH`, `tileIconSize`, `gridGap` | Tile grid layout |
| Tile (QML) | In-tree | Existing tile component with press feedback | Grid items |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| GridLayout | Flow/Grid | GridLayout is standard for fixed column counts; Flow is for wrapping — not needed with 6 tiles on 1024x600 |
| 6 new QML files | Inline Components in SettingsMenu | Separate files are cleaner, reusable, testable |
| Modifying Tile.qml | New CategoryTile.qml | Tile already has the right structure; just add subtitle property |

## Architecture Patterns

### Recommended Project Structure
```
qml/applications/settings/
├── SettingsMenu.qml          # MODIFY: replace ListView with tile grid
├── AASettings.qml             # NEW: Android Auto category page
├── DisplaySettings.qml        # MODIFY: minor (already correct grouping)
├── AudioSettings.qml          # MODIFY: add EQ stub section
├── ConnectivitySettings.qml   # MODIFY: becomes WiFi + BT only (AA controls moved out)
├── CompanionSettings.qml      # KEEP: already correct
├── SystemSettings.qml         # MODIFY: merge About content
├── AboutSettings.qml          # DELETE or KEEP as redirect
├── EqSettings.qml             # KEEP: unchanged (accessed from Audio category later)
├── VideoSettings.qml          # DELETE or KEEP — content moves to AASettings
qml/controls/
├── Tile.qml                   # MODIFY: add subtitle property
├── ReadOnlyField.qml          # MODIFY: improve visual distinction per UX-03
```

### Pattern 1: Tile Grid Landing Page
**What:** Replace the ListView with a centered GridLayout of 6 tiles
**When to use:** The settings top-level view
**Example:**
```qml
Component {
    id: settingsList

    Item {
        anchors.fill: parent

        GridLayout {
            anchors.centerIn: parent
            columns: 3
            rows: 2
            columnSpacing: UiMetrics.gridGap
            rowSpacing: UiMetrics.gridGap

            Tile {
                Layout.preferredWidth: UiMetrics.tileW
                Layout.preferredHeight: UiMetrics.tileH
                tileName: "Android Auto"
                tileIcon: "\ue531"  // directions_car
                tileSubtitle: ConfigService.value("video.resolution") + " " + ConfigService.value("video.fps") + "fps"
                onClicked: openPage("aa")
            }
            // ... 5 more tiles
        }
    }
}
```
**Note:** 3 columns x 2 rows fits well on 1024x600. With `tileW: 160*scale` and `gridGap: 24*scale`, total width ~= 528px (at scale=1.0), leaving comfortable margins. If tiles feel too small, increase `tileW`/`tileH` in UiMetrics — all sizing flows from there.

### Pattern 2: Tile with Live Status Subtitle
**What:** Extend Tile.qml with a `tileSubtitle` property that shows beneath the name
**When to use:** All 6 category tiles
**Example:**
```qml
// Addition to Tile.qml ColumnLayout:
Text {
    Layout.alignment: Qt.AlignHCenter
    text: tile.tileSubtitle
    color: ThemeService.descriptionFontColor
    font.pixelSize: UiMetrics.fontTiny
    horizontalAlignment: Text.AlignHCenter
    elide: Text.ElideRight
    Layout.maximumWidth: tile.width - UiMetrics.marginRow * 2
    visible: tile.tileSubtitle !== ""
}
```
**Note:** `fontTiny` (14*scale) for subtitles — small but readable in a car at glance distance. `elide: Text.ElideRight` handles long text gracefully.

### Pattern 3: Category Page Composition
**What:** Each category page is a Flickable with ColumnLayout containing SectionHeaders and existing controls
**When to use:** All 6 category pages
**Example:**
```qml
// AASettings.qml — composing controls from VideoSettings + ConnectionSettings
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

        SectionHeader { text: "Playback" }
        // FPS, Resolution, DPI controls (from VideoSettings)

        SectionHeader { text: "Video Decoding" }
        // Codec repeater (from VideoSettings)

        SectionHeader { text: "Sidebar" }
        // Sidebar controls (from VideoSettings)

        SectionHeader { text: "Connection" }
        // Auto-connect toggle, TCP port (from ConnectionSettings)
    }
}
```

### Pattern 4: Read-Only Field Visual Distinction (UX-03)
**What:** Make read-only fields unambiguously non-interactive
**When to use:** All `ReadOnlyField` instances
**Example:**
```qml
// ReadOnlyField.qml changes:
// 1. Remove "Edit via web panel" subtext entirely
// 2. Use descriptionFontColor for value text (muted vs normal)
// 3. Add optional info icon to indicate read-only nature
// 4. No chevron, no press feedback, no MouseArea

Text {
    text: root.value !== "" ? root.value : root.placeholder
    font.pixelSize: UiMetrics.fontBody
    color: ThemeService.descriptionFontColor  // ALWAYS muted — signals non-interactive
    elide: Text.ElideRight
    Layout.fillWidth: true
    horizontalAlignment: Text.AlignRight
}
// Remove the "Edit via web panel" Text entirely
```

### Anti-Patterns to Avoid
- **Creating new C++ models for tile status:** All status data is already exposed via existing services. Don't add StatusProvider or TileModel — just bind QML directly to ConfigService/ThemeService/BluetoothManager properties.
- **Duplicating control code between old and new pages:** Move controls, don't copy. If AASettings needs the codec repeater, extract it from VideoSettings — don't maintain two copies.
- **Hardcoding tile subtitle text:** Bind to service properties so subtitles update live. `ConfigService.value("video.resolution")` already returns the current value.
- **Deeply nested StackViews:** One StackView in SettingsMenu is enough. Category pages are leaf pages, not containers for more navigation. If a category needs sub-navigation (e.g., EQ from Audio), push another page onto the same StackView.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Tile grid layout | Manual x/y positioning | `GridLayout` with columns/rows | Handles spacing, alignment, resize |
| Status text updates | Manual timer polling | QML property bindings to C++ Q_PROPERTYs | Auto-updates on signal, zero overhead when unchanged |
| Page transitions | Custom animation code | StackView push/pop transitions (already done in Phase 1) | Already working, tested on Pi 4 |
| Tile press feedback | New touch handling | Existing `Tile.qml` scale+opacity behavior | Already implemented in Phase 1 |

**Key insight:** This phase is 90% QML reorganization and 10% Tile.qml enhancement. No new C++ code, no new architectural patterns. The complexity is in carefully decomposing the existing 7 settings pages into 6 new category groupings without losing any controls or breaking config bindings.

## Common Pitfalls

### Pitfall 1: Losing Controls During Reorganization
**What goes wrong:** A setting that existed in the old layout becomes unreachable in the new hierarchy.
**Why it happens:** Controls get lost when splitting pages (e.g., ConnectionSettings splits into AA + Connectivity).
**How to avoid:** Use `docs/settings-tree.md` as the checklist. After reorganization, verify every control from the tree document appears in exactly one category page.
**Warning signs:** A `configPath` binding exists in old code but not in any new category page.

### Pitfall 2: Tile Grid Too Small / Too Large for 1024x600
**What goes wrong:** Tiles are either cramped (can't read subtitles) or too large (grid doesn't fit).
**Why it happens:** UiMetrics tile sizing was defined in Phase 1 but not tested with actual content.
**How to avoid:** Current `tileW: 160*scale` and `tileH: 140*scale` with `gridGap: 24*scale` gives a 3x2 grid total ~528x304 (at scale=1.0). On 1024x600, this leaves ~496x~250 of margin — generous. May want to increase tile size to ~280x200 for better touch targets and readability. Test and adjust `UiMetrics` values.
**Warning signs:** Tiles feel tiny compared to the launcher tiles.

### Pitfall 3: StackView Depth Off-By-One in Back Navigation
**What goes wrong:** Pressing back from a category page doesn't return to the tile grid, or pressing back from the tile grid tries to pop an empty stack.
**Why it happens:** Current `onBackRequested` checks `settingsStack.depth > 1`. This logic remains correct for tile grid → category page, but the title-setting logic needs updating.
**How to avoid:** Keep the same `depth > 1` check. Update the title map in `openPage()` to include the new category page IDs.
**Warning signs:** Title shows wrong text after navigating back.

### Pitfall 4: Subtitle Bindings Failing When Services Unavailable
**What goes wrong:** QML errors when `BluetoothManager`, `CompanionService`, or `EqualizerService` doesn't exist (dev VM, disabled in config).
**Why it happens:** Some C++ services are conditionally created. `typeof X !== "undefined"` guards are used throughout.
**How to avoid:** Every tile subtitle binding must use the same `typeof` guard pattern already used in the settings pages. Example: `tileSubtitle: typeof BluetoothManager !== "undefined" && BluetoothManager.connectedDeviceName !== "" ? "BT: " + BluetoothManager.connectedDeviceName : "BT: Not connected"`
**Warning signs:** Console errors about undefined properties on dev VM.

### Pitfall 5: Moving Codec Repeater Loses Config Persistence Logic
**What goes wrong:** Codec enable/disable and decoder selection stops saving after moving to AASettings.
**Why it happens:** `VideoSettings.qml` has `saveCodecConfig()` and `saveDecoderConfig()` helper functions plus `Component.onCompleted` config loading. These must move with the codec repeater.
**How to avoid:** Move the complete codec section including all helper functions and the `Component.onCompleted` loader into AASettings.qml.
**Warning signs:** Codec settings reset on app restart.

### Pitfall 6: ReadOnlyField "Edit via web panel" Removal Causes Confusion
**What goes wrong:** Users don't understand why some fields can't be edited.
**Why it happens:** Removing the hint without replacing it with a clear visual distinction.
**How to avoid:** Use `ThemeService.descriptionFontColor` (muted) for the value text, never `normalFontColor`. The muted color universally signals "informational, not interactive." Optionally add a small lock or info icon.
**Warning signs:** Users try to tap read-only fields expecting them to be editable.

## Code Examples

### Tile.qml Subtitle Addition (SET-02)
```qml
// Add property:
property string tileSubtitle: ""

// Add to ColumnLayout, after the tileName Text:
Text {
    Layout.alignment: Qt.AlignHCenter
    text: tile.tileSubtitle
    color: ThemeService.descriptionFontColor
    font.pixelSize: UiMetrics.fontTiny
    horizontalAlignment: Text.AlignHCenter
    elide: Text.ElideRight
    Layout.maximumWidth: tile.width - UiMetrics.marginRow * 2
    visible: tile.tileSubtitle !== ""
}
```

### Tile Grid in SettingsMenu (SET-01)
```qml
Component {
    id: settingsList

    Item {
        GridLayout {
            anchors.centerIn: parent
            columns: 3
            columnSpacing: UiMetrics.gridGap
            rowSpacing: UiMetrics.gridGap

            Tile {
                Layout.preferredWidth: UiMetrics.tileW
                Layout.preferredHeight: UiMetrics.tileH
                tileName: "Android Auto"
                tileIcon: "\ue531"
                tileSubtitle: {
                    var res = ConfigService.value("video.resolution") || "720p"
                    var fps = ConfigService.value("video.fps") || 60
                    return res + " " + fps + "fps"
                }
                onClicked: openPage("aa")
            }

            Tile {
                Layout.preferredWidth: UiMetrics.tileW
                Layout.preferredHeight: UiMetrics.tileH
                tileName: "Display"
                tileIcon: "\ueb97"
                tileSubtitle: {
                    var theme = ConfigService.value("display.theme") || "default"
                    return theme.charAt(0).toUpperCase() + theme.slice(1)
                }
                onClicked: openPage("display")
            }

            Tile {
                Layout.preferredWidth: UiMetrics.tileW
                Layout.preferredHeight: UiMetrics.tileH
                tileName: "Audio"
                tileIcon: "\ue050"
                tileSubtitle: {
                    var vol = typeof AudioService !== "undefined" ? AudioService.masterVolume : 100
                    return "Vol: " + vol + "%"
                }
                onClicked: openPage("audio")
            }

            Tile {
                Layout.preferredWidth: UiMetrics.tileW
                Layout.preferredHeight: UiMetrics.tileH
                tileName: "Connectivity"
                tileIcon: "\ue1d8"
                tileSubtitle: {
                    if (typeof BluetoothManager === "undefined") return ""
                    var name = BluetoothManager.connectedDeviceName
                    return name !== "" ? "BT: " + name : "BT: No device"
                }
                onClicked: openPage("connectivity")
            }

            Tile {
                Layout.preferredWidth: UiMetrics.tileW
                Layout.preferredHeight: UiMetrics.tileH
                tileName: "Companion"
                tileIcon: "\ue324"
                tileSubtitle: {
                    if (typeof CompanionService === "undefined") return "Disabled"
                    return CompanionService.connected ? "Connected" : "Not connected"
                }
                onClicked: openPage("companion")
            }

            Tile {
                Layout.preferredWidth: UiMetrics.tileW
                Layout.preferredHeight: UiMetrics.tileH
                tileName: "System"
                tileIcon: "\uf8cd"
                tileSubtitle: "v" + (ConfigService.value("identity.sw_version") || "0.0.0")
                onClicked: openPage("system")
            }
        }
    }
}
```

### Status Subtitle Data Sources
```
| Tile          | Subtitle Content           | Data Source                                         |
|---------------|----------------------------|-----------------------------------------------------|
| Android Auto  | "720p 60fps"               | ConfigService.value("video.resolution") + video.fps |
| Display       | "Ember" (theme name)       | ConfigService.value("display.theme")                |
| Audio         | "Vol: 75%"                 | AudioService.masterVolume                           |
| Connectivity  | "BT: Galaxy S25"           | BluetoothManager.connectedDeviceName                |
| Companion     | "Connected" / "Disabled"   | CompanionService.connected                          |
| System        | "v0.4.3"                   | ConfigService.value("identity.sw_version")          |
```

### Control Migration Map
```
Old Page               → New Category Page     Controls Moved
─────────────────────────────────────────────────────────────
VideoSettings.qml      → AASettings.qml        FPS, Resolution, DPI, Codec repeater,
                                                 Sidebar toggle/position/width,
                                                 codecDisplayName(), saveCodecConfig(),
                                                 saveDecoderConfig(), decoderPickerDialog
ConnectionSettings.qml → AASettings.qml        Auto-connect toggle, TCP Port,
                                                 Protocol Capture section
ConnectionSettings.qml → ConnectivitySettings   WiFi AP section, Bluetooth section
                                                 (BT name, pairable, paired devices)
DisplaySettings.qml    → DisplaySettings.qml    (unchanged — already correct grouping)
AudioSettings.qml      → AudioSettings.qml      (keep + add EQ stub section header)
CompanionSettings.qml  → CompanionSettings.qml  (unchanged — already correct grouping)
SystemSettings.qml     → SystemSettings.qml     (merge About content: version + close btn)
AboutSettings.qml      → (removed/merged)        Content absorbed into SystemSettings
```

### ReadOnlyField.qml UX-03 Update
```qml
// Remove showWebHint property and "Edit via web panel" text entirely
// Change value text color to always use descriptionFontColor
// Result: clean label + muted value, clearly non-interactive

ColumnLayout {
    Layout.fillWidth: true
    spacing: 0  // was 2 with subtext, now just value

    Text {
        text: root.value !== "" ? root.value : root.placeholder
        font.pixelSize: UiMetrics.fontBody
        color: ThemeService.descriptionFontColor  // always muted
        elide: Text.ElideRight
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignRight
    }
    // "Edit via web panel" text REMOVED
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Flat ListView menu | Tile grid categories | This phase | Better discoverability, glanceable status |
| 7 separate pages + About | 6 category pages | This phase | Mental model alignment (AA vs Connection vs Video distinction was confusing) |
| "Edit via web panel" hint | Muted color visual distinction | This phase | Cleaner, less confusing UX |

## Open Questions

1. **Tile Sizing vs Touch Target**
   - What we know: `UiMetrics.tileW=160*scale`, `tileH=140*scale` gives ~160x140px at scale 1.0. Launcher tiles use `width * 0.18` (~184px on 1024).
   - What's unclear: Whether 160x140 is large enough as a car touch target with subtitle text.
   - Recommendation: Start with current UiMetrics values. Matt tests on Pi — if too small, bump to `tileW: 260`, `tileH: 200` which gives a 3x2 grid of ~828x424 (comfortable on 1024x600). This is a UiMetrics-only change, no layout code changes needed.

2. **EQ Stub in Audio Category**
   - What we know: EQ band sliders are deferred. Audio category should acknowledge EQ exists but not implement it.
   - What's unclear: Stub section header only, or a "Coming soon" text, or omit entirely?
   - Recommendation: Include a `SectionHeader { text: "Equalizer" }` followed by a single muted text: "Configure via EQ shortcut". The EqSettings.qml page already exists and works — it's just the in-Audio-category access that's deferred. This gives users a hint without creating dead UI.

3. **Protocol Capture in AA Category**
   - What we know: Protocol capture controls exist in `ConnectionSettings.qml` and work. They were added as a debug feature.
   - What's unclear: Whether to move them to AA category or leave them accessible elsewhere.
   - Recommendation: Move them to AASettings.qml under a "Debug" section. They're AA-specific debug tools, so they belong there. The controls already work and have config paths.

4. **Plugin Settings Integration**
   - What we know: SettingsMenu.qml already scans PluginModel for settingsQml and pushes those pages. No plugins currently provide settings.
   - What's unclear: Where plugin settings appear in the new tile grid.
   - Recommendation: Keep the existing dynamic plugin settings mechanism but trigger it differently — either as a "Plugins" sub-section within System, or as additional tiles. Since no plugins provide settings yet, just preserve the code path without prominent UI. A "Plugins" section in System/About with "No plugin settings" placeholder is sufficient.

## Sources

### Primary (HIGH confidence)
- In-tree codebase: All 9 existing settings QML files inspected directly
- In-tree codebase: `Tile.qml`, `ReadOnlyField.qml`, `SettingsListItem.qml`, `UiMetrics.qml` inspected
- In-tree codebase: C++ service headers — ConfigService, ThemeService, AudioService, BluetoothManager, CompanionListenerService
- In-tree docs: `docs/settings-tree.md` — complete control inventory with types, config keys, and notes
- Phase 1 research: Tile sizing, press feedback, StackView transitions all verified and implemented

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - everything is in-tree, no new dependencies
- Architecture: HIGH - rearranging existing QML with proven patterns (GridLayout, StackView, Tile)
- Pitfalls: HIGH - derived from direct code inspection (service guards, codec config persistence, back navigation)

**Research date:** 2026-03-02
**Valid until:** 2026-04-02 (stable — no external dependencies, pure QML reorganization)
