# Phase 4: Tech Debt Cleanup - Research

**Researched:** 2026-03-03
**Domain:** Qt/QML ThemeService property additions, dead-code removal, REQUIREMENTS.md housekeeping
**Confidence:** HIGH

## Summary

Phase 4 is a targeted cleanup of 5 specific items identified in the v0.4.3 milestone audit. All items are localized and low-risk: one missing ThemeService property, one QML divider color hack, one dead QML property, and a documentation check. No architecture changes, no new features, no config migrations.

The highest-value fix is adding `ThemeService.highlightFontColor` — NavStrip active icons currently resolve to `undefined` because the property doesn't exist in C++ or YAML. The other items are cosmetic dead code that adds noise without runtime impact. The REQUIREMENTS.md traceability table already has no "Pending" entries as of audit completion, so the documentation work is a verify-and-confirm task rather than a fix.

**Primary recommendation:** Do everything in a single plan (04-01). These are all 1-5 line changes. Splitting into multiple plans would add overhead without benefit.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| ICON-03 | NavStrip buttons use consistent icon sizing and press feedback | Fix: add `highlightFontColor` so active-state icon color binding resolves correctly |
| NAV-01 | NavStrip buttons have consistent automotive-minimal styling with press feedback | Same fix as ICON-03 — both requirements share the same broken binding in NavStrip.qml |
</phase_requirements>

## Standard Stack

This phase is pure Qt/QML/YAML — no new libraries needed. Everything uses the existing project stack.

### Core
| Component | Version | Purpose | Why Standard |
|-----------|---------|---------|--------------|
| ThemeService C++ | in-tree | Color property host for QML | Existing pattern — all colors go here |
| Theme YAML files | in-tree | Color value storage per theme | 4 themes: default, ember, ocean, amoled |
| Q_PROPERTY macro | Qt 6.x | Expose C++ properties to QML | Standard Qt QML binding mechanism |

### No New Dependencies
All fixes are edits to existing files. No packages to install.

## Architecture Patterns

### Pattern 1: Adding a ThemeService Color Property

This is the highest-priority fix. The pattern is fully established — `pressedColor` and `dividerColor` were added via this exact pattern in Phase 1.

**What:** Add `highlightFontColor` as a Q_PROPERTY backed by a YAML key `highlight_font`.

**Touch points (in order):**
1. `src/core/services/ThemeService.hpp` — add Q_PROPERTY declaration + accessor method signature
2. `src/core/services/ThemeService.cpp` — implement the accessor (delegates to `activeColor("highlight_font")` with a sensible fallback)
3. `config/themes/default/theme.yaml` — add `highlight_font` key to day + night sections
4. `config/themes/ember/theme.yaml` — same
5. `config/themes/ocean/theme.yaml` — same
6. `config/themes/amoled/theme.yaml` — same
7. `tests/data/themes/default/theme.yaml` — add key so tests stay consistent

**Color value rationale:** The active NavStrip button has `ThemeService.highlightColor` as its background. The icon on top needs contrast. The correct semantic is "text/icon color on top of a highlight-colored background." For all 4 themes, `ThemeService.backgroundColor` is a reasonable choice — it's the dark canvas color, which contrasts with the bright highlight. This matches the existing pattern in `SegmentedButton.qml` where the selected segment label uses `ThemeService.backgroundColor` (line 107). A dedicated YAML key gives per-theme control while defaulting to `backgroundColor` if missing.

**Fallback strategy (ThemeService.cpp):**
```cpp
QColor ThemeService::highlightFontColor() const
{
    QColor c = activeColor("highlight_font");
    if (c == QColor(Qt::transparent))
        return backgroundColor();  // Sensible default: dark canvas on highlight bg
    return c;
}
```

**Example Q_PROPERTY declaration (ThemeService.hpp):**
```cpp
Q_PROPERTY(QColor highlightFontColor READ highlightFontColor NOTIFY colorsChanged)
```

**Example accessor in ThemeService.hpp:**
```cpp
QColor highlightFontColor() const;
```

**Example YAML values** (using backgroundColor values from each theme):

| Theme | day.background | Suggested highlight_font |
|-------|---------------|--------------------------|
| default | `#1a1a2e` | `#1a1a2e` |
| ember | `#1e1712` | `#1e1712` |
| ocean | `#1b2838` | `#1b2838` |
| amoled | `#000000` | `#000000` |

Night mode values mirror the night background for each theme.

### Pattern 2: Fixing SegmentedButton Divider

**What:** The inter-segment divider in `SegmentedButton.qml` (line 98) uses `ThemeService.descriptionFontColor` + `opacity: 0.3` instead of `ThemeService.dividerColor`.

**Current code (SegmentedButton.qml line 97-100):**
```qml
color: ThemeService.descriptionFontColor
opacity: 0.3
```

