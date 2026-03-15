# Phase 3: Theme Color System - Research

**Researched:** 2026-03-08
**Domain:** Qt 6 QML theming, AA protocol UiConfigRequest, semantic color systems
**Confidence:** HIGH

## Summary

This phase renames all 16 existing ThemeService Q_PROPERTYs to match the AA wire token names, adds derived colors for scrim/pressed/status indicators, eliminates all 59+ hardcoded hex values across 12+ QML files, migrates 4 bundled theme YAMLs, adds a 5th "Connected Device" theme, parses UiConfigRequest (0x8011) from the AA video channel, and updates the web config panel.

The codebase is well-structured for this change. ThemeService already uses `activeColor(key)` internally -- the rename is mostly mechanical (change key strings + Q_PROPERTY names + QML references). The Connected Device theme is the most novel piece: parsing protobuf UiConfigEntry key-value pairs, converting ARGB uint32 to QColor, writing to YAML, and wiring through the orchestrator.

**Primary recommendation:** Execute as a 3-wave migration: (1) rename properties + migrate YAML themes, (2) eliminate QML hardcodes with derived colors, (3) add Connected Device theme with UiConfigRequest parsing.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Semantic color system matches the 16 tokens AA sends via UiConfigRequest (msg 0x8011): primary, on_surface, surface, surface_variant, surface_container_low, inverse_surface, inverse_on_surface, outline, outline_variant, background, text_primary, text_secondary, red, on_red, yellow, on_yellow
- ThemeService accepts all 46 known token keys by string (future-proofing) but only depends on the 16 confirmed wire tokens
- Colors beyond the 16 (scrim, pressed state, success green, etc.) are derived from the 16 base tokens in ThemeService -- not stored in YAML
- Wire token names used directly: `red`/`onRed` not `error`/`onError`, `yellow`/`onYellow` not `warning`/`onWarning`
- All 16 existing ThemeService Q_PROPERTYs renamed to AA token names (clean break, no alias layer)
- All QML references updated in the same pass
- "Connected Device" theme -- HU chrome adopts phone's Material You palette when AA is connected
- Minimal YAML theme file with metadata + default fallback palette (copy of default theme colors)
- When AA sends UiConfigRequest tokens, ThemeService writes them directly into the Connected Device theme YAML file
- Theme file IS the cache -- persists last-received colors across launches/disconnects
- All 59 hardcoded hex values across 12 QML files eliminated
- Flat key-value pairs using AA token names in YAML day/night sections
- All 4 bundled themes (default, AMOLED, Ocean, Ember) migrated to new token names with full palettes
- Clean break on property names -- no migration logic for old custom themes
- Web config panel theme routes updated to use new AA token names

### Claude's Discretion
- Exact derivation formulas for non-base colors (scrim opacity, pressed state dimming, etc.)
- Old-to-new property mapping decisions where the mapping isn't 1:1
- UiConfigRequest parsing implementation details (protobuf deserialization, ARGB uint32 -> QColor conversion)
- Test coverage approach for the color system migration
- How to handle the UiConfigRequest on the video channel (new handler method or extension of existing)
- Whether to send UiConfigRequest proactively on AA connect or wait for phone to request

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| THM-01 | Theme color palette is fully fleshed out with named semantic roles (surface, primary, secondary, accent, error, etc.) | 16 AA wire tokens as base roles + derived colors for scrim/pressed/status; old-to-new property mapping table; derivation formulas |
| THM-02 | Theme colors align with Material Design color system conventions for consistency and familiarity | AA wire tokens ARE Material Design 3 dynamic color tokens; using them directly ensures alignment |
| THM-03 | All UI elements consistently use semantic theme colors (no hardcoded color values in QML) | Full inventory of 59+ hardcodes across 12+ QML files; mapping each to appropriate semantic token or derived color |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt 6.4/6.8 | 6.4 (dev) / 6.8 (Pi) | QML property binding, Q_PROPERTY system | Already in use, no additions needed |
| yaml-cpp | existing | Theme YAML read/write | Already linked, used by ConfigService and ThemeService |
| protobuf | existing | UiConfigRequest deserialization | Already compiled in oaa-protocol lib |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| QColor | Qt 6 | ARGB conversion, color manipulation | Converting uint32 wire values, computing derived colors |

