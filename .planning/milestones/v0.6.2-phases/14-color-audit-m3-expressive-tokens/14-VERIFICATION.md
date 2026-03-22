---
phase: 14-color-audit-m3-expressive-tokens
verified: 2026-03-15T22:00:00Z
status: human_needed
score: 11/11 must-haves verified
human_verification:
  - test: "Deploy to Pi and visually confirm day/night mode legibility"
    expected: "Quick-tap navbar shows brief primaryContainer flash with onPrimaryContainer icon; hold-progress fills from bottom with same colors; tiles show onPrimary text on primary background; all Switches show primaryContainer track when checked; night mode is noticeably less saturated but still hue-correct"
    why_human: "Visual contrast and comfort can only be verified on the Pi display in real lighting conditions; automated checks cannot confirm glare or perceptual comfort"
  - test: "Verify REQUIREMENTS.md CA-02 traceability row is updated to Complete"
    expected: "Traceability table row for CA-02 shows 'Complete', matching the implemented code"
    why_human: "Documentation discrepancy — code satisfies CA-02 but the traceability table in REQUIREMENTS.md still reads 'Pending'; requires a human to update the doc or confirm the SUMMARY approved Pi verification as sufficient"
---

# Phase 14: Color Audit & M3 Expressive Tokens — Verification Report

**Phase Goal:** Audit and fix M3 color token usage across all QML controls; add expressive surface tint properties, semantic warning tokens, and night-mode comfort guardrail to ThemeService; produce a capability-based state matrix document.

**Verified:** 2026-03-15T22:00:00Z
**Status:** human_needed — all automated checks pass; Pi visual verification was claimed approved in SUMMARY but cannot be confirmed programmatically
**Re-verification:** No — initial verification

---

## Goal Achievement

### Success Criteria (from ROADMAP.md)

| # | Criterion | Status | Evidence |
|---|-----------|--------|----------|
| 1 | Text on every primary/secondary/tertiary-colored surface is legible in both day and night modes | ? NEEDS HUMAN | Code correctly uses onPrimary/onPrimaryContainer; Pi visual confirm reported approved in SUMMARY |
| 2 | Active/pressed navbar control shows primaryContainer fill with onPrimaryContainer foreground | ✓ VERIFIED | NavbarControl.qml lines 46-50, 53-60, 68, 118, 158 — solid fills + onPrimaryContainer color |
| 3 | Widget cards, settings tiles, interactive controls show bolder accent colors | ✓ VERIFIED | Tile.qml uses primary/primaryContainer; SettingsToggle + inline Switches use Material.accent: primaryContainer |
| 4 | Night mode primary color is visually comfortable — not a glare source | ? NEEDS HUMAN | Guardrail implemented and tested (kMaxNightSaturation=0.55); subjective comfort requires Pi verify |

### Observable Truths (from Plan 01 must_haves)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | surfaceTintHigh returns 88/12 blend of surfaceContainerHigh and primary | ✓ VERIFIED | ThemeService.cpp lines 629-638: exact 88/12 QColor::fromRgbF blend |
| 2 | surfaceTintHighest returns 88/12 blend of surfaceContainerHighest and primary | ✓ VERIFIED | ThemeService.cpp lines 640-649: same pattern with surfaceContainerHighest |
| 3 | Night mode clamps HSL saturation of accent roles to comfortable maximum | ✓ VERIFIED | ThemeService.cpp lines 691-699: kMaxNightSaturation=0.55, applied in activeColor() |
| 4 | warning/onWarning semantic tokens are available as Q_PROPERTYs | ✓ VERIFIED | ThemeService.hpp lines 90-91; ThemeService.cpp lines 651-658: #FF9800 / #FFFFFF |
| 5 | Guardrail does NOT affect day mode colors | ✓ VERIFIED | activeColor() guard: `if (night && isAccentRole(key))` — day mode skipped; test dayModeNoGuardrail passes |
| 6 | Guardrail does NOT affect non-accent roles | ✓ VERIFIED | isAccentRole() returns false for surface/outline roles; test nightGuardrailSkipsNonAccent passes |