**Correct code:**
```qml
color: ThemeService.dividerColor
// opacity: 0.3 — remove, dividerColor already has alpha baked into YAML (#ffffff26 = ~15%)
```

**Why remove the opacity:** `ThemeService.dividerColor` returns colors like `#ffffff26` (default) which already have alpha. Stacking `opacity: 0.3` on top would make the divider nearly invisible. The `dividerColor` pattern is self-contained.

**Note on scope:** The audit also identifies `ConnectivitySettings.qml` and `ConnectionSettings.qml` using the same `descriptionFontColor + opacity: 0.15` hack for horizontal divider lines (4 instances total). These are in-list section separators — same semantic problem, same fix. The planner should include these in scope. They were not called out in the audit's primary debt list but follow the same anti-pattern and are trivial to fix at the same time.

### Pattern 3: Removing Dead QML Property

**What:** `Tile.qml` line 9 declares `property string tileSubtitle: ""`. No QML file anywhere in the codebase reads or sets this property (confirmed by grep). It was left over when SET-02 (tile subtitles) was descoped.

**Fix:** Delete line 9 from `Tile.qml`. No other files need changing — no callers exist.

**Risk:** Zero. The property is never set or read outside Tile.qml itself.

### Pattern 4: REQUIREMENTS.md Traceability Review

**What the audit said:** Traceability table shows VIS-03 and ICON-01 as "Pending" (stale).

**What the code shows:** The REQUIREMENTS.md traceability table already has both as "Complete":
- Line 73: `| VIS-03 | Phase 1 | Complete |`
- Line 77: `| ICON-01 | Phase 1 | Complete |`

**Status:** The traceability table appears to have been corrected already. The plan should verify this, confirm no entries remain as "Pending", and mark the task complete if nothing needs changing. This is a verify-and-close task, not a fix.

### Pattern 5: pressedColor Q_PROPERTY (Decision Required)

The audit flags `pressedColor` as an "orphaned export" — it's defined in ThemeService and all theme YAMLs but never consumed in QML. The audit explicitly frames this as "a cleanup decision."

**Two options:**

**Option A — Remove pressedColor:** Delete Q_PROPERTY from ThemeService.hpp, remove accessor from .cpp, remove `pressed` keys from all theme YAMLs. Cleaner, but requires updating 4 production theme YAMLs + 1 test YAML + ThemeService.hpp/.cpp. Also means if QML ever wants press color overlays, it'll need to be re-added.

**Option B — Keep pressedColor:** Leave the infrastructure in place. It's already in all YAMLs and C++. The audit says "VIS-05 is satisfied by definition — the properties exist." Dead export, but harmless.

**Recommendation:** Keep it (Option B). VIS-05 explicitly says "ThemeService provides dividerColor and pressedColor properties." Removing a property that satisfies a stated requirement is perverse, and the cost of keeping it is exactly zero lines of runtime code. Document the rationale and close the item.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Per-theme highlight text color | Custom logic in NavStrip.qml | ThemeService.highlightFontColor Q_PROPERTY | Keeps color control in ThemeService, consistent with every other color in the codebase |
| Alpha-premultiplied divider color | `descriptionFontColor + opacity:` | `ThemeService.dividerColor` | dividerColor already has alpha in the YAML — stacking opacity is a double-dip |

## Common Pitfalls

### Pitfall 1: Double-Alpha on Divider Color
**What goes wrong:** Applying `opacity: 0.3` to `ThemeService.dividerColor` makes dividers nearly invisible. The YAML value `#ffffff26` is already ~15% opacity.
**How to avoid:** When switching from `descriptionFontColor + opacity:X` to `dividerColor`, remove the explicit `opacity` line. The alpha is baked into the color value.
**Warning signs:** Invisible dividers in the SegmentedButton after the fix.

### Pitfall 2: Forgetting the Test Theme YAML
**What goes wrong:** Adding `highlight_font` key to all 4 production theme YAMLs but forgetting `tests/data/themes/default/theme.yaml`. Tests may fail or test incorrect fallback behavior.
**How to avoid:** The test theme YAML at `tests/data/themes/default/theme.yaml` is a separate file from `config/themes/default/theme.yaml`. Both need the new key.
**Warning signs:** Test failures in `test_theme_service.cpp` if a test checks the new property.

### Pitfall 3: Fallback Causes Recursive Call
**What goes wrong:** If `highlightFontColor()` fallback calls `backgroundColor()` and `backgroundColor()` fallback also calls `highlightFontColor()`, infinite recursion.
**How to avoid:** The fallback should call `activeColor("background")` directly, not `backgroundColor()` — or just return a hardcoded safe default like `QColor(Qt::black)`. In practice `backgroundColor()` has no fallback chain, so `return backgroundColor()` is safe. But `return activeColor("background")` is cleaner and explicit.

