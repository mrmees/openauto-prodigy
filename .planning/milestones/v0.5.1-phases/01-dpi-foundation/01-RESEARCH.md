# Phase 1: DPI Foundation - Research

**Researched:** 2026-03-08
**Domain:** EDID parsing, DPI-based UI scaling, Qt 6 QML metrics, bash installer integration
**Confidence:** HIGH

## Summary

This phase replaces the hardcoded resolution-ratio scaling in UiMetrics with DPI-based scaling derived from the physical screen size. The work spans four layers: (1) EDID parsing in the bash installer to detect physical screen dimensions, (2) a new `display.screen_size` config key with 7.0" default, (3) DPI math in UiMetrics.qml replacing the `_winW / 1024.0` baseline, and (4) a read-only info display in Display settings.

The existing codebase is well-structured for this change. `DisplayInfo` already bridges window dimensions to QML, `ConfigService` already provides reactive dot-notation config reads, and `UiMetrics.qml` is a singleton with ~50 tokens that all flow from two scale factors. The formula change is surgical -- replace the denominator, not the architecture.

**Primary recommendation:** Parse EDID detailed timing descriptors (mm precision) with fallback to basic display params (cm precision), add `screenSizeInches` to `DisplayInfo` C++ class fed from config, and update UiMetrics to compute `actual_DPI / 160` instead of `windowWidth / 1024`.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Parse raw EDID from `/sys/class/drm/*/edid` -- extract physical dimensions from detailed timing descriptors (mm precision) or basic display params bytes 21-22 (cm precision)
- No extra packages needed -- works on any Linux with DRM
- Convert mm dimensions to diagonal inches for storage
- EDID detection happens during hardware detection step in installer (alongside WiFi cap probing)
- If EDID succeeds: show detected size with accept/override prompt
- If EDID fails or user skips: default to 7" (standard double-DIN)
- Input is numeric entry in inches, accepts decimals (e.g., 10.1)
- Installer writes only `display.screen_size` (inches) to config -- pixel resolution is auto-detected by app at runtime
- New config key: `display.screen_size` -- diagonal inches as float (e.g., `7.0`)
- DPI computed at runtime: diagonal_pixels / screen_size_inches
- Reference DPI is 160 PPI (Android mdpi standard)
- Replace current resolution-ratio baseline entirely -- no more hardcoded 1024/600 divisor
- Preserve dual-axis approach: min(H,V) for layout, sqrt(H*V) for fonts -- DPI replaces the baseline denominator
- Screen size is NOT editable in settings -- set once at install, edit YAML to change
- Read-only informational display at top of Display settings page (before brightness)
- Shows both size and computed DPI: "Screen: 7.0" / 170 PPI"

### Claude's Discretion
- Exact EDID parsing implementation details (error handling for malformed EDID)
- How DisplayInfo exposes physical size to QML (new property or new service)
- Config key naming beyond `display.screen_size`
- Read-only display styling in settings

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DPI-01 | Installer probes EDID for physical screen dimensions and prompts user to confirm or enter screen size | EDID byte layout documented, bash parsing approach with `od` verified, installer integration point identified (Step 2: `setup_hardware()`) |
| DPI-02 | Default physical screen size is 7" when EDID unavailable and user skips entry | Default value in `initDefaults()` and installer `generate_config()`, fallback logic documented |
| DPI-03 | UiMetrics computes baseline scale from real DPI instead of pure resolution ratio | DPI formula documented, UiMetrics.qml current structure analyzed, surgical replacement path identified |
| DPI-04 | Physical screen size is persisted in YAML config and can be changed in Display settings | ConfigService pattern documented, DisplayInfo extension path identified, read-only settings UI pattern with existing ReadOnlyField control |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt 6.4/6.8 | 6.4 (dev), 6.8 (Pi) | QML UI, Q_PROPERTY binding | Already in use -- this is the framework |
| yaml-cpp | (in-tree) | YAML config persistence | Already in use for all config |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| cmath (std) | C++17 | `sqrt()`, `hypot()` for diagonal calculation | DPI computation in DisplayInfo |
| od / hexdump | coreutils | Binary EDID parsing in bash | Installer EDID probe (no extra packages) |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Raw EDID parsing in bash | `edid-decode` package | Extra dependency, not on minimal Trixie -- rejected per decision |
| DisplayInfo C++ property | New DpiService class | Over-engineered for a single float value -- use existing class |

## Architecture Patterns

### Recommended Changes

