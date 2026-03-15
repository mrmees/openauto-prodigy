# Phase 14: Color Audit & M3 Expressive Tokens - Research

**Researched:** 2026-03-15
**Domain:** QML theming, M3 token pairing, color manipulation in Qt/C++
**Confidence:** HIGH

## Summary

This phase is a targeted QML/C++ color audit with four deliverables: (1) fix on-* token mismatches, (2) add NavbarControl active/pressed state model, (3) apply bolder accent colors following "colored islands on calm surfaces," and (4) add a dark-mode comfort guardrail in ThemeService. The scope also includes centralizing popup surface tint math into ThemeService computed properties and producing a capability-based state matrix document.

The codebase is well-positioned for this work. ThemeService already exposes all 34 M3 roles as Q_PROPERTYs. The confirmed mismatches are few and specific (NavbarControl hold-progress uses `tertiary` instead of `primaryContainer`, AAStatusWidget uses `primary` for connected status instead of semantic green, 4 files duplicate inline tint math). QColor natively supports HSL decomposition and reconstruction, making the saturation guardrail straightforward in C++.

**Primary recommendation:** Work in three waves -- (1) ThemeService C++ changes (tint properties + guardrail + semantic tokens), (2) QML audit fixes (NavbarControl state model + token pairing + accent boldness), (3) state matrix documentation.

<user_constraints>

## User Constraints (from CONTEXT.md)

### Locked Decisions
- Expression model: colored islands on calm surfaces -- interactive surfaces get container fills, structural backgrounds stay surfaceContainer* family
- Navbar active state: hold-progress fill uses primaryContainer (was tertiary), active/pressed uses primaryContainer + onPrimaryContainer, rest state unchanged
- Capability-based state matrix: action/selection/gesture/passive categories with token maps per state -- document only, no shared QML helper this phase
- Accent boldness: specific surfaces listed for primaryContainer fill vs neutral, launcher dock and AA overlay stay neutral
- Widget accent rules: one accent family per widget, primary for default, primaryContainer reserved for state changes only
- Selection treatment: settings rows get border treatment (left accent bar/outline), tiles/cards/widgets get full container fill
- Semantic status colors: semantic always wins (green=connected, amber=warning, red=error), tertiary must NOT masquerade as warning
- Night mode comfort guardrail: clamp only accent roles (primary, primaryContainer, secondary, secondaryContainer, tertiary, tertiaryContainer), preserve hue
- Popup surface tint: centralize from 4 files into ThemeService computed properties
- Token pairing audit: fix confirmed mismatches, widget text on neutral cards is correct as-is

### Claude's Discretion
- Exact chroma threshold values for the dark-mode comfort guardrail
- Whether to use HSL saturation clamp or HCT chroma clamp
- Exact computed property name for centralized surface tint
- Animation/transition details for state changes
- Whether warning/degraded semantic colors need new ThemeService Q_PROPERTYs or can use existing error + custom constants

### Deferred Ideas (OUT OF SCOPE)
- Shared QML StateStyle component -- document token map first, extract helper in future phase if controls drift
- Widget state-change container fills (active alert, edit mode) -- define the rule now, implement when widgets gain those states
- Phase 15 (Color Boldness Slider) handles user-adjustable saturation -- separate from the comfort guardrail

</user_constraints>

<phase_requirements>

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CA-01 | All QML surfaces using accent-colored backgrounds have matching on-* foreground tokens | Confirmed mismatches identified in code audit; M3 pairing rules documented; tint centralization pattern defined |
| CA-02 | NavbarControl active/pressed state uses primaryContainer fill with onPrimaryContainer foreground | NavbarControl.qml structure analyzed; state model pattern documented; barBg/barFg alias system understood |
| CA-03 | Widget cards, settings tiles, and interactive controls use bolder accent colors from M3 Expressive palette | Full widget and control audit complete; "colored islands" mapping documented per surface type |