### Observable Truths (from Plan 02 must_haves)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | NavbarControl quick-tap shows distinctly colored pressed fill (primaryContainer) | ✓ VERIFIED | NavbarControl.qml lines 46-50: `color: ThemeService.primaryContainer`, `visible: !navbar.aaActive && root._pressed && root._holdProgress === 0` |
| 2 | NavbarControl hold-progress fill uses primaryContainer (not tertiary) | ✓ VERIFIED | NavbarControl.qml lines 53-60: `color: ThemeService.primaryContainer` — was tertiary in old code |
| 3 | NavbarControl icon color changes to onPrimaryContainer during press and hold | ✓ VERIFIED | NavbarControl.qml line 68: `root._pressed || root._holdProgress > 0 ? ThemeService.onPrimaryContainer : navbar.barFg` |
| 4 | AA-active mode still forces black/white navbar colors | ✓ VERIFIED | NavbarControl.qml lines 49, 68: `navbar.aaActive` guards prevent pressed fills and force `navbar.barFg` |
| 5 | AAStatusWidget connected icon uses ThemeService.success not primary | ✓ VERIFIED | AAStatusWidget.qml line 28: `color: connected ? ThemeService.success : ThemeService.onSurfaceVariant` |
| 6 | All 4 popup/dialog QML files use ThemeService.surfaceTintHigh or surfaceTintHighest | ✓ VERIFIED | PairingDialog.qml line 30, ExitDialog.qml line 20, NavbarPopup.qml line 82, GestureOverlay.qml line 75 |
| 7 | Power menu pressed buttons have onPrimaryContainer text on primaryContainer background | ✓ VERIFIED | Navbar.qml line 315-317: `pressedColor: ThemeService.primaryContainer`, `pressedTextColor: ThemeService.onPrimaryContainer` (with aaActive guard) |
| 8 | Tile content uses onPrimary (rest) / onPrimaryContainer (pressed) | ✓ VERIFIED | Tile.qml lines 68, 75: `tile._isPressed ? ThemeService.onPrimaryContainer : ThemeService.onPrimary` |
| 9 | Every checked Switch uses primaryContainer active styling | ✓ VERIFIED | SettingsToggle.qml line 47, ThemeSettings.qml line 55, ConnectionSettings.qml line 46, DebugSettings.qml line 56: all have `Material.accent: ThemeService.primaryContainer` |
| 10 | CompanionSettings degraded status uses ThemeService.warning (not tertiary) | ✓ VERIFIED | CompanionSettings.qml lines 201, 218: `ThemeService.warning` — was `ThemeService.tertiary` |
| 11 | State matrix document maps all control types to token assignments | ✓ VERIFIED | docs/state-matrix.md: 159 lines, covers NavbarControl, Tile, SettingsToggle, ElevatedButton, popup surfaces, semantic status tokens |

**Score:** 11/11 automated truths verified

---

## Required Artifacts