```
src/ui/DisplayInfo.hpp        # Add screenSizeInches Q_PROPERTY
src/ui/DisplayInfo.cpp        # Add setter/getter, computedDpi() method
src/core/YamlConfig.cpp       # Add display.screen_size default (7.0) in initDefaults()
qml/controls/UiMetrics.qml   # Replace 1024/600 baseline with DPI-based computation
qml/applications/settings/DisplaySettings.qml  # Add read-only screen info row
install.sh                    # Add EDID probe + prompt in setup_hardware()
```

### Pattern 1: EDID Parsing in Bash (Installer)

**What:** Read raw EDID binary from `/sys/class/drm/*/edid`, extract physical dimensions
**When to use:** During `setup_hardware()` step, after touch device detection

EDID binary layout for physical dimensions:
- **Basic display params:** Byte 21 = width in cm, byte 22 = height in cm (1-byte each, integer cm)
- **Detailed timing descriptor (bytes 54-71, first descriptor):** Bytes 66-67 = horizontal size mm (lower 8 bits), byte 68 = vertical size mm (lower 8 bits), byte 68 bits 7-4 = horizontal upper 4 bits, bits 3-0 = vertical upper 4 bits. This gives 12-bit mm precision (0-4095mm).

**Recommended approach:**
1. Find connected display: `ls /sys/class/drm/card*-*/edid 2>/dev/null`
2. Read binary with `od`: extract bytes at offsets for physical dimensions
3. Try detailed timing first (mm precision), fall back to basic params (cm precision)
4. Compute diagonal: `sqrt(w_mm^2 + h_mm^2) / 25.4` for inches
5. If EDID empty/zeroed (projector, VNC, etc), skip to manual prompt

```bash
# Example: read basic display params (cm) from EDID
# Bytes 21-22 (0-indexed) are at offset 21
read_edid_basic() {
    local edid_path="$1"
    local w_cm h_cm
    w_cm=$(od -An -tx1 -j21 -N1 "$edid_path" | tr -d ' ')
    h_cm=$(od -An -tx1 -j22 -N1 "$edid_path" | tr -d ' ')
    w_cm=$((16#$w_cm))
    h_cm=$((16#$h_cm))
    # Convert to mm for diagonal calc
    echo "$((w_cm * 10)) $((h_cm * 10))"
}

# Detailed timing descriptor has mm precision at bytes 12-14 within the descriptor
# First descriptor starts at byte 54, so bytes 66-68
read_edid_detailed() {
    local edid_path="$1"
    local b12 b13 b14 w_mm h_mm
    b12=$(od -An -tu1 -j66 -N1 "$edid_path" | tr -d ' ')  # h_size lower 8
    b13=$(od -An -tu1 -j67 -N1 "$edid_path" | tr -d ' ')  # v_size lower 8
    b14=$(od -An -tu1 -j68 -N1 "$edid_path" | tr -d ' ')  # upper 4+4 bits
    w_mm=$(( (( (b14 >> 4) & 0x0F ) << 8) | b12 ))
    h_mm=$(( (( b14 & 0x0F ) << 8) | b13 ))
    echo "$w_mm $h_mm"
}
```

**Confidence:** HIGH -- EDID spec is stable, sysfs paths confirmed on Pi DRM, `od` is in coreutils.

### Pattern 2: DPI Computation in UiMetrics

**What:** Replace `scaleH: _winW / 1024.0` with DPI-derived scale
**When to use:** UiMetrics.qml singleton

```javascript
// Current (resolution-ratio):
readonly property real scaleH: _winW / 1024.0
readonly property real scaleV: _winH / 600.0

// New (DPI-based):
readonly property real _screenSize: DisplayInfo ? DisplayInfo.screenSizeInches : 7.0
readonly property real _diagPx: Math.sqrt(_winW * _winW + _winH * _winH)
readonly property real _dpi: _diagPx / _screenSize
readonly property real _referenceDpi: 160.0  // Android mdpi

readonly property real scaleH: (_dpi / _referenceDpi) * (_winW / _diagPx) * Math.sqrt(_winW * _winW + _winH * _winH) / _referenceDpi
// Actually simpler -- scale factor is just dpi/160, applied uniformly:
// But we need dual-axis. The dual-axis logic uses H and V independently.
```

**Key insight for dual-axis DPI scaling:**

The current dual-axis system uses `_winW / 1024.0` and `_winH / 600.0` as independent axis scales. With DPI, the fundamental scale is `actualDPI / 160`. But we still want dual-axis behavior (min for layout, geometric mean for fonts). The way to preserve this:

```javascript
// DPI-based scale factor (uniform)
readonly property real _dpiScale: _dpi / _referenceDpi

// Aspect ratio correction for dual-axis (preserves overflow prevention)
// Reference aspect = 1024/600 = 1.707
// Actual aspect = _winW / _winH
// These correct for aspect ratio differences vs the 160dpi reference
readonly property real scaleH: _dpiScale * (_winW / _diagPx) / (1024.0 / Math.sqrt(1024*1024 + 600*600))
readonly property real scaleV: _dpiScale * (_winH / _diagPx) / (600.0 / Math.sqrt(1024*1024 + 600*600))
```

Actually, let's think about this more simply. The decision says "DPI replaces the baseline denominator, not the dual-axis logic." So:

```javascript
// Old: scaleH = windowWidth / 1024.0 (what's 1024? it's the "reference" width)
// New: what "reference" width corresponds to 160 DPI at this screen size?
//      At 160 DPI, a 7" 16:9.6 screen would be ~987 pixels wide
//      But that's coupling to aspect ratio...
//
// Simplest correct approach: uniform DPI scale, split into axes via aspect ratio
readonly property real _screenSize: {
    var v = ConfigService.value("display.screen_size");
    if (v !== undefined && v !== null && Number(v) > 0) return Number(v);
    return 7.0;
}
readonly property real _diagPx: Math.sqrt(_winW * _winW + _winH * _winH)
readonly property real _dpi: _diagPx / _screenSize
readonly property real _dpiScale: _dpi / 160.0

// For dual-axis: the DPI scale IS the uniform scale.
// scaleH and scaleV diverge only when the display aspect ratio
// differs from the reference. At 160 DPI reference, the "reference"
// dimensions for any display are:  refW = _winW / _dpiScale, refH = _winH / _dpiScale
// So: scaleH = _winW / refW = _dpiScale (same!)
// The dual-axis only matters when H and V have different DPIs (non-square pixels).
// For square pixels (all modern displays), scaleH == scaleV == _dpiScale.
//
// This means autoScale = min(scaleH, scaleV) = _dpiScale
// And _autoFontScale = sqrt(scaleH * scaleV) = _dpiScale
// The dual-axis logic naturally collapses to uniform when pixels are square.

readonly property real scaleH: _dpiScale
readonly property real scaleV: _dpiScale
```

**Wait -- this loses the dual-axis benefit.** The dual-axis was valuable when different resolutions have different aspect ratios (e.g., 800x480 vs 1024x600 vs 1920x1080). With pure DPI, a 7" 800x480 and 7" 1024x600 get the same DPI scale, but the 800x480 screen has fewer pixels per axis -- elements sized for 1024x600 at that DPI would overflow.

**Correct approach:** Keep dual-axis, but normalize against the "reference" pixel counts that 160 DPI implies for each screen's aspect ratio:

```javascript
readonly property real _dpiScale: _dpi / 160.0

// Reference pixel dimensions at 160 DPI for THIS screen's aspect ratio:
readonly property real _refW: _winW / _dpiScale
readonly property real _refH: _winH / _dpiScale

// Dual-axis: actual / reference (both axes independently)
readonly property real scaleH: _winW / _refW  // == _dpiScale (collapses for square pixels)
readonly property real scaleV: _winH / _refH  // == _dpiScale (collapses for square pixels)
```

This always collapses to `_dpiScale` for square pixels. The dual-axis only diverges for non-square pixels, which no modern display has. So the simplest implementation is:

```javascript
readonly property real scaleH: _dpiScale
readonly property real scaleV: _dpiScale
```

And keep `autoScale = Math.min(scaleH, scaleV)` and `_autoFontScale = Math.sqrt(scaleH * scaleV)` -- they both equal `_dpiScale` for square pixels, preserving the infrastructure for the theoretical non-square case.

**Confidence:** HIGH -- the math is straightforward. The key insight is that DPI-based scaling with square pixels makes the dual-axis identical, which is correct behavior.

### Pattern 3: DisplayInfo Extension

**What:** Add `screenSizeInches` Q_PROPERTY to existing DisplayInfo class
**When to use:** At app startup, read from ConfigService, expose to QML

