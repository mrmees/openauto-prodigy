# Phase 2: Theme & Display - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Users can personalize the head unit appearance (color palettes, wallpapers) and control screen brightness. This phase delivers theme selection with at least 3 palettes, wallpaper selection from available options, persistence across reboots, and brightness control from both settings and the gesture overlay that writes to actual display hardware on the Pi.

</domain>

<decisions>
## Implementation Decisions

### Color Palettes
- Ship 3-4 bundled palettes that look good on a 1024x600 car display — Claude's discretion on specific colors and names
- Day/night variant handling per palette is Claude's discretion (some palettes like AMOLED may not need separate day/night)
- Support user-added themes via `~/.openauto/themes/` — scan directory, show alongside bundled themes
- Transition style when switching palettes is Claude's discretion (instant snap vs smooth fade)

### Wallpaper System
- Must satisfy success criterion: "User can select a wallpaper from available options"
- Wallpaper approach (images, gradients, presets, or mix) is Claude's discretion — pick what satisfies the criterion with minimum complexity
- Theme-wallpaper coupling (independent vs theme-bundled defaults) is Claude's discretion
- User-added wallpapers from `~/.openauto/wallpapers/` — Claude's discretion on whether to include in this phase or defer

### Brightness Control
- Display may not have sysfs backlight (DFRobot HDMI display) — plan for both hardware backlight (`/sys/class/backlight/`) and software dimming fallback (dark overlay or gamma)
- Minimum brightness floor — Claude's discretion, probably 5-10% to prevent black screen panic
- Auto-adjust with day/night mode — Claude's discretion on whether brightness follows day/night or stays manual-only
- Brightness persists across reboots via `display.brightness` in config.yaml (config path already exists in DisplaySettings)
- Wire the existing GestureOverlay brightness slider stub to the actual brightness control

### Settings UI
- Theme picker and wallpaper picker UI design is Claude's discretion — leverage existing controls (FullScreenPicker, SettingsListItem) where possible
- Live-apply vs confirm button is Claude's discretion — leaning toward instant apply for quick in-car interaction
- Web config panel support for theme/wallpaper/brightness is out of scope for this phase (separate concern)

### Claude's Discretion
- Specific palette colors and names (must be at least 3, must look good on car display)
- Day/night variant strategy per palette
- Wallpaper content approach (images, gradients, or mix)
- Theme-to-wallpaper coupling model
- Brightness minimum floor value
- Brightness auto-adjust behavior with day/night
- Theme/wallpaper picker UI layouts
- Color transition animation approach
- Whether user-added wallpapers ship in this phase or are deferred

</decisions>

<specifics>
## Specific Ideas

- The existing ThemeService YAML format already supports arbitrary color palettes — extend, don't replace
- `IDisplayService` interface is already defined with `setBrightness(int)` but has no implementation — this phase implements it
- GestureOverlay brightness slider has a `TODO: Wire to IDisplayService.setBrightness()` — close this loop
- DisplaySettings already has a `display.brightness` config path and slider — just needs backend wiring
- Success criterion explicitly requires "at least 3 color palettes" and "wallpaper from available options" — both must be selectable in settings

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `ThemeService` (`src/core/services/ThemeService.hpp`): Loads theme YAML, 13 color Q_PROPERTYs with day/night support. Currently loads one theme — needs multi-theme discovery.
- `IDisplayService` (`src/core/services/IDisplayService.hpp`): Interface defined with `brightness()`, `setBrightness(int)`, `screenSize()`, `orientation()`. No implementation exists.
- `FullScreenPicker` (`qml/controls/FullScreenPicker.qml`): Modal picker used for resolution/codec selection — could work for theme selection.
- `SettingsSlider` (`qml/controls/SettingsSlider.qml`): Config-bound slider — already used for brightness in DisplaySettings.
- `GestureOverlay` (`qml/components/GestureOverlay.qml`): Has brightness slider stub ready to wire.
- `Wallpaper.qml` (`qml/components/Wallpaper.qml`): Currently a solid Rectangle — needs to support images or visual options.
- `UiMetrics` (`qml/controls/UiMetrics.qml`): Responsive sizing singleton — use for all new UI elements.

### Established Patterns
- Config via `YamlConfig` with `display.theme` key (already defaults to "default")
- Theme files in `config/themes/{name}/theme.yaml` — extend this for bundled themes
- User data in `~/.openauto/` — themes and wallpapers follow this pattern
- Q_PROPERTY bindings for live QML updates (ThemeService pattern)
- Services exposed to QML via context properties in `main.cpp`

### Integration Points
- `DisplaySettings.qml`: Replace read-only theme field with interactive picker, add wallpaper picker
- `ThemeService`: Add theme enumeration, switching between loaded themes
- `main.cpp`: Create and register `IDisplayService` implementation
- `GestureOverlay.qml`: Wire brightness slider to DisplayService
- `ConfigService`/`YamlConfig`: Persist theme, wallpaper, brightness selections

</code_context>

<deferred>
## Deferred Ideas

- Web config panel theme/wallpaper/brightness controls — separate scope, possibly its own phase
- Theme marketplace or community theme sharing
- Dynamic wallpapers (animated, time-based)

</deferred>

---

*Phase: 02-theme-display*
*Context gathered: 2026-03-01*
