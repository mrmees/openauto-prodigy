# Phase 2: Theme & Display - Research

**Researched:** 2026-03-01
**Domain:** Qt 6 QML theming, Linux display brightness control, YAML config persistence
**Confidence:** HIGH

## Summary

This phase extends the existing ThemeService (already loads YAML themes with day/night colors and emits Q_PROPERTY signals) to support multi-theme discovery and switching, adds wallpaper support to the existing `Wallpaper.qml`, implements `IDisplayService` for brightness control, and wires everything into the settings UI.

The codebase is already well-structured for this work. ThemeService has the YAML loading, color Q_PROPERTYs, and `colorsChanged()` signal chain. `IDisplayService` interface is defined but unimplemented. `FullScreenPicker` handles modal selection UI with config persistence. The main work is: (1) theme directory scanning + `setTheme()` implementation, (2) a `DisplayService` concrete class with brightness backends, (3) 3+ bundled theme YAML files, and (4) wiring existing UI stubs.

**Primary recommendation:** Extend ThemeService with directory scanning and implement DisplayService with a 3-tier brightness strategy: sysfs backlight > ddcutil > QML overlay fallback. Ship 3-4 bundled palettes as YAML files with optional wallpaper images. Use `FullScreenPicker` for theme/wallpaper selection in DisplaySettings.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Ship 3-4 bundled palettes that look good on a 1024x600 car display
- Support user-added themes via `~/.openauto/themes/` -- scan directory, show alongside bundled themes
- Display may not have sysfs backlight (DFRobot HDMI display) -- plan for both hardware backlight (`/sys/class/backlight/`) and software dimming fallback (dark overlay or gamma)
- Brightness persists across reboots via `display.brightness` in config.yaml (config path already exists in DisplaySettings)
- Wire the existing GestureOverlay brightness slider stub to the actual brightness control
- Web config panel support for theme/wallpaper/brightness is out of scope for this phase

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

### Deferred Ideas (OUT OF SCOPE)
- Web config panel theme/wallpaper/brightness controls -- separate scope, possibly its own phase
- Theme marketplace or community theme sharing
- Dynamic wallpapers (animated, time-based)
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DISP-01 | User can select from multiple color palettes in settings | ThemeService extension with directory scanning + FullScreenPicker UI |
| DISP-02 | User can select a wallpaper from available options in settings | Wallpaper.qml upgrade from Rectangle to Image + wallpaper picker |
| DISP-03 | Selected theme and wallpaper persist across restarts | ConfigService `display.theme` and `display.wallpaper` keys + YAML persistence |
| DISP-04 | Screen brightness is controllable from settings slider and gesture overlay | DisplayService QObject + wire SettingsSlider and GestureOverlay |
| DISP-05 | Brightness control writes to actual display backlight hardware on Pi | 3-tier strategy: sysfs > ddcutil > QML overlay fallback |
</phase_requirements>

## Standard Stack

### Core (All Existing -- No New Dependencies)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt 6 (QML) | 6.4/6.8 | UI rendering, property bindings | Already the app framework |
| yaml-cpp | existing | Theme YAML parsing | Already used by ThemeService and YamlConfig |
| Qt QTest | existing | Unit tests | Already used for all tests |

### Supporting (Possibly New)

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| ddcutil | 2.x | DDC/CI brightness over HDMI I2C | Only if sysfs backlight absent; Pi 4 I2C buses at /dev/i2c-11, /dev/i2c-12 |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| ddcutil CLI | libddcutil C API | CLI is simpler, async QProcess call. Library adds compile dep. CLI is fine for brightness (called rarely). |
| QML overlay dimming | wlr-gamma-control protocol | Gamma control is compositor-level (better), but requires Wayland protocol client code. QML overlay is simpler, works everywhere, good enough for v1.0. |
| Per-theme wallpaper images | Independent wallpaper selection | Independent is more flexible but more complex. Recommend: themes can include default wallpaper, but user can override independently. |

**No new library installations required.** ddcutil may need `apt install ddcutil` on the Pi if DDC/CI path is used, but the software fallback makes it optional.

## Architecture Patterns

### Recommended Changes to Existing Structure

```
config/themes/
  default/           # EXISTING - extend with wallpaper
    theme.yaml
    wallpaper.jpg    # NEW (optional)
  midnight/          # NEW bundled theme
    theme.yaml
    wallpaper.jpg
  ocean/             # NEW bundled theme
    theme.yaml
    wallpaper.jpg
  amoled/            # NEW bundled theme (no day/night needed)
    theme.yaml

~/.openauto/
  themes/            # User-added themes (scanned alongside bundled)
  wallpapers/        # DEFER to future -- user wallpapers without full themes
```

