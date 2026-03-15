---
status: resolved
trigger: "Diagnose why custom wallpaper selection does not persist when switching themes"
created: 2026-03-01T00:00:00Z
updated: 2026-03-01T22:00:00Z
---

## Current Focus

hypothesis: CONFIRMED — wallpaper_override missing from YamlConfig defaults tree, causing silent save failure
test: Checked YamlConfig constructor defaults (line 20-24) and setValueByPath validation (line 703-719)
expecting: wallpaper_override absent from defaults -> setValueByPath rejects the write -> value never persisted
next_action: Report root cause

## Symptoms

expected: Custom wallpaper persists across theme changes and app restarts
actual: Wallpaper reverts when theme is switched (or on restart)
errors: none — failure is silent
reproduction: Select custom wallpaper, switch theme (or restart app), wallpaper reverts
started: unknown

## Eliminated

- hypothesis: setTheme() clears wallpaperOverride_ member
  evidence: Code review shows setTheme() (line 123-134) calls loadTheme() then resolveWallpaper(). Neither modifies wallpaperOverride_.
  timestamp: 2026-03-01

- hypothesis: resolveWallpaper() ignores override after theme change
  evidence: Code at line 199-202 unconditionally checks wallpaperOverride_ first, regardless of theme state.
  timestamp: 2026-03-01

- hypothesis: availableWallpapersChanged fires during theme switch causing QML reset
  evidence: That signal only fires in scanThemeDirectories() (line 120/185), called once at startup, not during setTheme().
  timestamp: 2026-03-01

## Evidence

- timestamp: 2026-03-01
  checked: YamlConfig constructor defaults (src/core/YamlConfig.cpp lines 20-24)
  found: display section defines brightness, theme, orientation, width, height. NO wallpaper_override key.
  implication: wallpaper_override is not in the defaults tree.

- timestamp: 2026-03-01
  checked: YamlConfig::setValueByPath() validation (src/core/YamlConfig.cpp lines 703-719)
  found: Validates path against defaults tree. If path not found in defaults, returns false without writing.
  implication: ConfigService.setValue("display.wallpaper_override", ...) silently fails — value never written to YAML.

- timestamp: 2026-03-01
  checked: ConfigService::setValue() (src/core/services/ConfigService.cpp lines 16-21)
  found: Only emits configChanged if setValueByPath returns true. On failure, no signal, no error logged.
  implication: Complete silent failure — no indication to caller or user that save failed.

- timestamp: 2026-03-01
  checked: main.cpp startup (lines 132-134)
  found: Reads display.wallpaper_override from YAML. If invalid (never saved), setWallpaperOverride not called.
  implication: On restart, wallpaper override is always lost because it was never persisted.

- timestamp: 2026-03-01
  checked: ThemeService in-memory behavior
  found: wallpaperOverride_ survives setTheme() calls during runtime. resolveWallpaper() respects it.
  implication: During a SINGLE session, wallpaper override works. Bug manifests on restart or when QML recreates the picker.

- timestamp: 2026-03-01
  checked: FullScreenPicker Component.onCompleted
  found: Reads config value to set currentIndex. If value was never saved, picker shows no selection / "Theme Default".
  implication: If settings page is destroyed and recreated (navigation), picker visual state also reverts.

## Resolution

root_cause: `display.wallpaper_override` is missing from YamlConfig's defaults tree (src/core/YamlConfig.cpp). The setValueByPath() method validates all writes against the defaults tree and silently rejects any path not found there. When the FullScreenPicker saves the wallpaper selection via ConfigService.setValue("display.wallpaper_override", value), the write silently fails. The value is never persisted to config.yaml, so it is lost on app restart. The in-memory wallpaperOverride_ in ThemeService works correctly during runtime, but since the config never saves, the override cannot survive a restart or a QML component re-creation cycle.
fix: Add `root_["display"]["wallpaper_override"] = "";` to YamlConfig constructor defaults (after line 22)
verification:
files_changed: []
