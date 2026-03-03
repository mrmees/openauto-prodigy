# Phase 3: QML Hardcoded Island Audit - Research

**Researched:** 2026-03-03
**Domain:** QML tokenization / design token adoption
**Confidence:** HIGH

## Summary

This phase is a mechanical tokenization pass across ~20 QML files, replacing ~175+ hardcoded pixel values with UiMetrics tokens. The existing UiMetrics singleton already provides the scaling infrastructure (`_tok()` overrides, `scale`/`_fontScaleTotal` multipliers, pixel floors). The work is adding ~6-8 new tokens and systematically rewriting every hardcoded literal.

The codebase audit reveals five categories of hardcoded values: font sizes (31 instances), icon sizes (22 instances), component dimensions (45+ instances including buttons, album art, progress bars), spacing/margins (20+ instances), and radius values (12+ instances). Many map directly to existing tokens; others need new tokens or simple `Math.round(N * UiMetrics.scale)` expressions.

**Primary recommendation:** Add new tokens upfront (Plan 1), then sweep files in batched plans by UI layer. Each plan ends with a grep verification confirming zero remaining hardcoded literals in the touched files.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Token inventory strategy:** Minimal new tokens (~6-8). Reuse existing tokens where close enough. New tokens follow Phase 1 pattern (overridable via `ui.tokens.*`, registered in YamlConfig initDefaults()). All new tokens added upfront in Plan 1 before any file edits.
- **Audit batching (3 plans by UI layer):**
  - Plan 1: New UiMetrics tokens + shell chrome (TopBar, BottomBar/NavStrip, Shell)
  - Plan 2: Plugin views (PhoneView, BtAudioView, GestureOverlay, IncomingCallOverlay)
  - Plan 3: Controls & dialogs (Tile, PairingDialog, NormalText, SpecialText, HomeMenu, remaining files)
  - Final plan includes grep-based verification confirming zero hardcoded pixel literals remain
- **Proportional scaling with safety floors:** Album art, call buttons, overlay buttons, icons all scale proportionally. Call/overlay buttons have touch-target floors (never below touchMin). GestureOverlay button height never drops below touchMin (44px).