### Pattern 1: ThemeService Multi-Theme Extension

**What:** Extend ThemeService to scan theme directories, build a list model, and implement `setTheme()`.
**When to use:** At app startup and when user selects a new theme.

The existing `setTheme()` stub returns false. Implement it to:
1. Scan both `config/themes/` (bundled, relative to executable) and `~/.openauto/themes/` (user)
2. Build a `QStringList` or lightweight model of available theme IDs + display names
3. `setTheme(id)` loads the matching directory, emits `colorsChanged()`, persists to config

```cpp
// Additions to ThemeService.hpp:
Q_PROPERTY(QStringList availableThemes READ availableThemes NOTIFY availableThemesChanged)
Q_PROPERTY(QStringList availableThemeNames READ availableThemeNames NOTIFY availableThemesChanged)
Q_PROPERTY(QString wallpaperSource READ wallpaperSource NOTIFY wallpaperChanged)

// Key methods:
void scanThemeDirectories(const QStringList& searchPaths);
QStringList availableThemes() const;  // IDs
QStringList availableThemeNames() const;  // Display names
QString wallpaperSource() const;  // file:// URL or "" for solid color
```

**Why QStringList over QAbstractListModel:** FullScreenPicker already works with `options` (display strings) and `values` (stored values). Two QStringLists (IDs + names) slot directly into the existing picker pattern with zero new model code.

### Pattern 2: DisplayService Implementation

**What:** Concrete `IDisplayService` implementation as a QObject with brightness backends.
**When to use:** Registered in main.cpp, exposed to QML, wired to GestureOverlay and SettingsSlider.

```cpp
class DisplayService : public QObject, public IDisplayService {
    Q_OBJECT
    Q_PROPERTY(int brightness READ brightness WRITE setBrightness NOTIFY brightnessChanged)

public:
    explicit DisplayService(QObject* parent = nullptr);

    // IDisplayService
    QSize screenSize() const override;
    int brightness() const override;
    void setBrightness(int value) override;
    QString orientation() const override;

    // Software dimming for QML binding (0.0 - 1.0 opacity of dark overlay)
    Q_INVOKABLE qreal dimOverlayOpacity() const;

signals:
    void brightnessChanged();

private:
    enum class Backend { SysfsBacklight, DdcUtil, SoftwareOverlay };
    Backend detectBackend();

    Backend backend_;
    int brightness_ = 80;
    QString sysfsPath_;  // e.g., "/sys/class/backlight/rpi_backlight/brightness"
    int sysfsMax_ = 255;
};
```

**3-tier brightness strategy:**

1. **sysfs backlight** (`/sys/class/backlight/*/brightness`): Scan for any backlight device. The official Pi DSI display exposes `rpi_backlight`. Some HDMI driver boards expose one too. If found, read `max_brightness`, scale 0-100 to 0-max. Requires write permission (udev rule or run as user in `video` group).

2. **ddcutil** (DDC/CI over HDMI I2C): If no sysfs backlight, attempt `ddcutil setvcp 0x10 <value>` via QProcess. DFRobot display may or may not support DDC/CI -- it's a cheap panel, so LOW confidence. The I2C buses on Pi 4 are `/dev/i2c-11` and `/dev/i2c-12` for HDMI. This is a "try it" backend.

3. **QML software overlay** (guaranteed fallback): A full-screen dark `Rectangle` with opacity mapped to brightness. 100% brightness = 0 opacity (invisible). Minimum brightness (e.g., 5%) = 0.85 opacity. Not true backlight control, but visually effective. This always works.

### Pattern 3: Wallpaper System

**What:** Upgrade Wallpaper.qml from solid color to image support.
**When to use:** Home screen and launcher background.

```qml
// Wallpaper.qml (upgraded)
Item {
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: ThemeService.backgroundColor
    }

    Image {
        anchors.fill: parent
        source: ThemeService.wallpaperSource
        fillMode: Image.PreserveAspectCrop
        visible: source != ""
        asynchronous: true
    }
}
```

**Wallpaper approach (discretion decision):** Ship JPEG wallpaper images at 1024x600 with bundled themes. Each theme can optionally include `wallpaper.jpg` (or `wallpaper-day.jpg` / `wallpaper-night.jpg` for day/night aware wallpapers). If no wallpaper image exists, the solid `backgroundColor` shows through. This satisfies "select a wallpaper from available options" with zero complexity beyond file existence checks.