| Artifact | Status | Details |
|----------|--------|---------|
| `src/core/services/ThemeService.hpp` | ✓ VERIFIED | Lines 88-91: 4 new Q_PROPERTYs (surfaceTintHigh, surfaceTintHighest, warning, onWarning); line 252: isAccentRole private helper declared |
| `src/core/services/ThemeService.cpp` | ✓ VERIFIED | 88/12 blend in surfaceTintHigh/surfaceTintHighest; hardcoded warning/onWarning; isAccentRole with 6-role QSet; guardrail in activeColor() |
| `tests/test_theme_service.cpp` | ✓ VERIFIED | 7 new test slots: surfaceTintHighBlend, surfaceTintHighestBlend, warningTokens, nightGuardrailClampsAccent, nightGuardrailPreservesHueAndLightness, nightGuardrailSkipsNonAccent, dayModeNoGuardrail — all pass |
| `qml/components/NavbarControl.qml` | ✓ VERIFIED | _pressed property, solid pressed-state Rectangle, solid hold-progress Rectangle, onPrimaryContainer icon/text colors, aaActive guards |
| `qml/components/Navbar.qml` | ✓ VERIFIED | pressedTextColor uses onPrimaryContainer with aaActive guard |
| `qml/components/PairingDialog.qml` | ✓ VERIFIED | `color: ThemeService.surfaceTintHigh` — no inline math |
| `qml/components/NavbarPopup.qml` | ✓ VERIFIED | `color: navbar.aaActive ? "#1A1A1A" : ThemeService.surfaceTintHigh` |
| `qml/components/ExitDialog.qml` | ✓ VERIFIED | `color: ThemeService.surfaceTintHigh` — no inline math |
| `qml/components/GestureOverlay.qml` | ✓ VERIFIED | `Qt.rgba(ThemeService.surfaceTintHighest.r, ..., 0.87)` — alpha in QML, blend centralized |
| `qml/widgets/AAStatusWidget.qml` | ✓ VERIFIED | `color: connected ? ThemeService.success : ThemeService.onSurfaceVariant` |
| `qml/controls/Tile.qml` | ✓ VERIFIED | onPrimary (rest) / onPrimaryContainer (pressed) for both icon and text |
| `qml/controls/SettingsToggle.qml` | ✓ VERIFIED | `Material.accent: ThemeService.primaryContainer` on Switch |
| `qml/applications/settings/CompanionSettings.qml` | ✓ VERIFIED | Both degraded status returns replaced with `ThemeService.warning` |
| `qml/applications/settings/ThemeSettings.qml` | ✓ VERIFIED | Inline Switch has `Material.accent: ThemeService.primaryContainer` |
| `qml/applications/settings/ConnectionSettings.qml` | ✓ VERIFIED | pairableSwitch has `Material.accent: ThemeService.primaryContainer` |
| `qml/applications/settings/DebugSettings.qml` | ✓ VERIFIED | codecSwitch has `Material.accent: ThemeService.primaryContainer` |
| `docs/state-matrix.md` | ✓ VERIFIED | 159 lines; covers action/selection/gesture/passive control categories; NavbarControl, Tile, SettingsToggle, popup surfaces, semantic token rules, "colored islands" principle |

---

## Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `ThemeService.cpp::activeColor()` | guardrail logic | `isAccentRole` check + `hslSaturationF` clamp | ✓ WIRED | Lines 691-699: guard applied before return, after both primary and day-fallback lookup |
| `ThemeService.hpp` | QML consumers | `Q_PROPERTY` declarations with `colorsChanged NOTIFY` | ✓ WIRED | Lines 88-91: all 4 new properties declared with NOTIFY |
| `NavbarControl.qml` | `ThemeService.primaryContainer` | QML property binding for pressed AND hold-progress | ✓ WIRED | Lines 48, 58: direct ThemeService.primaryContainer bindings |
| `PairingDialog.qml` | `ThemeService.surfaceTintHigh` | QML property binding replacing inline math | ✓ WIRED | Line 30: `color: ThemeService.surfaceTintHigh` |
| `AAStatusWidget.qml` | `ThemeService.success` | QML property binding for semantic status | ✓ WIRED | Line 28: `color: connected ? ThemeService.success : ...` |
| `SettingsToggle.qml` | `ThemeService.primaryContainer` | `Material.accent` on Switch | ✓ WIRED | Line 47: `Material.accent: ThemeService.primaryContainer` |
| `CompanionSettings.qml` | `ThemeService.warning` | QML property binding replacing tertiary | ✓ WIRED | Lines 201, 218: both degraded status returns use ThemeService.warning |

---

## Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| CA-01 | 14-01, 14-02 | All accent-background surfaces have matching on-* foreground tokens | ✓ SATISFIED | Tile (onPrimary/onPrimaryContainer), Navbar power menu (onPrimaryContainer), NavbarControl (onPrimaryContainer), popup tints centralized to ThemeService |
| CA-02 | 14-02 | NavbarControl active/pressed state uses primaryContainer fill with onPrimaryContainer foreground | ✓ SATISFIED | NavbarControl.qml fully implements pressed + hold-progress fills with correct token pairing; REQUIREMENTS.md traceability table still shows "Pending" — documentation discrepancy, not an implementation gap |
| CA-03 | 14-01, 14-02 | Interactive controls use bolder accent colors from M3 Expressive palette | ✓ SATISFIED | SettingsToggle + 3 inline Switches use Material.accent: primaryContainer; Tile uses primary/primaryContainer; warning/success semantic tokens available |

**Note on CA-02 traceability:** The REQUIREMENTS.md traceability table (line 57) reads `CA-02 | Phase 14 | Pending`. The implementation fully satisfies CA-02. This is a documentation-only gap — the traceability table was not updated when the plan was completed. The code is correct; the doc needs a one-line fix.

---

## Anti-Patterns Found

None. No TODO/FIXME/placeholder comments, no stub implementations, no empty handlers in any of the 17 modified files.

The one GestureOverlay.qml instance of `Qt.rgba()` is correct and intentional — it applies an alpha value (0.87) to the opaque centralized tint color. The tint computation itself is in ThemeService, satisfying the "no inline math" requirement.

---

## Test Suite

**86/86 tests pass** including 7 new theme tests added in this phase:
- `surfaceTintHighBlend` — verifies 88/12 RGB blend within tolerance
- `surfaceTintHighestBlend` — verifies 88/12 RGB blend for highest variant
- `warningTokens` — verifies #FF9800 / #FFFFFF
- `nightGuardrailClampsAccent` — verifies sat clamped to <= 0.56 in night mode
- `nightGuardrailPreservesHueAndLightness` — verifies hue and lightness unchanged
- `nightGuardrailSkipsNonAccent` — verifies surface tokens not clamped
- `dayModeNoGuardrail` — verifies accent roles not clamped in day mode

---

## Human Verification Required

### 1. Pi Day/Night Visual Verification

**Test:** Deploy to Pi via cross-build + rsync. In day mode: quick-tap a navbar control (should flash primaryContainer), hold a navbar control (should fill bottom-up), check launcher tiles (onPrimary text on primary bg), open settings and toggle any Switch (track should be primaryContainer color when on). In night mode: observe that accent colors are less saturated but still recognizably the same hue as day mode, and are comfortable to look at without being washed out.

**Expected:** All interactive surfaces show correct color pairings; night mode accent colors are noticeably calmer than day mode without appearing desaturated; AA mode shows black/white regardless of theme.

**Why human:** Visual contrast, comfort, and legibility can only be verified on the actual Pi display under real viewing conditions. Perceptual "comfortable at arm's length in a dark environment" (ROADMAP success criterion 4) cannot be asserted programmatically.

**Note:** The 14-02 SUMMARY states "Task 3: Pi visual verification — checkpoint approved (no commit)". This was a human gate. If that approval was genuine, this item is already satisfied.

### 2. REQUIREMENTS.md CA-02 Traceability Fix

**Test:** Open `.planning/REQUIREMENTS.md` line 57 and change `Pending` to `Complete` for CA-02.

**Expected:** All three CA requirements show Complete in the traceability table.

**Why human:** Minor doc update; the code is correct and satisfies CA-02. Human confirmation needed before automated tools downstream misread CA-02 as incomplete.

---

## Gaps Summary

No implementation gaps. All 11 must-have truths verified in code. All 17 artifacts exist, are substantive, and are wired to their consumers. The full test suite passes (86/86).

Two items require human attention:
1. Pi visual confirmation of legibility/comfort (claimed approved in SUMMARY but cannot be confirmed here)
2. REQUIREMENTS.md traceability table has CA-02 still marked "Pending" — one-line doc fix needed

---

*Verified: 2026-03-15T22:00:00Z*
*Verifier: Claude (gsd-verifier)*