```cpp
// DisplayInfo.hpp additions:
Q_PROPERTY(qreal screenSizeInches READ screenSizeInches NOTIFY screenSizeChanged)
Q_PROPERTY(int computedDpi READ computedDpi NOTIFY dpiChanged)

qreal screenSizeInches() const;
int computedDpi() const;
void setScreenSizeInches(qreal inches);

signals:
    void screenSizeChanged();
    void dpiChanged();
```

The `computedDpi` property is derived: `sqrt(w*w + h*h) / screenSizeInches`. It updates when either window size or screen size changes.

**Confidence:** HIGH -- follows established Q_PROPERTY patterns already in DisplayInfo.

### Pattern 4: Config Integration

**What:** Add `display.screen_size` to YamlConfig defaults and installer config generation
**When to use:** `initDefaults()` and `generate_config()`

```cpp
// YamlConfig.cpp initDefaults() addition:
root_["display"]["screen_size"] = 7.0;
```

```bash
# install.sh generate_config() addition:
display:
  screen_size: ${SCREEN_SIZE:-7.0}
  brightness: 80
```

**Confidence:** HIGH -- follows exact pattern of existing config keys.

### Anti-Patterns to Avoid
- **Hardcoding pixel dimensions as reference:** The whole point is to move away from `1024/600` constants. Don't introduce new hardcoded reference dimensions.
- **Creating a separate DpiService:** DisplayInfo already exists as the display bridge to QML. Adding a property is cleaner than a new service.
- **Making screen size editable in settings:** Decision is read-only display. Don't add setValue/stepper for DPI-04.
- **Using `edid-decode` or `read-edid` packages:** Not available on minimal Trixie. Parse raw bytes with `od`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Config persistence | Custom file I/O | `ConfigService.setValue()` + `save()` | Already handles YAML deep merge, dot-notation paths |
| QML reactive binding | Manual signal wiring | Q_PROPERTY + `ConfigService.value()` binding | Existing pattern, auto-updates |
| Installer prompts | New prompt framework | Existing `read -p` + TUI step infrastructure | Already battle-tested in install.sh |

## Common Pitfalls

### Pitfall 1: EDID Returns Zero Dimensions
**What goes wrong:** Projectors, VNC, and some cheap displays report 0x0 for physical dimensions
**Why it happens:** EDID spec says bytes 21-22 = 0 means "undefined" (e.g., projector)
**How to avoid:** Check for zero/missing values, fall through to manual prompt with 7" default
**Warning signs:** Both width and height are 0, or EDID file is empty/missing

### Pitfall 2: EDID File Not Readable Without Root
**What goes wrong:** `/sys/class/drm/*/edid` may require root or specific group membership
**Why it happens:** DRM device permissions vary by distro
**How to avoid:** Installer already runs with sudo context. If read fails, treat as "no EDID" and prompt.
**Warning signs:** Empty read from `od`, permission denied

### Pitfall 3: Multiple DRM Cards / Connectors
**What goes wrong:** System has multiple `/sys/class/drm/card*-*` entries (HDMI-1, HDMI-2, DP-1, etc.)
**Why it happens:** Pi 4 has two HDMI outputs, plus virtual outputs
**How to avoid:** Filter to connectors with status "connected" (`cat /sys/class/drm/card*/status`). Pick first connected display.
**Warning signs:** Wrong display's EDID parsed, dimensions don't match

### Pitfall 4: Bash Arithmetic with Floats
**What goes wrong:** Bash doesn't support floating-point math natively
**Why it happens:** `$(( ))` is integer only
**How to avoid:** Use `bc -l` or `awk` for the diagonal calculation: `echo "scale=1; sqrt($w*$w + $h*$h) / 25.4" | bc -l`
**Warning signs:** Truncated/wrong screen size values

### Pitfall 5: ConfigService.value() Reactivity for New Keys
**What goes wrong:** UiMetrics binds to `ConfigService.value("display.screen_size")` but doesn't update when config changes
**Why it happens:** ConfigService emits `configChanged(path, value)` but QML `readonly property var` bindings to function calls don't auto-rebind
**How to avoid:** Read screen size from `DisplayInfo.screenSizeInches` Q_PROPERTY instead (emits proper notify signal). Feed DisplayInfo from config at startup.
**Warning signs:** Changing screen size in YAML and restarting doesn't update scaling

### Pitfall 6: bc Not Installed on Minimal Trixie
**What goes wrong:** `bc` may not be on a fresh RPi OS Trixie install
**Why it happens:** Minimal installs don't include `bc`
**How to avoid:** Use `awk` instead: `awk "BEGIN {printf \"%.1f\", sqrt($w*$w + $h*$h) / 25.4}"` -- awk is in coreutils, always present.
**Warning signs:** "bc: command not found" during install