**Wallpaper selection model:** Independent from theme. Config key `display.wallpaper` stores a path (relative to theme dir or absolute). When user picks a theme, wallpaper defaults to the theme's bundled wallpaper. User can then override with a different wallpaper from any theme or a custom path. This gives flexibility without complexity.

### Anti-Patterns to Avoid

- **Building a ThemeListModel QAbstractListModel:** Overkill for 3-10 themes. QStringList properties work with FullScreenPicker and avoid boilerplate.
- **SVG or dynamic wallpapers:** Fixed 1024x600 display. JPEG is instant to load. SVG adds rendering overhead for no benefit.
- **Gamma LUT manipulation via Wayland protocol:** Too complex for v1.0. QML overlay achieves the visual effect with 3 lines of QML.
- **Direct I2C calls for DDC/CI:** Use ddcutil CLI via QProcess. It handles timing, retries, and bus detection.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Theme YAML parsing | Custom parser | yaml-cpp (already used) | Already integrated, handles edge cases |
| Config persistence | Manual file I/O | ConfigService.setValue() + save() | Already debounced, thread-safe, integrated with QML |
| Modal picker UI | Custom dialog | FullScreenPicker.qml | Already built, themed, handles config binding |
| DDC/CI protocol | Raw I2C commands | ddcutil CLI via QProcess | Handles timing, retries, bus enumeration |
| Image scaling | Manual QImage resize | QML Image with PreserveAspectCrop | GPU-accelerated on Pi, handles async loading |

**Key insight:** Almost everything needed already exists in the codebase. This phase is primarily wiring and extending, not building from scratch.

## Common Pitfalls

### Pitfall 1: ThemeService Losing Current Colors on Scan

**What goes wrong:** Calling `scanThemeDirectories()` during startup could reset the currently loaded theme.
**Why it happens:** If scan triggers a reload or clears state before re-loading the active theme.
**How to avoid:** Scan populates the available list only. Active theme loading happens separately via `setTheme()` with the config-persisted ID.
**Warning signs:** Colors flash to transparent on startup.

### Pitfall 2: sysfs Backlight Permission Denied

**What goes wrong:** Writing to `/sys/class/backlight/*/brightness` fails with EACCES.
**Why it happens:** sysfs backlight files are owned by root. User `matt` (or whoever runs the app) needs write access.
**How to avoid:** udev rule: `SUBSYSTEM=="backlight", ACTION=="add", RUN+="/bin/chmod g+w /sys/class/backlight/%k/brightness"` and add user to `video` group. Installer should handle this.
**Warning signs:** Brightness slider moves but display doesn't change; check stderr for permission errors.

### Pitfall 3: QML Image Source Not Updating

**What goes wrong:** Changing `ThemeService.wallpaperSource` doesn't update the displayed wallpaper.
**Why it happens:** QML Image caches by URL. If the property changes to the same filename in a different theme dir, Qt may serve the cached version.
**How to avoid:** Use full absolute `file://` URLs. Append a cache-bust query param if needed (`?t=timestamp`), or ensure each theme's wallpaper has a distinct path.
**Warning signs:** Wallpaper shows previous theme's image after switching.

### Pitfall 4: Brightness Slider Feedback Loop

**What goes wrong:** Setting brightness from QML triggers a config save, which triggers a config change signal, which updates the slider, which triggers another save.
**Why it happens:** SettingsSlider auto-saves on `onMoved`. DisplayService also listens to config changes.
**How to avoid:** Use the SettingsSlider's existing debounce pattern. DisplayService reads brightness from its own property, not from config change signals. Config is the persistence layer, not the live control path. Slider -> DisplayService.setBrightness() -> hardware. Config save is a side effect, not the control loop.
**Warning signs:** Slider jitters, CPU spikes, or rapid disk writes.

### Pitfall 5: Bundled Theme Path Resolution

**What goes wrong:** Bundled themes not found when running from build dir vs installed location.
**Why it happens:** `QCoreApplication::applicationDirPath()` differs between build (`build/src/`) and installed locations.
**How to avoid:** Follow existing pattern in main.cpp: try `~/.openauto/themes/` first, then `applicationDirPath() + "/../../config/themes/"` (build layout). Current code already does this for the default theme.
**Warning signs:** "No themes found" when themes exist in repo tree.

### Pitfall 6: DFRobot Display Has No DDC/CI

**What goes wrong:** ddcutil hangs or returns "No displays found" on the DFRobot HDMI display.
**Why it happens:** Cheap HDMI display boards often don't implement DDC/CI. The DFRobot 1024x600 uses a physical potentiometer for brightness.
**How to avoid:** Backend detection must have a timeout and graceful fallback. Try sysfs first (instant), try ddcutil with a 2-second QProcess timeout, fall back to software overlay. Log which backend was selected.
**Warning signs:** 2+ second delay on first brightness change while ddcutil probes fail.

