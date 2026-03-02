# Phase 2: Settings Restructuring - Context

**Gathered:** 2026-03-02
**Status:** Ready for planning
**Source:** REQUIREMENTS.md (SET-01 through SET-09, UX-03) + docs/settings-tree.md

<domain>
## Phase Boundary

Replace the current flat settings menu with a 6-category tile grid. Each tile drills into a category page containing reorganized settings. Purely presentational reorganization — no config key changes, no new settings fields, no web panel work.

</domain>

<decisions>
## Implementation Decisions

### Top-Level Layout
- 6 category tiles in a grid: Android Auto, Display, Audio, Connectivity, Companion, System/About
- Each tile shows a live status subtitle reflecting current configuration state
- Tapping a tile drills into its category page

### Category Contents (from requirements)
- **Android Auto (SET-03):** resolution, FPS, DPI, codec enable/disable, decoder selection, auto-connect, sidebar config, protocol capture
- **Display (SET-04):** brightness, theme picker, wallpaper picker, orientation, day/night mode source
- **Audio (SET-05):** master volume, output device, mic gain, input device, EQ subsection (stream selector, presets, band sliders)
- **Connectivity (SET-06):** WiFi AP channel/band, Bluetooth device name, pairable toggle, paired devices list with connect/forget actions
- **Companion (SET-07):** connection status, GPS coordinates, platform info
- **System/About (SET-08):** app version, build info, system diagnostics

### Config Preservation (SET-09)
- All existing config paths preserved — reorganization is purely presentational
- No config migration needed

### Read-Only Fields (UX-03)
- Read-only fields display clearly as informational — no confusion about interactivity

### Settings Tree Reference
- `docs/settings-tree.md` describes every page, section, control type, config key, and notes
- This is the authoritative reference for what controls exist and where they currently live
- Controls move between category pages but keep their config keys and behavior

### Claude's Discretion
- Tile grid layout details (spacing, sizing, icon selection)
- Status subtitle content and formatting per tile
- Internal section organization within each category page
- How to handle settings that don't exist yet (EQ, protocol capture) — stub or omit
- Plugin settings integration approach (settingsQml role already planned)
- StackView transition reuse from Phase 1

</decisions>

<specifics>
## Specific Ideas

- 1024x600 touchscreen in a car — tiles need to be large, glanceable touch targets
- Status subtitles should show actually useful info at a glance (current theme, volume level, connected device, resolution)
- The Phase 1 press feedback and transitions should carry over naturally

</specifics>

<deferred>
## Deferred Ideas

- EQ band sliders UI (Audio category will have a placeholder or section header — actual EQ is a future milestone)
- Protocol capture toggle (AA category — feature doesn't exist yet)
- Plugin settings pages (infrastructure only — no plugins provide settings yet)

</deferred>

---

*Phase: 02-settings-restructuring*
*Context gathered: 2026-03-02 from requirements*