</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt 6.8 (QColor) | 6.8.2 | HSL color manipulation for guardrail | Already in-tree, native HSL support via setHsl/hslSaturation/hslHue |
| ThemeService | in-tree | Central theme token provider | Already exposes 34 M3 roles + success/onSuccess; add tint + guardrail here |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| QColor HSL API | Qt 6.8 | Saturation clamping in guardrail | `QColor::hslSaturationF()`, `QColor::setHslF()` -- float 0.0-1.0 range |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| HSL saturation clamp | HCT chroma (material-color-utilities port) | HCT is perceptually uniform but requires porting C++ library; HSL is sufficient for a comfort guardrail and QColor supports it natively. Out of scope per REQUIREMENTS.md |
| Computed Q_PROPERTY for tint | QML-side color math helper | C++ computed property is cleaner -- single source of truth, reactive via colorsChanged signal, eliminates 4 duplicate QML expressions |

**No new dependencies required.** All work uses existing Qt 6.8 QColor API and in-tree ThemeService.

## Architecture Patterns

### Recommended Change Structure
```
ThemeService.hpp/cpp    -- Add: surfaceTintHigh, surfaceTintHighest, warning, onWarning,
                           guardrail logic in activeColor()
NavbarControl.qml       -- Add: active/pressed state model with primaryContainer fill
AAStatusWidget.qml      -- Fix: connected icon color to ThemeService.success (semantic green)
4 popup/dialog QML      -- Replace inline tint math with ThemeService.surfaceTintHigh/Highest
Various settings QML    -- Apply accent boldness per "colored islands" decisions
docs/                   -- New: state-matrix.md capability-based token map
```

### Pattern 1: ThemeService Computed Tint Property
**What:** C++ computed QColor property that blends surface + primary at 88/12 ratio
**When to use:** All popup/dialog/overlay surfaces that need subtle primary tint
**Example:**
```cpp
// ThemeService.hpp
Q_PROPERTY(QColor surfaceTintHigh READ surfaceTintHigh NOTIFY colorsChanged)
Q_PROPERTY(QColor surfaceTintHighest READ surfaceTintHighest NOTIFY colorsChanged)

// ThemeService.cpp
QColor ThemeService::surfaceTintHigh() const {
    QColor base = activeColor("surface-container-high");
    QColor tint = activeColor("primary");
    return QColor::fromRgbF(
        base.redF() * 0.88 + tint.redF() * 0.12,
        base.greenF() * 0.88 + tint.greenF() * 0.12,
        base.blueF() * 0.88 + tint.blueF() * 0.12,
        base.alphaF());
}
```

### Pattern 2: Night Mode Comfort Guardrail
**What:** Clamp HSL saturation of accent roles when nightMode() is true
**When to use:** Applied transparently inside activeColor() for the 6 accent roles
**Example:**
```cpp
QColor ThemeService::activeColor(const QString& key) const {
    // ... existing lookup ...
    QColor c = /* looked up color */;

    // Night-mode comfort guardrail: reduce saturation on accent roles
    if (nightMode() && isAccentRole(key)) {
        qreal sat = c.hslSaturationF();
        const qreal maxNightSat = 0.55; // Recommendation: 55% HSL saturation cap
        if (sat > maxNightSat) {
            c.setHslF(c.hslHueF(), maxNightSat, c.lightnessF(), c.alphaF());
        }
    }
    return c;
}

bool ThemeService::isAccentRole(const QString& key) const {
    static const QSet<QString> accentRoles = {
        "primary", "primary-container",
        "secondary", "secondary-container",
        "tertiary", "tertiary-container"
    };
    return accentRoles.contains(key);
}
```