### Alternatives Considered
None -- this phase uses only existing dependencies.

**Installation:**
```bash
# No new dependencies needed
```

## Architecture Patterns

### Property Rename Mapping

The old 16 properties map to the new AA wire token names. This is the critical mapping table:

| Old Property | Old YAML Key | New Property | New YAML Key | Mapping Rationale |
|-------------|-------------|-------------|-------------|-------------------|
| `backgroundColor` | `background` | `background` | `background` | Direct match |
| `highlightColor` | `highlight` | `primary` | `primary` | Accent/action color = Material primary |
| `controlBackgroundColor` | `control_background` | `surfaceVariant` | `surface_variant` | Interactive surface = surface_variant |
| `controlForegroundColor` | `control_foreground` | `onSurface` | `on_surface` | Text/icons on controls |
| `normalFontColor` | `normal_font` | `textPrimary` | `text_primary` | Main body text |
| `specialFontColor` | `special_font` | `primary` | `primary` | Accent text = primary (same as highlight) |
| `descriptionFontColor` | `description_font` | `textSecondary` | `text_secondary` | Subdued text |
| `barBackgroundColor` | `bar_background` | `surfaceContainerLow` | `surface_container_low` | Navbar/bar background |
| `controlBoxBackgroundColor` | `control_box_background` | `surface` | `surface` | Card/panel background |
| `gaugeIndicatorColor` | `gauge_indicator` | `primary` | `primary` | Gauge accent = primary |
| `iconColor` | `icon` | `onSurface` | `on_surface` | Icon tint = on_surface |
| `sideWidgetBackgroundColor` | `side_widget_background` | `surface` | `surface` | Side panel = surface |
| `barShadowColor` | `bar_shadow` | (derived) | -- | Derived from background with alpha |
| `dividerColor` | `divider` | `outlineVariant` | `outline_variant` | Divider lines |
| `pressedColor` | `pressed` | (derived) | -- | Derived from on_surface with alpha |
| `highlightFontColor` | `highlight_font` | `inverseSurface` | `inverse_surface` | Text on primary buttons |

**Note:** `specialFontColor` and `gaugeIndicatorColor` both map to `primary`. This is correct -- they were always the accent color. The new system has fewer properties because AA tokens consolidate roles. QML references that used `ThemeService.specialFontColor` and `ThemeService.gaugeIndicatorColor` will both use `ThemeService.primary`.

### New Q_PROPERTY Set (16 base + derived)

**16 Base Properties (from YAML):**
```cpp
Q_PROPERTY(QColor primary READ primary NOTIFY colorsChanged)
Q_PROPERTY(QColor onSurface READ onSurface NOTIFY colorsChanged)
Q_PROPERTY(QColor surface READ surface NOTIFY colorsChanged)
Q_PROPERTY(QColor surfaceVariant READ surfaceVariant NOTIFY colorsChanged)
Q_PROPERTY(QColor surfaceContainerLow READ surfaceContainerLow NOTIFY colorsChanged)
Q_PROPERTY(QColor inverseSurface READ inverseSurface NOTIFY colorsChanged)
Q_PROPERTY(QColor inverseOnSurface READ inverseOnSurface NOTIFY colorsChanged)
Q_PROPERTY(QColor outline READ outline NOTIFY colorsChanged)
Q_PROPERTY(QColor outlineVariant READ outlineVariant NOTIFY colorsChanged)
Q_PROPERTY(QColor background READ background NOTIFY colorsChanged)
Q_PROPERTY(QColor textPrimary READ textPrimary NOTIFY colorsChanged)
Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY colorsChanged)
Q_PROPERTY(QColor red READ red NOTIFY colorsChanged)
Q_PROPERTY(QColor onRed READ onRed NOTIFY colorsChanged)
Q_PROPERTY(QColor yellow READ yellow NOTIFY colorsChanged)
Q_PROPERTY(QColor onYellow READ onYellow NOTIFY colorsChanged)
```

