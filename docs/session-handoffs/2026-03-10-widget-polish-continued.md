# Session Handoff: Widget System & UI Polish (continued)

**Date:** 2026-03-10 (evening session)
**Branch:** `dev/0.5.2`
**Milestone:** v0.5.2 — Widget System Implementation and General UI Improvements

## What Was Done This Session

### Widget & Home Screen Fixes
- **AA widget long-press:** AAStatusWidget forwards pressAndHold to WidgetHost via `requestContextMenu()` on Loader
- **Widget picker "None" option:** "No Widget" entry (empty id, close icon) prepended in WidgetPickerModel; HomeMenu clears pane when selected
- **Dock icons:** Text labels removed, icons sized to 90% of button height (not iconSmall)
- **Dock icon fix:** `width`/`height` → `implicitWidth`/`implicitHeight` in RowLayout; added `opticalSize` for proper glyph scaling
- **Icon updates:** AA→android (0xe859), Music→media_bluetooth_on (0xf032), Phone→phone_in_talk (0xe61d)

### Theme Fixes
- **Force dark mode at startup:** `activeColor()` was reading `nightMode_` field directly instead of calling `nightMode()` method (which checks `forceDarkMode_ || nightMode_`)
- **Time-based night mode at startup:** main.cpp now evaluates time config and calls `setNightMode()` before UI loads — previously only ran during AA connection

### Settings Menu Overhaul
- **Main settings menu:** Replaced GridLayout of Tile components with scrollable ListView
- **Layout:** Icon left-justified (70% row height), text right-justified (45% row height), `spacing * 3` padding
- **Alternating rows:** surfaceContainer / surfaceContainerHigh backgrounds, primaryContainer press state
- **Scrollable:** clip + VerticalFlick for small screens

## Pi Config Note
Launcher tiles were cleared from Pi's `~/.openauto/config.yaml` to pick up new default icons. If icons revert, clear `launcher.tiles` again.

## Next Up: Phase 2 — Settings Cleanup & Touch-Friendly Normalization

### Proposed Changes (discussed, not yet implemented)

**Demote to YAML-only (remove from UI):**
- AA Settings: Video Decoding section (codec enable/disable, hw/sw, decoder picker), TCP port, entire Debug/protocol capture section
- Connection Settings: WiFi AP channel/band (set at install), BT device name
- Display Settings: Night mode time fields (day_start/night_start), GPIO pin/active-high
- System Settings: All Identity fields (HU name, manufacturer, model, sw version, car model/year), hardware profile, touch device, Debug AA Protocol buttons

**Keep in UI:**
- AA: Resolution, FPS, DPI, Auto-connect
- Display: Brightness, Theme, Wallpaper, Clock 24h, Dark mode toggle, Night mode source, Navbar position, Navbar show during AA
- Audio: Unchanged (volume, output device, mic gain, input device, EQ)
- Bluetooth: Accept pairings toggle, paired devices list (remove AA/protocol duplication)
- Companion: Unchanged
- System: Left-hand drive, About section, Close App

**Touch-friendly normalization:**
- All remaining settings pages need the same treatment as the main menu — readable text, proper touch targets, consistent styling
- Remove duplicate settings between AA and Connection pages

### Screenshot
Settings main menu screenshot taken — clean list layout with icons left, text right, alternating row backgrounds. See `/tmp/pi-screenshot.png`.

## Commits This Session
```
575639c fix(ui): increase settings menu padding and text size
d384264 refactor(ui): settings menu from tile grid to scrollable list
1653eed fix(ui): use media_bluetooth_on icon for BT audio
8c9a9e8 fix(ui): update dock icons — android, bluetooth_audio, phone_in_talk
bc84f9e fix(theme): evaluate time-based night mode at startup
712ad86 fix(theme): activeColor() now respects forceDarkMode at startup
e1d4ec9 fix(ui): use implicitWidth/Height for dock items in RowLayout
a11ad04 fix(ui): size dock icons to 90% of button height, not iconSmall
e4a04a7 fix(ui): widget long-press, picker remove option, icon-only navbar
```

## Gotchas Learned
- **RowLayout ignores `width`/`height`** — must use `implicitWidth`/`implicitHeight` for children to be sized correctly
- **MaterialIcon `opticalSize`** — variable font opsz axis must be set to match rendered size (capped at 48) for proper glyph design at large sizes
- **Qt 6.8 cachegen** — QML-only changes still need a cross-build because QML is compiled into the binary. Always cross-build + rsync, don't just git pull.
- **`nightMode()` vs `nightMode_`** — method respects forceDarkMode, field does not. Always call the method.
- **TimedNightMode only runs during AA connection** — must evaluate time at startup independently in main.cpp