## Code Examples

### Theme Scanning (C++ -- ThemeService extension)

```cpp
// Source: Extending existing ThemeService pattern
void ThemeService::scanThemeDirectories(const QStringList& searchPaths)
{
    availableThemes_.clear();
    availableThemeNames_.clear();

    for (const auto& basePath : searchPaths) {
        QDir dir(basePath);
        if (!dir.exists()) continue;

        for (const auto& entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString yamlPath = dir.filePath(entry + "/theme.yaml");
            if (!QFile::exists(yamlPath)) continue;

            try {
                YAML::Node root = YAML::LoadFile(yamlPath.toStdString());
                QString id = QString::fromStdString(root["id"].as<std::string>(""));
                QString name = QString::fromStdString(root["name"].as<std::string>(entry.toStdString()));
                if (!id.isEmpty() && !availableThemes_.contains(id)) {
                    availableThemes_.append(id);
                    availableThemeNames_.append(name);
                    themeDirectories_[id] = dir.filePath(entry);
                }
            } catch (const YAML::Exception&) {
                // Skip malformed theme files
            }
        }
    }
    emit availableThemesChanged();
}
```

### DisplayService Brightness Backend Detection

```cpp
// Source: New DisplayService implementation
DisplayService::Backend DisplayService::detectBackend()
{
    // 1. Check sysfs backlight
    QDir backlightDir("/sys/class/backlight");
    if (backlightDir.exists()) {
        for (const auto& entry : backlightDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString path = backlightDir.filePath(entry + "/brightness");
            if (QFile::exists(path)) {
                sysfsPath_ = backlightDir.filePath(entry);
                QFile maxFile(sysfsPath_ + "/max_brightness");
                if (maxFile.open(QIODevice::ReadOnly))
                    sysfsMax_ = QString(maxFile.readAll().trimmed()).toInt();
                qCInfo(lcDisplay) << "Using sysfs backlight:" << sysfsPath_;
                return Backend::SysfsBacklight;
            }
        }
    }

    // 2. Try ddcutil (async probe with timeout)
    // ... QProcess with 2s timeout ...

    // 3. Fallback: software overlay
    qCInfo(lcDisplay) << "No hardware backlight found, using software dimming overlay";
    return Backend::SoftwareOverlay;
}
```

### DisplaySettings Theme Picker (QML)

```qml
// Source: Extending existing DisplaySettings.qml pattern
FullScreenPicker {
    label: "Theme"
    options: ThemeService.availableThemeNames
    values: ThemeService.availableThemes
    configPath: "display.theme"
    onActivated: function(index) {
        ThemeService.setTheme(ThemeService.availableThemes[index])
    }
}
```

### GestureOverlay Brightness Wiring (QML)

```qml
// Source: Closing existing TODO in GestureOverlay.qml
Slider {
    id: brightnessSlider
    Layout.fillWidth: true
    from: 5    // Minimum floor to prevent black screen
    to: 100
    value: DisplayService.brightness
    onValueChanged: {
        DisplayService.setBrightness(Math.round(value))
        dismissTimer.restart()
    }
}
```

### Software Dimming Overlay (QML)

```qml
// Source: Fallback brightness for displays without hardware control
// Placed as topmost child in Shell.qml, below GestureOverlay z-order
Rectangle {
    anchors.fill: parent
    color: "black"
    opacity: DisplayService.dimOverlayOpacity  // 0.0 at 100%, ~0.85 at 5%
    visible: opacity > 0
    z: 998  // Below GestureOverlay (999), above everything else
    // Must not capture mouse events
    enabled: false
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Theme is "default" only (hardcoded load path) | Multi-theme with directory scanning | This phase | Users can pick themes |
| Wallpaper.qml is solid Rectangle | Image with fallback to solid color | This phase | Visual personalization |
| IDisplayService is interface-only stub | Concrete DisplayService with 3 backends | This phase | Brightness actually works |
| GestureOverlay brightness slider is TODO | Wired to DisplayService | This phase | Quick brightness adjustment |

**Nothing deprecated.** All changes extend existing patterns.

## Discretion Recommendations

### Palette Colors and Names (3-4 bundled themes)

1. **Default** (existing): Dark navy/red accent. Day: `#1a1a2e` bg, `#e94560` highlight. Night: dimmed variants. Keep as-is.
2. **Ocean**: Cool blue/teal. Day: `#1b2838` bg, `#66c0f4` highlight. Night: deeper `#0e1a26`. Calm, easy on eyes.
3. **Ember**: Warm dark with amber/orange accent. Day: `#1e1712` bg, `#ff8c42` highlight. Night: `#120e0a`. Classic car dashboard feel.
4. **AMOLED**: Pure black, minimal accent. Day AND night: `#000000` bg, `#ffffff` text, `#4fc3f7` subtle accent. No separate day/night needed -- it's already maximally dark.