### Claude's Discretion
- Button token strategy: one generic touchMin-based approach vs separate touchMin + touchLarge tokens
- Divider height: always 1px or scale with resolution
- Radius strategy: standardize to a 3-4 tier design scale vs tokenize existing values (8/9/11/12 differences don't matter at car distance)
- Spacing standardization: hybrid approach -- fix obvious oddities (87px), keep reasonable values, snap to 4px grid where sensible
- Whether pad tokens cover spacing needs or separate concept warranted
- Status indicators and progress bar heights: dedicated tokens vs reuse existing small spacing tokens
- Album art sizing approach: proportional to content area vs fixed token with UiMetrics scaling
- Call button size floor value (e.g., 56px minimum)
- Whether settings sub-views are included in full or deprioritized
- Exact list of new tokens beyond the categories identified

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| AUDIT-01 | NormalText and SpecialText use UiMetrics font tokens instead of hardcoded pixelSize | Both files have `font.pixelSize: 16` -- map to `UiMetrics.fontBody` (base 20) or `UiMetrics.fontSmall` (base 16). fontSmall is the exact match. |
| AUDIT-02 | TopBar and NavStrip margins/spacing/radius use UiMetrics tokens | TopBar: `anchors.*Margin: 16`, `spacing: 12`. NavStrip: `anchors.*Margin: 12`, `spacing: 8`. Map to existing `gap`/`marginRow`/`spacing` tokens. |
| AUDIT-03 | Sidebar icon sizes, font sizes, thumb dimensions, and margins use UiMetrics tokens | Sidebar has ~20 hardcoded values: icons (20/24/28/32), fonts (12), dims (6/18/56), margins (6/8). Map icons to `iconSmall`/`iconSize`, knob to `knobSizeSmall`, bar to `trackThick`. |
| AUDIT-04 | GestureOverlay font sizes, spacing, and dimensions use UiMetrics tokens | ~25 hardcoded values. Fonts (14/16) map to fontTiny/fontSmall. Button dims (100x36) need touchMin floor. Icons (18/22) map to iconSmall/iconSize expressions. |
| AUDIT-05 | PhoneView font sizes, button dimensions, and spacing use UiMetrics tokens | ~20 hardcoded values. Call buttons (80x80) need new token with touchMin floor. Status dot (10x10) needs small indicator approach. Numpad font (24) maps to fontTitle. |
| AUDIT-06 | IncomingCallOverlay font sizes, spacing, and button dimensions use UiMetrics tokens | ~12 hardcoded values. Call buttons (72x72) need touchMin floor. Fonts (18/28/16) map to fontBody/fontHeading/fontSmall. |
| AUDIT-07 | BtAudioView font sizes, album art dimensions, and spacing use UiMetrics tokens | ~20 hardcoded values. Album art (200x200) needs new token. Playback buttons (56/72) need touchMin floor. Progress bar (height 4) needs thin track approach. |
| AUDIT-08 | HomeMenu font sizes use UiMetrics tokens | Single `font.pixelSize: 28` maps to `UiMetrics.fontHeading`. |
| AUDIT-09 | Tile and PairingDialog radius and dimensions use UiMetrics tokens | Tile: `radius: 8`, `spacing: 8` -- already partially tokenized. PairingDialog: `radius: 12`, button `width: 140`, `radius: 8` need token mapping. |
| AUDIT-10 | Zero hardcoded pixel values remain in QML files (excluding intentional dev debug overlays) | Grep verification pattern covers all categories. Debug overlay in AndroidAutoMenu.qml (touch crosshairs) is the only exclusion. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| UiMetrics.qml | Singleton (in-tree) | Centralized design tokens with config-driven overrides | Already established in Phase 1/2; all scaling infrastructure exists |
| YamlConfig.cpp | In-tree | Token override defaults registration | `initDefaults()` pattern established for all config keys |

No external libraries needed. This phase uses only existing project infrastructure.

## Architecture Patterns

### Existing Token Pattern (established in Phase 1/2)

All UiMetrics tokens follow one of three patterns:

**Pattern A: Overridable layout token (most common for new tokens)**
```qml
readonly property int tokenName: {
    var o = _tok("tokenName");
    return isNaN(o) ? Math.round(BASE * scale) : o;
}
```

**Pattern B: Overridable font token (with pixel floor)**
```qml
readonly property int fontName: {
    var o = _tok("fontName");
    return isNaN(o) ? Math.max(Math.round(BASE * _fontScaleTotal), _floor("fontName", DEFAULT_FLOOR)) : o;
}
```

**Pattern C: Non-overridable scaled value**
```qml
readonly property int tokenName: Math.round(BASE * scale)
```

**When to use each:**
- Pattern A: Dimensions users might want to override (button sizes, album art, radii)
- Pattern B: Font sizes only (need floors for automotive legibility)
- Pattern C: Small internal dimensions (dividers, spacing sub-values) that don't need user override

### Token Mapping Strategy

Based on the codebase audit, here is the recommended mapping from hardcoded values to tokens:

**Fonts (31 instances) -- all map to existing tokens:**

| Hardcoded Value | UiMetrics Token | Base Value | Used In |
|-----------------|-----------------|------------|---------|
| `font.pixelSize: 10` | `fontTiny` | 14 | AndroidAutoMenu debug text (EXCLUDE -- debug overlay) |
| `font.pixelSize: 12` | `fontTiny` | 14 | Sidebar %, BtAudio time, PhoneView status |
| `font.pixelSize: 14` | `fontSmall` | 16 | GestureOverlay labels, BtAudio album/status, PhoneView device, NotificationArea |
| `font.pixelSize: 16` | `fontSmall` | 16 | NormalText, SpecialText, GestureOverlay title, BtAudio artist, IncomingCall number |
| `font.pixelSize: 18` | `fontBody` | 20 | IncomingCall subtitle, PhoneView in-call display |
| `font.pixelSize: 20` | `fontBody` | 20 | PhoneView "Call" button text |
| `font.pixelSize: 22` | `fontTitle` | 22 | BtAudio track title |
| `font.pixelSize: 24` | `fontTitle` | 22 | PhoneView numpad digits |
| `font.pixelSize: 28` | `fontHeading` | 28 | HomeMenu, IncomingCall caller name, PhoneView dialed number |

**Icons (22 instances) -- map to existing tokens + scaling expressions:**

| Hardcoded Value | Token/Expression | Rationale |
|-----------------|------------------|-----------|
| `size: 16` | `UiMetrics.iconSmall` (base 20) | Close icon in notifications -- iconSmall is close enough |
| `size: 18` | `UiMetrics.iconSmall` | GestureOverlay button icons |
| `size: 20` | `UiMetrics.iconSmall` | Sidebar horizontal, NotificationArea info icon |
| `size: 22` | `Math.round(22 * UiMetrics.scale)` | GestureOverlay slider icons, PhoneView backspace/dial |
| `size: 24` | `UiMetrics.iconSize` (base 36) is too large; use `Math.round(24 * UiMetrics.scale)` | Sidebar vertical volume, BottomBar volume |
| `size: 28` | `Math.round(28 * UiMetrics.scale)` | Sidebar horizontal home |
| `size: 32` | `UiMetrics.iconSize` | Sidebar home, BtAudio skip, IncomingCall buttons, PhoneView |
| `size: 36` | `UiMetrics.iconSize` | PhoneView call buttons |
| `size: 48` | `Math.round(48 * UiMetrics.scale)` | BtAudio play/pause |
| `size: 72` | `Math.round(72 * UiMetrics.scale)` | BtAudio album art placeholder icon |

### Recommended New Tokens (~6-8)

Based on the audit, these new tokens would eliminate all remaining hardcoded dimensions:

| Token Name | Base Value | Pattern | Purpose | Where Used |
|------------|------------|---------|---------|------------|
| `radiusSmall` | 8 | A (overridable) | Smaller corners for buttons, tiles, inputs | Tile, PhoneView buttons, GestureOverlay buttons, NavStrip items |
| `radiusLarge` | 16 | A (overridable) | Panel/overlay corners | GestureOverlay panel |
| `albumArt` | 200 | A (overridable) | Album art square dimension | BtAudioView |
| `callBtnSize` | 72 | A (overridable, floor = touchMin) | Circular call action buttons | PhoneView (80), IncomingCallOverlay (72) |
| `statusDot` | 10 | C (non-overridable) | Small status indicator dots | PhoneView connection dot |
| `progressH` | 4 | C (non-overridable) | Thin progress/seekbar track height | BtAudioView progress bar |
| `overlayBtnW` | 100 | A (overridable, floor = touchMin for height) | GestureOverlay action buttons width | GestureOverlay Home/Night/Close buttons |
| `overlayBtnH` | 36 | A (overridable, floor = touchMin) | GestureOverlay action buttons height | GestureOverlay Home/Night/Close buttons |

**Discretion recommendations:**

1. **Button token strategy:** Use `callBtnSize` (72 base) with `Math.max(computed, touchMin)` floor. One token handles both PhoneView (80 -> can use callBtnSize) and IncomingCallOverlay (72). The PhoneView currently uses 80 but 72 scaled would be visually equivalent. Keep it simple with one token.

2. **Divider height:** Keep at literal `1` -- dividers should always be 1 physical pixel. At sub-100px resolution the scaled value would round to 0. This is a standard practice in UI frameworks. The existing `height: 1` lines in settings views and TopBar should stay as-is.

3. **Radius strategy:** Standardize to 3 tiers:
   - `radiusSmall` (8): buttons, tiles, inputs, small cards
   - `radius` (10, already exists): general purpose, medium panels
   - `radiusLarge` (16): overlay panels, large dialogs
   - The 9/11/12 differences are visually indistinguishable at car-arm distance. Collapse 8/9 -> radiusSmall, 11/12 -> radius, 15/16 -> radiusLarge.

4. **Spacing:** Reuse existing spacing tokens (spacing=8, marginRow=12, gap=16, marginPage=20, sectionGap=20). The oddball 87px in BtAudioView context doc doesn't appear in current code (was likely the `spacing: 32` between playback controls -- use `sectionGap` or `gap * 2`). No new spacing tokens needed.

5. **Settings sub-views:** Settings files (AASettings, VideoSettings, EqSettings, ConnectivitySettings, ConnectionSettings) have hardcoded `height: 1` dividers and `padding: 0` resets. The `height: 1` dividers should stay as literal 1 (see divider decision above). The `padding: 0` is zero -- not a pixel value to tokenize. The `radius: 12` in CompanionSettings maps to existing `radius` token. Include settings in Plan 3.

6. **Album art:** Use fixed token `albumArt` (base 200) with standard `scale` multiplication. Proportional-to-content-area is fragile (depends on sibling layout), while a fixed scaled token is predictable and overridable.

7. **Call button floor:** Use `touchMin` (base 56) as the floor. The `callBtnSize` base of 72 will always be above touchMin at standard scales, and the floor catches extreme downscaling.

### Anti-Patterns to Avoid
- **Inline Math.round everywhere:** For values used more than once, create a UiMetrics token rather than repeating `Math.round(N * UiMetrics.scale)` in multiple files. Single-use scaled values are fine inline.
- **Over-tokenizing:** Don't create tokens for values used once in one file. Use inline `Math.round(N * UiMetrics.scale)` for one-off dimensions.
- **Replacing 0 and 1 divider heights:** `height: 0` (spacer) and `height: 1` (divider) are structural, not design tokens. Leave them as literals.
- **Tokenizing debug overlay values:** The touch debug overlay in AndroidAutoMenu.qml (30x30 crosshairs, 15px radius, font 10) is intentionally hardcoded for debugging. Exclude it.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Proportional scaling | Custom per-file scale calculations | `UiMetrics.scale` / `UiMetrics._fontScaleTotal` | Already handles dual-axis scaling, config overrides, and font floors |
| Token overrides | Custom config lookup per file | `UiMetrics._tok()` helper | Centralized, consistent NaN-based fallback pattern |
| Touch target enforcement | Per-button minimum size checks | `Math.max(computed, UiMetrics.touchMin)` | touchMin already scales; use it as floor parameter |

## Common Pitfalls

### Pitfall 1: Breaking Existing UiMetrics Consumers
**What goes wrong:** Renaming or changing existing token base values breaks files that already use them correctly (ExitDialog, FirstRunBanner, LauncherMenu, etc.)
**Why it happens:** Desire to "normalize" everything at once
**How to avoid:** Only ADD new tokens. Never modify existing token names or base values. Existing consumers are correct.
**Warning signs:** Files that already reference `UiMetrics.*` shouldn't appear in diffs unless they also had OTHER hardcoded values.

### Pitfall 2: Forgetting `Math.round()` on Scaled Values
**What goes wrong:** Fractional pixel values cause subpixel rendering artifacts (blurry text, aliased borders)
**Why it happens:** Writing `48 * UiMetrics.scale` instead of `Math.round(48 * UiMetrics.scale)`
**How to avoid:** Every inline scaled expression MUST use `Math.round()`. Token properties in UiMetrics already do this.
**Warning signs:** Any `* scale` or `* UiMetrics.scale` without a surrounding `Math.round()`

### Pitfall 3: Using Font Scale for Layout or Layout Scale for Fonts
**What goes wrong:** Fonts scale differently than layout (geometric mean vs min-axis). Using the wrong one causes either oversized fonts or undersized layout.
**Why it happens:** Not distinguishing between `scale` (layout) and `_fontScaleTotal` (fonts)
**How to avoid:** Font tokens use `_fontScaleTotal`. Everything else uses `scale`. Inline scaled values that aren't fonts MUST use `UiMetrics.scale`.
**Warning signs:** A `font.pixelSize` referencing `UiMetrics.scale` instead of a font token.

### Pitfall 4: Tokenizing Fixed-Size Debug Overlays
**What goes wrong:** Debug overlays (touch crosshairs) become unusably small/large at extreme scales
**Why it happens:** Zealous application of the "zero hardcoded values" rule
**How to avoid:** The debug touch overlay in AndroidAutoMenu.qml is explicitly excluded from AUDIT-10. Leave its 30x30 crosshairs, `font.pixelSize: 10`, and 15px radius as-is.
**Warning signs:** Changes in AndroidAutoMenu.qml to the Repeater delegate inside the debug touch overlay.

### Pitfall 5: BottomBar Slider Already Uses Token-Sized Elements
**What goes wrong:** Double-applying tokens to elements that already use trackThick/knobSize dimensions
**Why it happens:** Not checking if a hardcoded value coincidentally matches an existing token's base
**How to avoid:** BottomBar's slider `implicitHeight: 6` matches `trackThick` (base 6), and `implicitWidth: 22` / `implicitHeight: 22` match `knobSize` (base 22). Replace with the tokens directly.
**Warning signs:** Math doesn't match: `Math.round(6 * UiMetrics.scale)` is redundant when `UiMetrics.trackThick` already does exactly that.

### Pitfall 6: Changing `width: 1024; height: 600` in main.qml
**What goes wrong:** These are the default window dimensions, not a layout constraint. Changing them to tokens would create a circular dependency (UiMetrics reads window size to compute scale).
**How to avoid:** Leave `main.qml` width/height as-is. They are initial window size, not a design token.
**Warning signs:** Any changes to main.qml's root Item width/height.

## Code Examples

### Adding a New Overridable Token to UiMetrics.qml
```qml
// Pattern A: Overridable layout token
readonly property int albumArt: {
    var o = _tok("albumArt");
    return isNaN(o) ? Math.round(200 * scale) : o;
}

// Pattern A with touch-target floor
readonly property int callBtnSize: {
    var o = _tok("callBtnSize");
    return isNaN(o) ? Math.max(Math.round(72 * scale), touchMin) : o;
}
```

### Replacing Hardcoded Font Size
```qml
// BEFORE
font.pixelSize: 14

// AFTER (maps to fontSmall, base 16 -- closest semantic match)
font.pixelSize: UiMetrics.fontSmall
```

### Replacing Hardcoded Button Dimensions with Floor
```qml
// BEFORE
Layout.preferredWidth: 72
Layout.preferredHeight: 72

// AFTER
Layout.preferredWidth: UiMetrics.callBtnSize
Layout.preferredHeight: UiMetrics.callBtnSize
```

### Replacing Hardcoded Spacing with Existing Token
```qml
// BEFORE
spacing: 12

// AFTER (marginRow base is 12)
spacing: UiMetrics.marginRow
```

### Inline Scaling for One-Off Values
```qml
// BEFORE
Layout.preferredWidth: 40

// AFTER (one-off, not worth a token)
Layout.preferredWidth: Math.round(40 * UiMetrics.scale)
```

### Registering Token Defaults in YamlConfig.cpp
```cpp
// In initDefaults(), after existing ui.* entries:
root_["ui"]["tokens"]["radiusSmall"] = 0;    // 0 = use auto-derived
root_["ui"]["tokens"]["radiusLarge"] = 0;
root_["ui"]["tokens"]["albumArt"] = 0;
root_["ui"]["tokens"]["callBtnSize"] = 0;
root_["ui"]["tokens"]["overlayBtnW"] = 0;
root_["ui"]["tokens"]["overlayBtnH"] = 0;
```

### Grep Verification Command
```bash
# Should return ZERO results (excluding debug overlay)
grep -rn -E '(font\.pixelSize|\.pixelSize):\s*[0-9]+' qml/ --include="*.qml" \
  | grep -v UiMetrics.qml \
  | grep -v 'AndroidAutoMenu.qml.*font.pixelSize: 10'

# Dimension check (width/height/radius with numeric literals)
grep -rn -E '(radius|implicitWidth|implicitHeight|Layout\.preferred(Width|Height)):\s*[0-9]+' qml/ --include="*.qml" \
  | grep -v UiMetrics.qml \
  | grep -v -E '(UiMetrics\.|border\.width:\s*[12]$|: 0$)' \
  | grep -v 'AndroidAutoMenu.qml.*debug'
```

## File-by-File Audit Summary

### Plan 1: New Tokens + Shell Chrome

| File | Hardcoded Count | What Changes |
|------|-----------------|--------------|
| `UiMetrics.qml` | N/A | ADD ~6-8 new tokens |
| `YamlConfig.cpp` | N/A | Register new token defaults |
| `TopBar.qml` | 3 | margins (16->gap), spacing (12->marginRow), divider height stays 1 |
| `BottomBar.qml` | 5 | margins (20->marginPage), spacing (12->marginRow), slider dims (6->trackThick, 22->knobSize), icon size (24->scaled), radius (3->trackThick/2, 11->knobSize/2) |
| `NavStrip.qml` | 3 | margins (12->marginRow), spacing (8->spacing), radius (8->radiusSmall) |
| `Shell.qml` | 0 | Already clean (proportional heights via percentages) |
| `NotificationArea.qml` | 7 | margins (8->spacing), width cap, padding (16->gap), radius (8->radiusSmall), font (14->fontSmall), icons (20->iconSmall, 16->iconSmall) |

### Plan 2: Plugin Views

| File | Hardcoded Count | What Changes |
|------|-----------------|--------------|
| `GestureOverlay.qml` | ~25 | Panel dims (500 max width, 48 padding->marginPage*2+spacing), radius (16->radiusLarge, 8->radiusSmall), fonts (14/16->fontSmall), icons (18/22->iconSmall/scaled), button dims (100x36->overlayBtn tokens), slider label width (40->scaled), spacing (6/12/16/24->spacing/marginRow/gap) |
| `PhoneView.qml` | ~20 | margins (16->gap), spacing (8/12/16->spacing/marginRow/gap), fonts (12/14/18/20/24/28->fontTiny-fontHeading), status dot (10->statusDot), display height (60->scaled), buttons (64/80->scaled with touchMin floor), radius (5/8->statusDot/2, radiusSmall) |
| `IncomingCallOverlay.qml` | ~12 | spacing (24/48->sectionGap/gap*3), fonts (18/28/16->fontBody/fontHeading/fontSmall), spacer (16->gap), buttons (72->callBtnSize), icons (32->iconSize) |
| `BtAudioView.qml` | ~20 | spacing (4/8/20/32->spacing/marginRow*2/sectionGap), fonts (12/14/16/22->fontTiny-fontTitle), album art (200->albumArt), radius (12->radius), icons (32/48/72->scaled), buttons (56/72->touchMin/callBtnSize), progress bar (4/2->progressH/progressH/2) |
| `Sidebar.qml` | ~20 | margins (6/8->spacing), fonts (12->fontTiny), icons (20/24/28/32->iconSmall/scaled/iconSize), bar dims (6->trackThick), knob (18->knobSizeSmall), home button (56->touchMin), divider (1 stays) |

### Plan 3: Controls, Dialogs, Remaining

| File | Hardcoded Count | What Changes |
|------|-----------------|--------------|
| `NormalText.qml` | 1 | font.pixelSize: 16 -> UiMetrics.fontSmall |
| `SpecialText.qml` | 1 | font.pixelSize: 16 -> UiMetrics.fontSmall |
| `Tile.qml` | 2 | radius: 8 -> UiMetrics.radiusSmall, spacing: 8 -> UiMetrics.spacing |
| `PairingDialog.qml` | 3 | radius: 12 -> UiMetrics.radius, button width: 140 -> scaled, button radius: 8 -> UiMetrics.radiusSmall |
| `HomeMenu.qml` | 1 | font.pixelSize: 28 -> UiMetrics.fontHeading |
| `CompanionSettings.qml` | 1 | radius: 12 -> UiMetrics.radius |
| `AndroidAutoMenu.qml` | 1 | `Math.round(40 * UiMetrics.scale)` already scaled; debug overlay excluded |
| Settings dividers | ~15 | `height: 1` stays as literal (divider decision) |
| `main.qml` | 2 | `width: 1024; height: 600` stays (initial window size, not design token) |

## Open Questions

1. **`padS/padM/padL/padXL` mentioned in CONTEXT.md but don't exist**
   - What we know: CONTEXT.md says "UiMetrics already has padS (4), padM (8), padL (16), padXL (24)." The actual UiMetrics.qml has `spacing` (8), `marginRow` (12), `gap` (16), `marginPage` (20), `sectionGap` (20). No padS/M/L/XL tokens exist.
   - What's unclear: Whether the intent was to rename existing tokens or add aliases.
   - Recommendation: Use existing token names as-is. The CONTEXT.md reference appears to be aspirational naming. The existing tokens (`spacing`, `marginRow`, `gap`, `marginPage`, `sectionGap`) are already well-distributed across the spacing range and cover all needed values.

2. **Sidebar scaling at extreme downscale (800x480)**
   - What we know: Sidebar has volume bar (6px track), knob (18px), and home button (56px). At 800x480, scale = ~0.78, so knob becomes ~14px and home button ~44px.
   - What's unclear: Whether the sidebar is usable at 800x480 given it's only visible during AA projection (and likely on 1024x600+ screens in practice).
   - Recommendation: Proceed with standard scaling. The sidebar width is user-configured and adapts independently. Touch targets will respect touchMin floors.

## Sources

### Primary (HIGH confidence)
- Direct codebase audit of all 45 QML files -- exhaustive grep for hardcoded values
- `qml/controls/UiMetrics.qml` -- current token inventory and patterns
- `src/core/YamlConfig.cpp` -- config registration pattern
- `.planning/phases/03-qml-hardcoded-island-audit/03-CONTEXT.md` -- user decisions

### Secondary (MEDIUM confidence)
- Phase 1/2 implementation patterns (established in prior work)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all infrastructure exists in-tree, no external dependencies
- Architecture: HIGH -- token patterns established in Phase 1/2, this is pure application of existing patterns
- Pitfalls: HIGH -- based on direct codebase reading, not speculation

**Research date:** 2026-03-03
**Valid until:** 2026-04-03 (stable -- no external dependencies to go stale)