## Code Examples

### EDID Probe Function (Installer)

```bash
# Probe EDID for physical screen size
# Returns: screen size in inches (e.g., "7.0") or empty string if unavailable
probe_screen_size() {
    local edid_path=""

    # Find first connected display's EDID
    for card_dir in /sys/class/drm/card*-*/; do
        if [[ -f "${card_dir}status" ]]; then
            local status
            status=$(cat "${card_dir}status" 2>/dev/null)
            if [[ "$status" == "connected" ]] && [[ -f "${card_dir}edid" ]] && [[ -s "${card_dir}edid" ]]; then
                edid_path="${card_dir}edid"
                break
            fi
        fi
    done

    if [[ -z "$edid_path" ]]; then
        return
    fi

    # Try detailed timing descriptor first (mm precision)
    # First descriptor at byte 54, physical size at bytes 12-14 within it (bytes 66-68)
    local b66 b67 b68 w_mm h_mm
    b66=$(od -An -tu1 -j66 -N1 "$edid_path" 2>/dev/null | tr -d ' ')
    b67=$(od -An -tu1 -j67 -N1 "$edid_path" 2>/dev/null | tr -d ' ')
    b68=$(od -An -tu1 -j68 -N1 "$edid_path" 2>/dev/null | tr -d ' ')

    if [[ -n "$b66" ]] && [[ -n "$b67" ]] && [[ -n "$b68" ]]; then
        w_mm=$(( ((b68 >> 4) << 8) | b66 ))
        h_mm=$(( ((b68 & 0x0F) << 8) | b67 ))

        if [[ $w_mm -gt 0 ]] && [[ $h_mm -gt 0 ]]; then
            awk "BEGIN {printf \"%.1f\", sqrt($w_mm*$w_mm + $h_mm*$h_mm) / 25.4}"
            return
        fi
    fi

    # Fall back to basic display params (cm precision)
    local w_cm h_cm
    w_cm=$(od -An -tu1 -j21 -N1 "$edid_path" 2>/dev/null | tr -d ' ')
    h_cm=$(od -An -tu1 -j22 -N1 "$edid_path" 2>/dev/null | tr -d ' ')

    if [[ -n "$w_cm" ]] && [[ -n "$h_cm" ]] && [[ $w_cm -gt 0 ]] && [[ $h_cm -gt 0 ]]; then
        local w_mm=$((w_cm * 10))
        local h_mm=$((h_cm * 10))
        awk "BEGIN {printf \"%.1f\", sqrt($w_mm*$w_mm + $h_mm*$h_mm) / 25.4}"
        return
    fi
}
```

**Confidence:** HIGH for the approach. The exact byte offsets within the detailed timing descriptor should be validated against the Pi's actual EDID output during implementation.

### DisplayInfo Extension (C++)

```cpp
// DisplayInfo.hpp -- add to existing class
Q_PROPERTY(qreal screenSizeInches READ screenSizeInches NOTIFY screenSizeChanged)
Q_PROPERTY(int computedDpi READ computedDpi NOTIFY windowSizeChanged)

// DisplayInfo.cpp
qreal DisplayInfo::screenSizeInches() const {
    return screenSizeInches_;
}

void DisplayInfo::setScreenSizeInches(qreal inches) {
    if (inches <= 0) return;
    if (qFuzzyCompare(inches, screenSizeInches_)) return;
    screenSizeInches_ = inches;
    emit screenSizeChanged();
    emit windowSizeChanged();  // triggers DPI recalc in QML
}

int DisplayInfo::computedDpi() const {
    double diagPx = std::hypot(windowWidth_, windowHeight_);
    return static_cast<int>(std::round(diagPx / screenSizeInches_));
}
```

### UiMetrics DPI Replacement (QML)

```javascript
// Replace existing scaleH/scaleV with:
readonly property real _screenSize: DisplayInfo ? DisplayInfo.screenSizeInches : 7.0
readonly property real _diagPx: Math.sqrt(_winW * _winW + _winH * _winH)
readonly property real _dpi: _diagPx / _screenSize
readonly property real _referenceDpi: 160.0

// DPI-based scale replaces resolution-ratio
readonly property real scaleH: _dpi / _referenceDpi
readonly property real scaleV: _dpi / _referenceDpi
```

### Settings Read-Only Display (QML)