**Derived Properties (computed, not in YAML):**
```cpp
// Scrim: semi-transparent black overlay for dialogs/popups
QColor scrim() const { return QColor(0, 0, 0, 180); }  // #000000B4 (~70% opacity)

// Pressed: interaction feedback overlay
QColor pressed() const {
    QColor c = activeColor("on_surface");
    c.setAlpha(26);  // ~10% opacity
    return c;
}

// Success green: derive from primary or use a fixed accessible green
QColor success() const { return QColor("#4CAF50"); }
QColor onSuccess() const { return QColor("#FFFFFF"); }

// Bar shadow: derived from background
QColor barShadow() const { return QColor(0, 0, 0, 128); }
```

### Derived Color Recommendations

| Derived Color | Formula | Use Cases |
|--------------|---------|-----------|
| `scrim` | `#000000` at 70% alpha | Dialog/overlay backgrounds (PairingDialog, IncomingCallOverlay, CompanionSettings modal, GestureOverlay backdrop) |
| `pressed` | `onSurface` at 10% alpha | Button/tile press feedback |
| `barShadow` | `#000000` at 50% alpha | Navbar shadow |
| `success` | Fixed `#4CAF50` | Connected indicators, accept buttons |
| `onSuccess` | Fixed `#FFFFFF` | Text on success backgrounds |
| `danger` | Alias for `red` token | Reject/disconnect buttons (red already in base 16) |
| `onDanger` | Alias for `onRed` token | Text on danger buttons |
| `warning` | Alias for `yellow` token | Degraded status (yellow already in base 16) |

**Key insight:** Most "missing" semantic colors (error, warning, success) map directly to the AA tokens (`red`, `yellow`) or are simple fixed values. Only scrim and pressed need actual derivation.

### ARGB uint32 to QColor Conversion

The AA wire format sends colors as `uint32` in ARGB format (alpha in high byte):

```cpp
QColor argbToQColor(uint32_t argb) {
    return QColor::fromRgba(
        ((argb & 0xFF000000))       |  // alpha stays in place
        ((argb & 0x00FF0000) >> 16) |  // R -> B position for RGBA
        ((argb & 0x0000FF00))       |  // G stays
        ((argb & 0x000000FF) << 16)    // B -> R position for RGBA
    );
    // Actually simpler: QColor has fromRgba which expects AARRGGBB
    // But QColor::fromRgba() expects 0xAARRGGBB which IS the ARGB format.
    // So: return QColor::fromRgba(argb);  -- that's it.
}
```

**Verification (HIGH confidence):** Qt's `QColor::fromRgba(QRgb)` takes `0xAARRGGBB` which matches Android's ARGB uint32 format directly. No byte swapping needed.

### UiConfigRequest Integration Point

The UiConfigRequest (0x8011) needs to be sent from the HU to the phone on the video channel. The response (0x8012, UpdateHuUiConfigResponse with ThemingTokensStatus) comes back.

**Recommended approach:** Add a method to `AndroidAutoOrchestrator` that:
1. Constructs `UiConfigRequest` with the current theme's 16 tokens
2. Sends on the video channel after session setup
3. Handles `UpdateHuUiConfigResponse` (0x8012) -- log accepted/rejected status

**However**, for the Connected Device theme, the flow is reversed -- we want to RECEIVE the phone's palette. The 0x8011 message is HU->Phone (we send our palette TO the phone so it can style the projected UI). The phone's Material You colors come through a different mechanism or may not be directly available on the wire.

**IMPORTANT CLARIFICATION:** Re-reading the CONTEXT.md more carefully -- the Connected Device theme sends OUR theme tokens TO the phone (so the AA projected UI matches our HU chrome). When the phone sends its palette back via the response, we could potentially extract colors, but the response is just a status enum (accepted/rejected/error), not colors.

