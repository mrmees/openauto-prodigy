# State Matrix: Control Token Assignments

Single source of truth for which M3 tokens each control type uses in each visual state. All controls follow the **"colored islands on calm surfaces"** principle: interactive surfaces users touch get container fills, structural backgrounds stay in the surfaceContainer family.

## Gesture Controls

### NavbarControl

| State | Container | Content (Icon/Text) | Notes |
|-------|-----------|---------------------|-------|
| Rest | surfaceContainer (via barBg) | onSurface (via barFg) | Calm neutral surface |
| Pressed (quick tap) | primaryContainer (solid fill) | onPrimaryContainer | Instant full-fill feedback |
| Hold-progress | primaryContainer (solid bottom-up fill) | onPrimaryContainer | Animates height with _holdProgress |
| AA active (rest) | #000000 | #FFFFFF | Hardcoded -- AA owns display, blends with status bar |
| AA active (any interaction) | #000000 | #FFFFFF | No accent colors during AA projection |

## Action Controls

### ElevatedButton

| State | Container | Content (Text/Icon) | Notes |
|-------|-----------|---------------------|-------|
| Rest | surfaceContainerLow | barFg (context-dependent) | Default neutral button |
| Pressed | primaryContainer | onPrimaryContainer | Bold accent on interaction |
| Disabled | surfaceContainerLow at 0.5 opacity | barFg at 0.5 opacity | Dimmed but visible |

### FilledButton

| State | Container | Content | Notes |
|-------|-----------|---------|-------|
| Rest | buttonColor (caller-defined, e.g. error, success) | textColor (caller-defined) | Semantic color via props |
| Pressed | pressedColor (caller-defined) | pressedTextColor | Usually Qt.darker() of buttonColor |

### OutlinedButton

| State | Container | Content | Border | Notes |
|-------|-----------|---------|--------|-------|
| Rest | transparent | onSurface | outlineVariant | Minimal emphasis |
| Pressed | surfaceContainerLow | onSurface | outlineVariant | Subtle fill on press |

### Power Menu Buttons (Navbar)

| State | Container | Content | Notes |
|-------|-----------|---------|-------|
| Rest | surfaceContainerLow | barFg | Same as ElevatedButton |
| Pressed | primaryContainer | onPrimaryContainer | Correct on-* pairing |
| AA active (rest) | #1A1A1A | #FFFFFF | Dark mode forced |
| AA active (pressed) | #333333 | #FFFFFF | Subtle lift in dark |

## Selection Controls

### SettingsToggle (Switch)

| State | Track | Thumb | Notes |
|-------|-------|-------|-------|
| Checked (active) | primaryContainer (via Material.accent) | white (Qt Material default) | Bold accent for ON state |
| Unchecked | default Material styling | default Material styling | Neutral for OFF state |

All Switches -- whether via SettingsToggle component or declared inline in settings pages -- use `Material.accent: ThemeService.primaryContainer` for consistent active styling. Qt's Material style maps `Material.accent` to the checked track color and provides a white thumb automatically; these are not explicitly set by the app.

### Tile (Launcher)

| State | Container | Content (Icon/Text) | Notes |
|-------|-----------|---------------------|-------|
| Rest | primary | onPrimary | Bold accent island |
| Pressed | primaryContainer | onPrimaryContainer | Lighter accent on press |
| Disabled | primary at 0.5 opacity | onPrimary | Dimmed |

### Segmented Button (e.g. HW/SW decoder toggle)

| State | Container | Content | Notes |
|-------|-----------|---------|-------|
| Selected | secondaryContainer | onSecondaryContainer | Secondary family for selection |
| Unselected | surfaceContainerLow | onSurface | Neutral |

## Popup/Dialog Surfaces

| Surface | Color | Notes |
|---------|-------|-------|
| PairingDialog panel | surfaceTintHigh (opaque) | Level 3 elevation, 88/12 blend computed in ThemeService |
| ExitDialog panel | surfaceTintHigh (opaque) | Same blend, centralized |
| NavbarPopup panel | surfaceTintHigh (opaque), #1A1A1A when AA active | AA guard preserved |
| GestureOverlay panel | surfaceTintHighest at 0.87 alpha | Highest elevation, semi-transparent; alpha stays in QML |
| Power menu bg | surfaceContainerHigh | Standard elevated surface |

## Passive Content

### Settings Rows

| Element | Color | Notes |
|---------|-------|-------|
| Row background | surfaceContainer / surfaceContainerHigh (alternating by row index) | Calm neutral -- NO primaryContainer fill ("don't Skittle the settings") |
| Label text | onSurface | Standard content color |
| Secondary text | onSurfaceVariant | De-emphasized |
| Dividers | outlineVariant | Subtle separation |

### NormalText / SpecialText

| Element | Color | Notes |
|---------|-------|-------|
| NormalText | onSurface | Standard body text |
| SpecialText | primary | Accent emphasis text |

### Page Dots (NavbarControl clock)

| Element | Color | Notes |
|---------|-------|-------|
| Dot | onSurfaceVariant at 0.4 opacity | Subtle indicator, not interactive |

## Widget Cards

| Element | Color | Notes |
|---------|-------|-------|
| Card background | surfaceContainer glass (from WidgetHost, default 25% opacity) | Semi-transparent neutral |
| Accent values/icons | primary | One accent family per widget |
| Labels | onSurface / onSurfaceVariant | Neutral text |
| State changes | primaryContainer reserved | For future interactive widget states |

### AAStatusWidget

| State | Icon Color | Notes |
|-------|-----------|-------|
| Connected | success (#4CAF50) | Semantic green -- NOT primary accent |
| Disconnected | onSurfaceVariant | Neutral de-emphasized |

## Semantic Status Colors

Semantic colors always win over theme accent for status indicators. These are NOT affected by the night comfort guardrail (only accent roles are clamped).

| Token | Value | Usage |
|-------|-------|-------|
| success | #4CAF50 (green) | Connected states, active routes, confirmation |
| onSuccess | #FFFFFF | Content on success backgrounds |
| warning | #FF9800 (amber) | Degraded states, retrying indicators |
| onWarning | #FFFFFF | Content on warning backgrounds |
| error | M3 error token | Failed states, destructive actions |
| onError | M3 onError token | Content on error backgrounds |

**Key rule:** CompanionSettings degraded route status uses `warning` token, NOT `tertiary`. Tertiary is a theme accent color, not a semantic status indicator.

## Expression Principle

**"Colored islands on calm surfaces"**

- Interactive surfaces that users touch (tiles, navbar controls, toggle tracks) get bold accent container fills
- Structural backgrounds (settings rows, card hosts, page backgrounds) stay in the surfaceContainer family
- Status indicators use semantic colors (success/warning/error), never theme accents
- Night mode guardrail clamps accent HSL saturation to 0.55 for in-car comfort

## Widget Accent Rules

- One accent family per widget (default: primary)
- primaryContainer reserved for state changes (future: interactive widget states)
- Accent applied to text/icons only, not card backgrounds (glass cards stay neutral)

## Deferred Items

- **Shared StateStyle QML component:** If controls drift from this matrix, extract a declarative style object that maps states to tokens. Not needed yet -- current control count is manageable.
- **Widget interactive states:** When widgets gain tappable actions, use primaryContainer for pressed states (matching Tile pattern).
