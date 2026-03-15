# Phase 14: Color Audit & M3 Expressive Tokens - Context

**Gathered:** 2026-03-15
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix on-* token pairings, add NavbarControl active/pressed/hold state model, apply bolder accent colors to interactive controls following a "colored islands on calm surfaces" principle, add dark-mode comfort guardrail, centralize popup surface tint, and document a capability-based state matrix for all control types.

</domain>

<decisions>
## Implementation Decisions

### Expression model: colored islands on calm surfaces
- **Interactive surfaces users touch** (navbar active, selected tiles, toggles, chips) get container fills (primaryContainer, secondaryContainer, tertiaryContainer)
- **Large structural backgrounds** stay in surfaceContainer* family — no glowing clown car
- **Text and icons on containers** follow matching on-*Container tokens — no custom contrast choices
- Dark mode goal: "colored islands on calm surfaces, not everything is saturated"

### Navbar active state (CA-02)
- Hold-progress fill: `primaryContainer` (was `tertiary`) — previews the activated look
- Active/pressed background: `primaryContainer`
- Active/pressed icon color: `onPrimaryContainer` (was `barFg`/`onSurface`)
- Rest state: unchanged (`surfaceContainer` bg, `onSurface` icons via `barFg`)
- Dark mode: use normal `primaryContainer` — global comfort guardrail catches outliers, no navbar-specific variant
- Page dots (lines 103/120/136/161): these are `onSurfaceVariant` — correct, they're page indicators not control icons

### Capability-based state matrix
- **Action controls** (buttons, tappable rows): rest, pressed, disabled
- **Selection controls** (toggles, pickers, theme selector): rest, pressed, selected, disabled
- **Gesture controls** (navbar, hold-enabled items): rest, pressed, selected (if applicable), disabled, hold-progress
- **Passive content** (labels, dividers, status text): rest only
- Each state maps to: container token, content token, optional border token, opacity/elevation treatment
- Phase 14 deliverable: token map document. Shared QML helper deferred to future phase if controls keep drifting.

### Accent boldness (CA-03)
- **Navbar active:** primaryContainer fill + onPrimaryContainer content
- **Widget card headers/key data:** primary text accents on neutral surfaceContainer cards
- **Settings toggles (active):** primaryContainer
- **Settings rows:** stay surfaceContainer (neutral) — selected state gets left-edge accent bar or subtle outline, NOT full fill
- **Tiles/cards:** full primaryContainer fill for selected/active state
- **Launcher dock:** stay neutral
- **AA overlay:** stay neutral

### Widget accent rules
- Cards stay neutral (surfaceContainer glass) — accent via text/icons only
- One accent family per widget in default state (e.g. all `primary`, don't mix `primary` + `secondary`)
- Multiple accent elements OK within same role (icon + value + progress can all be `primary`)
- Supporting text and secondary controls stay neutral (`onSurface`/`onSurfaceVariant`)
- `primary` for default accent use (icons, key values, progress, transport controls)
- `primaryContainer` reserved for state changes only (selected, editing, expanded, alerted)
- Exception: active alert / connected / strong state can temporarily use container fill

### Selection treatment (mixed model)
- **Settings rows:** border treatment (left accent bar, subtle outline, or trailing check) on neutral surface
- **Tiles/cards/widgets:** full container fill — they're isolated islands that can carry theme personality

### Semantic status colors
- Semantic always wins over theme accent for status indicators
- Connected = green (not primary), warning = amber (not tertiary), error = red (error token)
- Do NOT let tertiary masquerade as warning — define real semantic token paths for warning/degraded
- Theme accents only for non-status decorative use
- Define explicit handling for: success, warning, degraded, connected, destructive

### Night mode comfort guardrail
- Comfort wins over expression in dark mode
- Only clamp high-exposure accent roles (primary, primaryContainer, secondary, secondaryContainer), not the whole palette
- Preserve hue identity, reduce chroma/saturation to a comfortable maximum
- Do NOT re-theme or second-guess the entire companion palette
- Subtle enough that user still recognizes their chosen theme
- Guardrail scope: list exactly which roles can be softened

### Popup surface tint
- Current hand-rolled `surfaceContainerHigh * 0.88 + primary * 0.12` in PairingDialog/NavbarPopup is not wrong but is fragile
- Centralize into ThemeService as a computed property (e.g. `surfaceTinted` or `elevatedSurface`)
- One rule for all tinted overlays — popup, dialog, and elevated surfaces all use the same token
- Fallback option if this scope-creeps: just use `surfaceContainerHigh` directly and revisit tinting later

### Token pairing audit (CA-01)
- Fix all confirmed mismatches where background/foreground tokens don't pair correctly per M3 rules
- Biggest issue is missing state model on interactive controls, not widespread token misuse
- Widget text on neutral cards (`onSurface`/`onSurfaceVariant`) is correct
- NowPlayingWidget/NavigationWidget `primary` accents are intentional and correct on neutral cards
- PairingDialog/NavbarPopup tinting is ad-hoc but not broken — centralize per popup tint decision

### Claude's Discretion
- Exact chroma threshold values for the dark-mode comfort guardrail
- Whether to use HSL saturation clamp or HCT chroma clamp
- Exact computed property name for centralized surface tint
- Animation/transition details for state changes
- Whether warning/degraded semantic colors need new ThemeService Q_PROPERTYs or can use existing error + custom constants

</decisions>

<specifics>
## Specific Ideas

- "Colored islands on calm surfaces" — the core visual principle
- "Don't turn dense settings screens into a bag of Skittles"
- "Accent the headline signal, not the whole damn widget"
- State model audit should document which controls are action/selection/gesture/passive before changing any colors
- Dark-mode guardrail: list exactly which roles can be softened in night mode

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- ThemeService already has full 34-role M3 token set via Q_PROPERTY
- `surfaceContainer`, `surfaceContainerHigh`, `surfaceContainerLow`, `surfaceDim`, `surfaceBright` all available
- `primaryContainer`/`onPrimaryContainer` and sibling pairs ready to use
- WidgetHost glass card background already uses `surfaceContainer` — correct base

### Established Patterns
- NavbarControl uses `navbar.barBg`/`navbar.barFg` aliases (Navbar.qml lines 15-16)
- AA-active mode overrides to hardcoded black/white (intentional — AA owns display)
- Settings rows use `SettingsRow` + `SettingsHoldArea` + content controls pattern
- Widget cards rendered by WidgetHost with configurable opacity

### Integration Points
- NavbarControl.qml: hold-progress fill (line 49), icon color (line 60)
- Navbar.qml: barBg/barFg properties (lines 15-16), popup button colors (lines 314-315)
- PairingDialog.qml / NavbarPopup.qml: inline tint math (lines 31-33 / 83-85) → centralize
- ThemeService.hpp/cpp: add surfaceTinted computed property, add chroma guardrail logic
- All widget QML files: audit accent usage against the one-family-per-widget rule

### Confirmed Mismatches
- NavbarControl hold-progress: `tertiary` → `primaryContainer`
- NavbarControl: no selected/pressed visual model at all
- PairingDialog/NavbarPopup: inline tint math should be centralized

</code_context>

<deferred>
## Deferred Ideas

- Shared QML StateStyle component — document token map first, extract helper in future phase if controls drift
- Widget state-change container fills (active alert, edit mode) — define the rule now, implement when widgets gain those states
- Phase 15 (Color Boldness Slider) handles user-adjustable saturation — separate from the comfort guardrail

</deferred>

---

*Phase: 14-color-audit-m3-expressive-tokens*
*Context gathered: 2026-03-15*