**Revised understanding:** The Connected Device theme might work differently than initially assumed. The phone's Material You palette might be communicated via the `UiConfigRequest` mechanism where we send tokens and the phone applies them. The CONTEXT.md says "when AA sends UiConfigRequest tokens" -- this could mean the phone sends its current tokens, but 0x8011 is HU->Phone per the proto audit.

**Recommendation:** For the planner -- implement the Connected Device theme as:
1. A theme YAML with default fallback colors
2. A `setTokenColors(QMap<QString, QColor>)` method on ThemeService that writes to the Connected Device YAML and reloads
3. Wire the UiConfigRequest sending (HU->Phone) for all themes so the projected AA UI matches HU chrome
4. If phone-side color extraction is needed, that's a separate investigation -- the proto audit shows 0x8011 is HU->Phone only

### Hardcoded Color Inventory

Full categorization of the 59+ hardcoded hex values across QML files:

**Scrim/Overlay backgrounds (replace with `ThemeService.scrim`):**
- `PairingDialog.qml`: `#CC000000`
- `IncomingCallOverlay.qml`: `#CC000000`
- `CompanionSettings.qml`: `#CC000000`
- `GestureOverlay.qml`: `#AA000000`

**Panel/card backgrounds (replace with `ThemeService.surface` or `surfaceContainerLow` with alpha):**
- `GestureOverlay.qml`: `#DD1a1a2e`, `#0f3460`
- `NotificationArea.qml`: `#DD2a2a3e`, `#0f3460`

**Text colors (replace with `ThemeService.textPrimary` or `textSecondary`):**
- `GestureOverlay.qml`: 10x `#e0e0e0`, 2x `#a0a0c0`
- `NotificationArea.qml`: 2x `#e0e0e0`, 1x `#808080`
- `IncomingCallOverlay.qml`: `#AAAAAA`, `#CCCCCC`

**Success green (replace with `ThemeService.success`):**
- `PhoneView.qml`: 3x `#4CAF50`, 1x `#388E3C`
- `ConnectionSettings.qml`: `#44aa44`
- `ConnectivitySettings.qml`: `#44aa44`
- `CompanionSettings.qml`: 2x `#4CAF50`

**Error/danger red (replace with `ThemeService.red`):**
- `PhoneView.qml`: 2x `#F44336`, 1x `#D32F2F`
- `IncomingCallOverlay.qml`: `#F44336`, `#D32F2F`
- `ExitDialog.qml`: `#F44336`
- `PairingDialog.qml`: `#cc4444`
- `ConnectionSettings.qml`: 2x `#cc4444`
- `ConnectivitySettings.qml`: `#cc4444`
- `CompanionSettings.qml`: `#F44336`
- `EqSettings.qml`: `#d32f2f`

**Warning/status (replace with `ThemeService.yellow` or `warning`):**
- `CompanionSettings.qml`: 2x `#FF9800`

**Pressed state colors (replace with derived pressed):**
- `GestureOverlay.qml`: 3x `#e94560` (pressed) vs `#0f3460` (normal) -- ternary patterns
- `PhoneView.qml`: `#388E3C`/`#4CAF50`, `#D32F2F`/`#F44336` -- pressed pairs
- `IncomingCallOverlay.qml`: same pressed pair pattern

**Debug colors (keep or wrap in debug conditional):**
- `AndroidAutoMenu.qml`: `#00FF00` (4x, touch crosshair debug), `#666666`, `#FFA500`, `#FFFF00` (signal quality colors)

**Disabled/inactive (replace with `ThemeService.textSecondary` or `outline`):**
- `PhoneView.qml`: `#666666` (disabled button)

### Theme YAML Migration

Each theme needs all 16 keys in both day and night sections. Example for default theme:

```yaml
id: default
name: Default Theme
version: 2.0.0
font_family: "Lato"

day:
  primary: "#e94560"
  on_surface: "#e0e0e0"
  surface: "#16213e"
  surface_variant: "#16213e"
  surface_container_low: "#0f3460"
  inverse_surface: "#1a1a2e"
  inverse_on_surface: "#ffffff"
  outline: "#a0a0a0"
  outline_variant: "#ffffff26"
  background: "#1a1a2e"
  text_primary: "#ffffff"
  text_secondary: "#a0a0a0"
  red: "#cc4444"
  on_red: "#ffffff"
  yellow: "#FF9800"
  on_yellow: "#000000"

night:
  primary: "#c73650"
  on_surface: "#b0b0b0"
  surface: "#0e1729"
  # ... (dimmed variants)
```

### Web Config Panel Migration

The `web-config/templates/themes.html` has its own `colorKeys` array that must be updated to the new 16 AA token names. The `base.html` has ~15 hardcoded colors in its CSS that should also be migrated (though the web panel is a separate UI from the Qt app -- it could reference the theme via API or remain static).

**Recommendation:** Update `colorKeys` in themes.html to the 16 AA token names. Leave base.html CSS as-is for now (it's a standalone web UI, not QML).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| ARGB conversion | Manual bit shifting | `QColor::fromRgba(uint32_t)` | Qt handles ARGB format natively |
| Color derivation | Complex HSL manipulation | Simple alpha overlay on base tokens | Keep derived colors predictable |
| YAML serialization | Manual string formatting | `yaml-cpp` YAML::Emitter | Already used throughout codebase |
| Property change signals | Per-property signals | Single `colorsChanged` signal | Existing pattern, all properties update atomically on theme switch |

## Common Pitfalls

### Pitfall 1: QML Property Name Breakage
**What goes wrong:** Renaming Q_PROPERTYs without updating ALL QML references causes silent binding failures -- QML shows transparent/default colors instead of crashing.
**Why it happens:** QML property lookups that fail return `undefined`, which QColor treats as transparent.
**How to avoid:** Use grep/search to find ALL references before AND after rename. The 34 QML files that reference ThemeService must all be checked.
**Warning signs:** Elements becoming invisible or transparent after theme switch.

### Pitfall 2: specialFontColor and gaugeIndicatorColor Collapse
**What goes wrong:** Three old properties (highlight, special_font, gauge_indicator) all map to `primary`. QML code that distinguished between them will now use the same color.
**Why it happens:** AA's token system is more consolidated than the old OAP-derived naming.
**How to avoid:** This is intentional -- verify the visual result is acceptable. If some elements need differentiation, use `primary` with opacity variants.

### Pitfall 3: Test Data Themes
**What goes wrong:** Tests use theme data in `tests/data/themes/` which reference old key names. Tests break after rename.
**Why it happens:** Test fixtures have their own theme YAMLs with old keys.
**How to avoid:** Migrate test theme data at the same time as production themes. The existing test_theme_service.cpp tests reference `backgroundColor()`, `highlightColor()` etc. by accessor name -- those tests need updating.

### Pitfall 4: IPC Color Key Names
**What goes wrong:** `IpcServer::handleGetTheme()` exports colors using map keys directly. `handleSetTheme()` imports using keys from web panel. Both must use new key names.
**Why it happens:** IPC passes through raw YAML key names -- renaming keys in YAML means IPC responses change format.
**How to avoid:** Update web config `colorKeys` array in themes.html simultaneously with YAML key rename.

### Pitfall 5: Connected Device Theme File Permissions
**What goes wrong:** Writing the Connected Device theme YAML to `~/.openauto/themes/connected-device/theme.yaml` could fail on first run if the directory doesn't exist.
**Why it happens:** The theme directory structure is only created when the user customizes themes.
**How to avoid:** Use `QDir::mkpath()` before writing, or bundle the Connected Device theme in `config/themes/` (bundled themes are read-only, so the writable copy needs to go in `~/.openauto/themes/`).

### Pitfall 6: ARGB vs RGBA Confusion
**What goes wrong:** Android uses ARGB (alpha in high byte: 0xAARRGGBB). Qt's `QColor::fromRgba()` also expects ARGB. But `QColor::rgba()` returns ARGB too. Confusion arises if someone uses `QColor::fromRgb()` which ignores alpha.
**How to avoid:** Always use `QColor::fromRgba()` for the wire format conversion. Verify with a known color (e.g., opaque white = 0xFFFFFFFF should produce QColor(255,255,255,255)).

## Code Examples

### ThemeService Property Accessor Pattern (existing, rename only)
```cpp
// Source: src/core/services/ThemeService.hpp (current pattern)
QColor primary() const { return activeColor("primary"); }
QColor surface() const { return activeColor("surface"); }
QColor textPrimary() const { return activeColor("text_primary"); }
// ... same pattern for all 16 base properties
```

### Derived Color Pattern
```cpp
// Recommended: derived colors computed at access time from base tokens
QColor scrim() const {
    return QColor(0, 0, 0, 180);  // Fixed 70% black -- standard Material scrim
}

QColor pressed() const {
    QColor c = activeColor("on_surface");
    c.setAlpha(26);  // 10% of content color
    return c;
}

QColor success() const {
    return QColor("#4CAF50");  // Material green 500
}
```

### AA Token Ingestion Method
```cpp
// New method on ThemeService for Connected Device theme
void ThemeService::applyAATokens(const QMap<QString, uint32_t>& argbTokens)
{
    // Only apply if Connected Device theme is active
    if (currentThemeId() != "connected-device") return;

    auto& colors = nightMode_ ? nightColors_ : dayColors_;
    for (auto it = argbTokens.begin(); it != argbTokens.end(); ++it) {
        colors[it.key()] = QColor::fromRgba(it.value());
    }

    // Write to theme YAML for persistence
    persistConnectedDeviceTheme();
    emit colorsChanged();
}
```

### UiConfigRequest Protobuf Construction
```cpp
// Source: proto/oaa/video/UiConfigRequestMessage.proto
oaa::proto::messages::UiConfigRequest request;
auto* data = request.mutable_config();
for (const auto& [key, color] : themeColors) {
    auto* entry = data->add_primary_configs();
    entry->set_key(key.toStdString());
    auto* value = entry->mutable_config_value();
    value->set_value(color.rgba());  // QColor::rgba() returns ARGB uint32
}
// Send as msg 0x8011 on video channel
```

### QML Migration Example
```qml
// BEFORE (hardcoded):
Rectangle {
    color: "#CC000000"  // semi-transparent black overlay
}
Text {
    color: "#e0e0e0"  // hardcoded white-ish text
}
Rectangle {
    color: parent.pressed ? "#e94560" : "#0f3460"  // hardcoded pressed state
}

// AFTER (themed):
Rectangle {
    color: ThemeService.scrim  // derived: #000000 at 70% alpha
}
Text {
    color: ThemeService.textPrimary  // base token
}
Rectangle {
    color: parent.pressed ? ThemeService.primary : ThemeService.surfaceContainerLow
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| OAP-derived color names (highlight, control_background, etc.) | AA wire token names (primary, surface, surface_variant, etc.) | This phase | Property names change, YAML keys change, all QML references change |
| Hardcoded status colors in QML | Semantic derived colors from ThemeService | This phase | Overlays and status indicators adapt to theme |
| Static theme only | Static + Connected Device (live AA palette) | This phase | HU chrome can match phone's Material You |

## Open Questions

1. **UiConfigRequest direction for Connected Device theme**
   - What we know: 0x8011 is HU->Phone (we send tokens TO phone). Response 0x8012 is status only (accepted/rejected), not colors.
   - What's unclear: How does the phone communicate ITS palette to the HU? The Connected Device concept assumes receiving colors from the phone, but the wire protocol shows HU pushing to phone.
   - Recommendation: Implement UiConfigRequest sending (HU->Phone) for all themes. For Connected Device, may need to investigate if there's a phone->HU color message we haven't identified, or if the feature should be repositioned as "HU pushes its theme to the phone's projected UI" instead.

2. **Debug colors in AndroidAutoMenu.qml**
   - What we know: Touch crosshair and signal quality indicators use fixed colors (#00FF00, #FFA500, etc.)
   - What's unclear: Should these be themed or left as debug constants?
   - Recommendation: Leave debug overlay colors as-is (they're intentionally high-contrast for debugging). Signal quality colors (grey/orange/yellow/green) are semantic status indicators -- could map to theme tokens but the graduated scale doesn't fit neatly into the 4 status tokens.

3. **Web config panel base.html CSS**
   - What we know: base.html has ~15 hardcoded CSS colors matching the old default theme
   - What's unclear: Should the web panel dynamically theme itself from the Qt app's current theme?
   - Recommendation: Update web panel's colorKeys array for the theme editor page. Leave base.html CSS static -- it's a separate web UI, not worth the complexity of dynamic theming for v0.5.1.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Qt Test (QTest) 6.4+ |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd build && ctest -R test_theme_service --output-on-failure` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| THM-01 | ThemeService exposes 16 named semantic color roles as Q_PROPERTYs | unit | `ctest -R test_theme_service --output-on-failure` | Exists but needs update |
| THM-01 | Derived colors (scrim, pressed, success) computed correctly | unit | `ctest -R test_theme_service --output-on-failure` | Wave 0 |
| THM-02 | All 4+1 bundled themes load with valid colors for all 16 tokens | unit | `ctest -R test_theme_service --output-on-failure` | Wave 0 |
| THM-02 | Day/night mode switching produces correct color values | unit | `ctest -R test_theme_service --output-on-failure` | Exists but needs update |
| THM-03 | No hardcoded hex values in QML files (grep check) | smoke | `grep -rn '#[0-9a-fA-F]\{6,8\}"' qml/ --include='*.qml' \| grep -v '// debug' \| wc -l` | Wave 0 |
| THM-03 | IPC get_theme returns new token names | unit | `ctest -R test_ipc --output-on-failure` | No IPC theme test exists |

### Sampling Rate
- **Per task commit:** `cd build && ctest -R test_theme_service --output-on-failure`
- **Per wave merge:** `cd build && ctest --output-on-failure`
- **Phase gate:** Full suite green + grep confirms zero QML hardcodes

### Wave 0 Gaps
- [ ] Update `tests/data/themes/default/theme.yaml` (and any other test fixtures) to use new AA token key names
- [ ] Update `tests/test_theme_service.cpp` -- rename accessor calls (`backgroundColor()` -> `background()`, etc.)
- [ ] Add test cases for derived colors (scrim, pressed, success return expected values)
- [ ] Add test case verifying all 16 base tokens present in each bundled theme

## Sources

### Primary (HIGH confidence)
- `src/core/services/ThemeService.hpp/cpp` -- current 16-property implementation, activeColor pattern, YAML loading
- `libs/prodigy-oaa-protocol/proto/oaa/video/UiConfigRequestMessage.proto` -- gold confidence wire format (0x8011)
- `libs/prodigy-oaa-protocol/proto/oaa/video/UpdateHuUiConfigResponse.proto` -- gold confidence response (0x8012, ThemingTokensStatus enum)
- `config/themes/*/theme.yaml` -- all 4 existing theme files with current key names
- QML source files (34 files with ThemeService refs, 12+ with hardcoded hex values)

### Secondary (MEDIUM confidence)
- Qt documentation: `QColor::fromRgba()` accepts ARGB uint32 format -- verified against Qt 6 API docs
- Material Design 3 token naming conventions -- the 16 AA wire tokens follow MD3 dynamic color system

### Tertiary (LOW confidence)
- Connected Device theme concept assumes phone pushes colors to HU -- but proto audit shows 0x8011 is HU->Phone. May need further protocol investigation.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - no new dependencies, all existing code examined
- Architecture: HIGH - property rename is mechanical, mapping table is clear
- Pitfalls: HIGH - tested codebase with comprehensive existing tests, known QML binding behavior
- Connected Device theme: MEDIUM - wire protocol direction needs clarification for color reception

**Research date:** 2026-03-08
**Valid until:** 2026-04-08 (stable domain, no fast-moving dependencies)