### Pitfall 4: IThemeService Interface
**What goes wrong:** Adding `highlightFontColor()` to ThemeService but not to `IThemeService`. If any code accesses the service via the interface pointer, the method won't be visible.
**How to avoid:** Check whether `highlightFontColor` needs to be on `IThemeService`. Looking at the interface — it exposes `color(name)` as a generic accessor. QML gets direct access to `ThemeService` (not `IThemeService`), so Q_PROPERTYs only need to be on `ThemeService`. Adding to `IThemeService` is optional unless C++ code calls it via the interface. For this phase: no change to `IThemeService` needed.

## Code Examples

### Adding highlightFontColor to ThemeService.hpp
```cpp
// Source: existing pressedColor pattern (ThemeService.hpp lines 46, 118)
Q_PROPERTY(QColor highlightFontColor READ highlightFontColor NOTIFY colorsChanged)

// In the public accessor section:
QColor highlightFontColor() const;
```

### Adding highlightFontColor to ThemeService.cpp
```cpp
// Source: existing dividerColor/pressedColor pattern (ThemeService.cpp lines 225-238)
QColor ThemeService::highlightFontColor() const
{
    QColor c = activeColor("highlight_font");
    if (c == QColor(Qt::transparent))
        return activeColor("background");  // Dark canvas contrasts with highlight bg
    return c;
}
```

### Adding highlight_font to theme YAML (default theme example)
```yaml
# In day: section
  highlight_font: "#1a1a2e"  # Same as background — dark on bright highlight

# In night: section
  highlight_font: "#0a0a14"  # Same as night background
```

### Fixing SegmentedButton divider
```qml
// Before (SegmentedButton.qml line 97-100):
color: ThemeService.descriptionFontColor
opacity: 0.3

// After:
color: ThemeService.dividerColor
// opacity line removed — alpha is baked into dividerColor
```

### Removing dead Tile property
```qml
// Remove from Tile.qml:
property string tileSubtitle: ""   // DELETE THIS LINE
```

## State of the Art

| Old Approach | Current Approach | Context |
|--------------|------------------|---------|
| `descriptionFontColor + opacity:` for dividers | `ThemeService.dividerColor` | Phase 1 added dividerColor specifically to replace this pattern |
| Hardcoded/undefined active icon colors | ThemeService Q_PROPERTY | Phase 3 added icon color to NavStrip but referenced a property that didn't exist yet |

## Open Questions

1. **Should pressedColor be removed?**
   - What we know: It's defined everywhere but used nowhere in QML. VIS-05 says ThemeService "provides" pressedColor.
   - What's unclear: Whether "provides" means "has the property" or "uses the property." The spec says provides — not uses.
   - Recommendation: Keep it. No action required. Document decision in STATE.md.

2. **Should ConnectivitySettings/ConnectionSettings divider hacks be fixed?**
   - What we know: 4 lines across 2 files use `descriptionFontColor + opacity: 0.15` for horizontal dividers. Same anti-pattern as SegmentedButton.
   - What's unclear: Whether these are in scope for this phase (not mentioned in audit's primary debt list).
   - Recommendation: Include them. Trivial fix, same pattern, completes the cleanup properly. Planner should add to 04-01.

## Sources

### Primary (HIGH confidence)
- Direct code inspection: `qml/components/NavStrip.qml` — confirmed `highlightFontColor` referenced at lines 39, 72, 186
- Direct code inspection: `src/core/services/ThemeService.hpp` — confirmed property does NOT exist
- Direct code inspection: `src/core/services/ThemeService.cpp` — confirmed `pressedColor`/`dividerColor` pattern for implementation reference
- Direct code inspection: `qml/controls/SegmentedButton.qml` line 98 — confirmed `descriptionFontColor + opacity: 0.3` hack
- Direct code inspection: `qml/controls/Tile.qml` line 9 — confirmed `tileSubtitle` declaration with zero external references
- Direct code inspection: `.planning/REQUIREMENTS.md` lines 73, 77 — confirmed no "Pending" entries remain in traceability table
- Direct code inspection: `config/themes/*/theme.yaml` (all 4) — confirmed `divider` and `pressed` keys exist, `highlight_font` does not
- Direct code inspection: `tests/data/themes/default/theme.yaml` — confirmed test theme needs `highlight_font` key too

## Metadata

**Confidence breakdown:**
- Problem identification: HIGH — every item verified in source files
- Fix approach: HIGH — follows established patterns already in codebase
- Scope: HIGH — all 5 audit items examined, 2 extended items identified

**Research date:** 2026-03-03
**Valid until:** Indefinite — this is a closed system (no external dependencies)