### Day/Night Variant Strategy

- Most themes: provide both day and night color maps (dimmer night = less eye strain at night)
- AMOLED: single set (already optimal for darkness)
- Night mode fallback already works (ThemeService falls back to day colors for missing night keys)

### Wallpaper Approach

Ship 2-3 abstract/geometric JPEG wallpapers at 1024x600 with the bundled themes. Not every theme needs one. Solid color background is the default if no wallpaper. This satisfies the "select from available options" criterion without bloating the binary or requiring an image pipeline.

### Theme-Wallpaper Coupling

Loosely coupled. Themes can bundle a default wallpaper. User picks theme -> default wallpaper loads. User can then pick a different wallpaper from the available set (all wallpapers from all themes + user wallpapers). Config stores `display.theme` (theme ID) and `display.wallpaper` (wallpaper path, empty = theme default).

### Brightness Minimum Floor

5% (from: 5 in slider). Prevents black screen panic. At 5%, software overlay is opacity ~0.85 (still visible text). sysfs/DDC would map to ~5% of max hardware value.

### Brightness + Day/Night

Manual-only for this phase. No auto-brightness on day/night switch. Users in cars have widely varying ambient light preferences. Auto-adjust can be added later as an opt-in feature.

### Color Transition

Instant snap, no animation. In a car, you want immediate feedback. Fade animations are cute but waste the user's attention. Qt property bindings already handle this -- `colorsChanged()` triggers instant QML updates.

### User-Added Wallpapers

Defer `~/.openauto/wallpapers/` scanning to a future phase. User-added *themes* (which can include wallpapers) are in scope via `~/.openauto/themes/`. This satisfies personalization without a separate wallpaper management system.

## Open Questions

1. **DFRobot DDC/CI support**
   - What we know: DFRobot 1024x600 HDMI display uses a physical potentiometer for brightness. Cheap panels rarely implement DDC/CI.
   - What's unclear: Whether this specific panel responds to DDC/CI commands at all.
   - Recommendation: Matt should run `ddcutil detect` and `ddcutil capabilities` on the Pi to verify. Software overlay fallback means this isn't blocking. If DDC/CI works, great bonus. If not, software overlay is the permanent solution for this hardware.

2. **sysfs backlight on HDMI**
   - What we know: sysfs backlight is typically for DSI displays only. HDMI displays on Pi 4 don't usually expose a sysfs backlight device.
   - What's unclear: Whether the Pi 4's KMS driver exposes any backlight interface for HDMI.
   - Recommendation: Code defensively -- scan /sys/class/backlight/, use if present, fall through to ddcutil/software. The detection runs once at startup.

## Sources

### Primary (HIGH confidence)
- Existing codebase: ThemeService.hpp/cpp, IDisplayService.hpp, GestureOverlay.qml, DisplaySettings.qml, FullScreenPicker.qml, main.cpp
- Existing research: `.planning/research/ARCHITECTURE.md` (theme selection system design)
- Existing research: `.planning/research/STACK.md` (wallpaper approach)

### Secondary (MEDIUM confidence)
- [Arch Wiki - Backlight](https://wiki.archlinux.org/title/Backlight) - sysfs, DDC/CI, software approaches
- [ddcutil Raspberry Pi documentation](https://www.ddcutil.com/raspberry/) - I2C bus /dev/i2c-11, /dev/i2c-12
- [Linux kernel backlight documentation](https://docs.kernel.org/gpu/backlight.html) - sysfs interface spec
- [Raspberry Pi Forums - HDMI backlight](https://forums.raspberrypi.com/viewtopic.php?t=320423) - confirms HDMI displays lack sysfs backlight

### Tertiary (LOW confidence)
- DFRobot DDC/CI support - unverified, needs Pi testing
- ddcutil reliability on Pi 4 HDMI - community reports are mixed

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - no new dependencies, extends existing code
- Architecture: HIGH - follows established patterns (ThemeService, QML controls, config persistence)
- Pitfalls: HIGH - identified from code analysis and existing gotchas in CLAUDE.md
- Brightness hardware: LOW - DFRobot DDC/CI unknown, sysfs unlikely for HDMI. Software fallback is HIGH confidence.

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (stable domain, no fast-moving dependencies)