### Pattern 3: NavbarControl State Model
**What:** QML state machine for rest/hold-progress/active visual states
**When to use:** NavbarControl already has hold-progress; add active/pressed differentiation
**Example:**
```qml
// NavbarControl.qml -- hold-progress fill change
Rectangle {
    // Progress overlay
    color: ThemeService.primaryContainer  // was: ThemeService.tertiary
    opacity: 0.3
}

// Icon color during active state
MaterialIcon {
    color: root._holdProgress > 0 ? ThemeService.onPrimaryContainer : navbar.barFg
}
```

### Pattern 4: Semantic Status Colors
**What:** Use dedicated semantic tokens instead of theme accents for status indicators
**When to use:** Any UI element showing connection state, warning, or error
**Example:**
```qml
// AAStatusWidget.qml -- connected icon
color: connected ? ThemeService.success : ThemeService.onSurfaceVariant
// NOT: ThemeService.primary (theme accent should not indicate connection status)
```

### Anti-Patterns to Avoid
- **Accent as status indicator:** Using `ThemeService.primary` for "connected" state -- must use semantic green (`ThemeService.success`)
- **Inline tint math in QML:** Duplicating `surfaceContainerHigh.r * 0.88 + primary.r * 0.12` -- use centralized `ThemeService.surfaceTintHigh`
- **Full container fill on dense layouts:** Settings rows should NOT get primaryContainer fill -- use left accent bar or outline per "colored islands" principle
- **Missing on-* pairing:** Any time a container/accent is used as background, the matching on-* token MUST be the foreground color

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Surface tint calculation | Inline QML color math in each popup | `ThemeService.surfaceTintHigh` / `surfaceTintHighest` | 4 duplicate expressions, easy to diverge |
| Night saturation reduction | Per-QML-element opacity hacks | `activeColor()` guardrail in ThemeService | Transparent to all consumers, single threshold to tune |
| Semantic status colors | Ad-hoc QColor constants in QML | `ThemeService.success`/`onSuccess` + new `warning`/`onWarning` | Already have success/onSuccess pattern; extend it |

**Key insight:** The guardrail belongs in `activeColor()` because it applies universally to all consumers (QML bindings, IPC export, etc.) without any QML changes.

## Common Pitfalls

### Pitfall 1: Guardrail Affecting Day Mode
**What goes wrong:** Saturation clamp fires in day mode, dulling vibrant themes
**Why it happens:** Checking `nightMode()` but not accounting for `forceDarkMode_` interactions
**How to avoid:** `nightMode()` already respects `forceDarkMode_` (returns `forceDarkMode_ || nightMode_`). The guardrail check is correct as-is.
**Warning signs:** Day mode colors look washed out after the change

### Pitfall 2: HSL Hue Shift at Low Saturation
**What goes wrong:** Colors visually shift hue when saturation is clamped
**Why it happens:** HSL hue becomes unstable at very low saturation or very high/low lightness
**How to avoid:** Only clamp saturation, never modify hue or lightness. The 55% cap is high enough to avoid the instability zone.
**Warning signs:** Night mode colors look like a different theme entirely

### Pitfall 3: surfaceTintHigh Alpha Mismatch
**What goes wrong:** GestureOverlay uses 0.87 alpha, other popups use 1.0 -- centralizing loses this distinction
**Why it happens:** GestureOverlay is a semi-transparent overlay, while dialogs are opaque
**How to avoid:** `surfaceTintHighest` handles the GestureOverlay base color; the 0.87 alpha is applied by the QML Rectangle's own `opacity` or the alpha channel in `Qt.rgba()` wrapper. The tint property provides the RGB; alpha remains per-use.
**Warning signs:** GestureOverlay becomes fully opaque or popup dialogs become semi-transparent

### Pitfall 4: Navbar barBg/barFg AA Override
**What goes wrong:** Active state colors show during AA projection when navbar should be black/white
**Why it happens:** NavbarControl uses `navbar.barBg`/`navbar.barFg` which already handle AA override
**How to avoid:** The active state fill must respect `navbar.aaActive` -- when AA is active, keep black/white colors, don't apply primaryContainer
**Warning signs:** Colored navbar controls visible during AA video projection