```qml
// At top of DisplaySettings.qml content ColumnLayout, before brightness slider:
ReadOnlyField {
    label: "Screen"
    value: {
        var size = DisplayInfo ? DisplayInfo.screenSizeInches : 7.0;
        var dpi = DisplayInfo ? DisplayInfo.computedDpi : 170;
        return size.toFixed(1) + "\" / " + dpi + " PPI";
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `windowWidth / 1024.0` ratio scaling | DPI-based: `actualDPI / 160` | This phase | Correct sizing across different physical screen sizes at same resolution |
| No physical size awareness | EDID-detected screen size stored in config | This phase | Different screens get appropriate element sizes |

## Open Questions

1. **Detailed timing descriptor byte offsets**
   - What we know: Bytes 66-68 relative to EDID start, based on descriptor at offset 54 + 12 bytes in
   - What's unclear: Some displays may have the first descriptor as a "monitor descriptor" (not timing), which has different byte layout. Need to check pixel clock bytes (54-55) are non-zero to confirm it's a timing descriptor.
   - Recommendation: Check pixel clock at bytes 54-55; if zero, it's a monitor descriptor, skip to next descriptor at byte 72. Fallback to basic params if no timing descriptor found.

2. **DRM card naming on Pi 4 with Trixie**
   - What we know: Pi 4 uses vc4 DRM driver, typically `card1-HDMI-A-1` and `card1-HDMI-A-2`
   - What's unclear: Whether Trixie changed card numbering from older Pi OS versions
   - Recommendation: Use glob pattern `card*-*`, filter by `status == connected`, don't hardcode card number.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Qt Test (QTest) |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd build && ctest --output-on-failure -R test_display_info` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| DPI-01 | EDID probe returns valid screen size | manual-only | N/A (requires Pi with display) | N/A |
| DPI-02 | Default screen size is 7.0 when not configured | unit | `ctest -R test_yaml_config -x` | Needs new test case |
| DPI-03 | DisplayInfo computes DPI from screen size + window dims | unit | `ctest -R test_display_info -x` | Needs new test cases |
| DPI-03 | UiMetrics uses DPI-based scale | manual-only | N/A (QML singleton, visual verification) | N/A |
| DPI-04 | screen_size persisted and read from config | unit | `ctest -R test_config_service -x` | Needs new test case |
| DPI-04 | Display settings shows screen info | manual-only | N/A (QML UI verification) | N/A |

### Sampling Rate
- **Per task commit:** `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
- **Per wave merge:** Full suite
- **Phase gate:** Full suite green before verify

### Wave 0 Gaps
- [ ] `tests/test_display_info.cpp` -- add tests for `screenSizeInches`, `computedDpi`, `setScreenSizeInches`
- [ ] `tests/test_yaml_config.cpp` -- add test for `display.screen_size` default value (7.0)
- [ ] `tests/test_config_service.cpp` -- add test for `display.screen_size` read/write via ConfigService

## Sources

### Primary (HIGH confidence)
- EDID byte layout: [EDID Dictionary](http://www.drhdmi.eu/dictionary/edid.html) -- physical dimensions at bytes 21-22 (cm) and detailed timing bytes 66-68 (mm precision)
- EDID on Pi: [AdamsDesk - Read EDID on Raspberry Pi](https://www.adamsdesk.com/posts/learn-to-read-edid-displayid-metadata-on-a-raspberry-pi/) -- sysfs paths confirmed
- Existing codebase: `UiMetrics.qml`, `DisplayInfo.hpp/cpp`, `YamlConfig.cpp`, `install.sh` -- all read directly

### Secondary (MEDIUM confidence)
- [VESA EDID Standard Release A Rev.2](https://glenwing.github.io/docs/VESA-EEDID-A2.pdf) -- formal spec for byte layout
- [Extron EDID Guide](https://www.extron.com/article/uedid) -- EDID structure overview

### Tertiary (LOW confidence)
- Detailed timing descriptor byte 14 bit layout (4+4 upper bits for h/v size) -- verified by multiple sources but should be validated against real Pi EDID output during implementation

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new libraries, all existing patterns
- Architecture: HIGH -- surgical changes to existing well-understood code
- EDID parsing: HIGH for basic params, MEDIUM for detailed timing descriptor byte offsets (validate on real hardware)
- Pitfalls: HIGH -- well-known failure modes documented

**Research date:** 2026-03-08
**Valid until:** 2026-04-08 (stable domain, EDID spec doesn't change)