### Pitfall 5: Power Menu pressedTextColor
**What goes wrong:** Power menu ElevatedButton `pressedTextColor` is `navbar.barFg` (line 317) but should be `onPrimaryContainer` when pressed on `primaryContainer` background
**Why it happens:** Original code used same text color for both states
**How to avoid:** Set `pressedTextColor: navbar.aaActive ? "#FFFFFF" : ThemeService.onPrimaryContainer`
**Warning signs:** Low contrast text on pressed power menu buttons in non-AA mode

## Code Examples

### Existing M3 Token Properties (ThemeService.hpp)
```cpp
// Already available -- no changes needed for basic tokens
Q_PROPERTY(QColor primary READ primary NOTIFY colorsChanged)
Q_PROPERTY(QColor onPrimary READ onPrimary NOTIFY colorsChanged)
Q_PROPERTY(QColor primaryContainer READ primaryContainer NOTIFY colorsChanged)
Q_PROPERTY(QColor onPrimaryContainer READ onPrimaryContainer NOTIFY colorsChanged)
// ... all 34 roles ...
Q_PROPERTY(QColor success READ success NOTIFY colorsChanged)  // hardcoded #4CAF50
Q_PROPERTY(QColor onSuccess READ onSuccess NOTIFY colorsChanged)  // hardcoded #FFFFFF
```

### Current Inline Tint Math (to be replaced)
```qml
// PairingDialog.qml line 31-33, NavbarPopup.qml line 83-85, ExitDialog.qml line 21-23
color: Qt.rgba(
    ThemeService.surfaceContainerHigh.r * 0.88 + ThemeService.primary.r * 0.12,
    ThemeService.surfaceContainerHigh.g * 0.88 + ThemeService.primary.g * 0.12,
    ThemeService.surfaceContainerHigh.b * 0.88 + ThemeService.primary.b * 0.12,
    1.0)

// GestureOverlay.qml line 76-78 (uses Highest base + 0.87 alpha)
color: Qt.rgba(
    ThemeService.surfaceContainerHighest.r * 0.88 + ThemeService.primary.r * 0.12,
    ThemeService.surfaceContainerHighest.g * 0.88 + ThemeService.primary.g * 0.12,
    ThemeService.surfaceContainerHighest.b * 0.88 + ThemeService.primary.b * 0.12,
    0.87)
```

### Current NavbarControl Hold Progress (to be fixed)
```qml
// NavbarControl.qml line 49 -- currently tertiary, should be primaryContainer
Rectangle {
    color: ThemeService.tertiary    // FIX: ThemeService.primaryContainer
    opacity: 0.3
}
// NavbarControl.qml line 60 -- icon color stays barFg, no active state differentiation
MaterialIcon {
    color: navbar.barFg             // FIX: add conditional for hold-progress state
}
```

### Current AAStatusWidget Connected Color (to be fixed)
```qml
// AAStatusWidget.qml line 28 -- uses theme primary for connected status
color: connected ? ThemeService.primary : ThemeService.onSurfaceVariant
// FIX: connected ? ThemeService.success : ThemeService.onSurfaceVariant
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Inline color math in QML | Computed properties in ThemeService | This phase | 4 files simplified, single source of truth |
| Tertiary for hold-progress | primaryContainer for hold-progress | This phase | Consistent M3 container pairing |
| Theme accent for status | Semantic colors for status | This phase | Connection/warning/error visually distinct from theme |

**Deprecated/outdated:**
- Inline `surfaceContainerHigh.r * 0.88 + primary.r * 0.12` pattern: replaced by `ThemeService.surfaceTintHigh`

## Open Questions

1. **Night saturation threshold value**
   - What we know: HSL saturation 0.55 is a reasonable starting point for not-too-vivid night colors
   - What's unclear: Exact value needs Pi testing in a dark car environment
   - Recommendation: Start at 0.55, plan for Pi verification. Easy to adjust -- single constant in ThemeService.cpp

2. **Warning/degraded semantic tokens**
   - What we know: `success`/`onSuccess` pattern already exists (hardcoded #4CAF50/#FFFFFF)
   - What's unclear: Whether warning needs a Q_PROPERTY or can be a QML-side constant
   - Recommendation: Add `warning`/`onWarning` as Q_PROPERTYs in ThemeService following the success pattern. Amber #FF9800 is standard M3 warning. This future-proofs for companion-app-provided semantic colors. Also add `connected`/`onConnected` aliases if desired, or reuse `success` for connected state (green for both is standard).

3. **GestureOverlay alpha preservation**
   - What we know: GestureOverlay uses 0.87 alpha on its tinted surface; other popups use 1.0
   - What's unclear: Whether to bake alpha into surfaceTintHighest or keep it in QML
   - Recommendation: Keep alpha in QML. The tint property provides opaque RGB; the QML Rectangle applies `opacity: 0.87` or wraps in `Qt.rgba(surfaceTintHighest.r, ..., 0.87)`. This matches how other QML elements handle variable opacity.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | QTest (Qt 6.8.2) |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cd build && ctest --output-on-failure -R theme` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CA-01 | Token pairing audit (tint properties return correct blended colors) | unit | `ctest -R theme --output-on-failure` | Existing: tests/test_theme_service.cpp (extend) |
| CA-01 | surfaceTintHigh computed correctly | unit | `ctest -R theme --output-on-failure` | Extend existing test |
| CA-02 | NavbarControl state model (QML visual) | manual-only | Visual inspection on Pi | N/A -- QML visual behavior |
| CA-03 | Accent boldness (QML visual) | manual-only | Visual inspection on Pi in day + night mode | N/A -- QML visual behavior |
| CA-01/03 | Night guardrail clamps accent saturation | unit | `ctest -R theme --output-on-failure` | Extend existing test |
| CA-03 | Semantic success/warning tokens exist | unit | `ctest -R theme --output-on-failure` | Extend existing test |

### Sampling Rate
- **Per task commit:** `cd build && ctest -R theme --output-on-failure`
- **Per wave merge:** `cd build && ctest --output-on-failure`
- **Phase gate:** Full suite green + Pi visual verification in day AND night mode in dark environment

### Wave 0 Gaps
- [ ] Extend `tests/test_theme_service.cpp` with tests for: surfaceTintHigh/Highest computation, night guardrail saturation clamping, warning/onWarning tokens
- No new test files needed -- existing test_theme_service.cpp covers ThemeService

## Sources

### Primary (HIGH confidence)
- Direct codebase analysis: ThemeService.hpp/cpp, NavbarControl.qml, Navbar.qml, AAStatusWidget.qml, NowPlayingWidget.qml, ClockWidget.qml, ElevatedButton.qml, SettingsToggle.qml, 4 popup/dialog QML files
- [QColor Class | Qt GUI 6.10.2](https://doc.qt.io/qt-6/qcolor.html) -- HSL API (`hslSaturationF`, `setHslF`, `hslHueF`, `lightnessF`)

### Secondary (MEDIUM confidence)
- M3 token pairing rules (training knowledge, consistent with Material Design 3 spec): primary/onPrimary, primaryContainer/onPrimaryContainer, surface/onSurface pairs

### Tertiary (LOW confidence)
- Night saturation threshold of 0.55 -- reasonable estimate, needs Pi testing to validate

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all changes use existing Qt 6.8 QColor API and in-tree ThemeService
- Architecture: HIGH -- patterns are straightforward extensions of existing code (computed properties, activeColor guardrail)
- Pitfalls: HIGH -- all identified from direct code analysis of the actual codebase
- Night guardrail threshold: LOW -- needs empirical validation on Pi hardware in dark environment

**Research date:** 2026-03-15
**Valid until:** 2026-04-15 (stable -- no external dependencies changing)
