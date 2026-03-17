# Phase 20: Simple Widgets - Context

**Gathered:** 2026-03-17
**Status:** Ready for planning

<domain>
## Phase Boundary

Four new utility widgets for the home screen: theme cycle, phone battery, companion status, and AA focus toggle. These are "simple" because they consume existing C++ services (ThemeService, CompanionListenerService, AndroidAutoOrchestrator) with no new backends, APIs, or protocol work. Each follows the formalized widget contract (span-based breakpoints, context injection, WidgetDescriptor registration).

</domain>

<decisions>
## Implementation Decisions

### Theme cycle widget (TC-01, TC-02)
- Material palette icon (`\ue40a`) + current theme name text below, centered
- Icon tinted with ThemeService.primary (widget itself is a live preview of the active theme)
- Tap advances to the next theme in `ThemeService.availableThemes`, wrapping around after the last
- Name updates immediately via ThemeService.currentThemeId binding
- No configSchema needed (no per-instance settings)

### Battery widget (BW-01)
- At 1x1: battery icon (from `batteryIconForLevel()` in StatusIndicatorHelper.hpp) + percentage text
- When companion disconnected: battery outline with slash/X icon, "--" instead of percentage, no stale data shown
- At larger sizes: icon and text scale up proportionally to fill the widget — user can make the indicator as large as they want
- Reads `CompanionListenerService.phoneBattery` and `isPhoneCharging` via WidgetInstanceContext.hostContext

### Companion status widget (CS-01, CS-02)
- Separate widget from battery (not combined)
- At 1x1: phone icon + green dot (connected) or red dot (disconnected)
- At 2x1+: expanded detail rows showing GPS status, battery, and proxy status
- Proxy status: simple "Active" / "Off" — not full ACTIVE/FAILED/DEGRADED routing state (too system-level for glanceability)
- Reads `CompanionListenerService.isConnected`, `gpsLat/Lon`, `phoneBattery`, `proxyAddress` via hostContext

### AA focus toggle widget (AF-01, AF-02, AF-03)
- Toggle with state indicator: phone icon + "AA" label when PROJECTED, car icon + "Car" label when NATIVE
- Tap calls `requestVideoFocus()` to toggle between states
- Current state read from `IProjectionStatusProvider.projectionState` and video focus mode
- When AA not connected: widget shows disabled/greyed out (dimmed icon, no tap response)
- No configSchema needed (binary toggle, no settings)

### Visual consistency
- All four widgets use consistent 1x1 pattern: centered MaterialIcon + label below
- Each uses different icon and label but same layout structure
- All use UiMetrics for sizing, ThemeService for colors — no hardcoded pixels
- Span-based breakpoints for companion status (detail rows at colSpan >= 2) and battery (proportional scaling)

### Claude's Discretion
- Exact MaterialIcon codepoints for each widget state
- Animation/transition when theme changes or focus toggles
- Layout of companion status detail rows at expanded sizes
- How charging state is visually indicated on battery icon

### Not At Claude's Discretion
- Palette icon + name for theme cycle (not swatch-only or text-only)
- Icon tinted with primary color for theme cycle
- Wrap-around on theme cycle (not stop-at-end)
- Disconnected = slash icon + "--" for battery (not stale data or hidden)
- Phone icon + colored dot for companion status at 1x1
- Simple on/off for proxy status (not full routing state)
- Toggle with state indicator for AA focus (not play/pause or text-only)
- Disabled/greyed when AA not connected (not hidden or stale state)
- Consistent centered icon+label pattern across all four widgets
- Battery scales up at larger sizes (not locked to 1x1)
- Companion status expands to detail rows at 2x1+ (not locked to 1x1)
- Separate battery and companion status widgets (not combined)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Widget contract
- `src/core/widget/WidgetTypes.hpp` — WidgetDescriptor, GridPlacement, ConfigSchemaField, configSchema, defaultConfig
- `src/ui/WidgetGridModel.hpp` + `.cpp` — Placement model, config roles, setWidgetConfig
- `src/ui/WidgetInstanceContext.hpp` + `.cpp` — QML context injection (effectiveConfig, providers via hostContext)
- `src/ui/WidgetContextFactory.hpp` + `.cpp` — Creates WidgetInstanceContext, tracks live contexts per instanceId
- `src/main.cpp` — Widget descriptor registration with WidgetRegistry (see clock widget for configSchema pattern)

### Services consumed
- `src/core/services/ThemeService.hpp` — `availableThemes`, `availableThemeNames`, `setTheme()`, `currentThemeId()`, color Q_PROPERTYs
- `src/core/services/CompanionListenerService.hpp` — `isConnected`, `phoneBattery`, `isPhoneCharging`, `gpsLat/Lon`, `proxyAddress`, NOTIFY signals
- `src/core/aa/AndroidAutoOrchestrator.hpp` — `requestVideoFocus()`, video focus states (1=PROJECTED, 2=NATIVE)
- `src/core/services/IProjectionStatusProvider.hpp` — `projectionState` enum (Disconnected/Connecting/Negotiating/Connected)
- `src/core/aa/StatusIndicatorHelper.hpp` — `batteryIconForLevel()` helper

### Existing widget examples
- `qml/widgets/ClockWidget.qml` — Span-based breakpoints, effectiveConfig reading, context injection pattern
- `qml/widgets/AAStatusWidget.qml` — IProjectionStatusProvider consumption, connection state display
- `qml/widgets/NowPlayingWidget.qml` — Service data binding via widgetContext providers

### Edit mode
- `qml/applications/home/HomeMenu.qml` — Widget delegate, edit mode overlays, gear icon pattern

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `ThemeService` (context property): `availableThemes` QStringList + `availableThemeNames` parallel list + `setTheme(id)` — theme cycle widget can call directly from QML
- `CompanionListenerService` (via hostContext): battery/charging/connected/GPS/proxy all exposed as Q_PROPERTYs with NOTIFY signals — battery and companion widgets bind directly
- `batteryIconForLevel()` in StatusIndicatorHelper.hpp: maps 0-100 to Material Symbols battery codepoints — battery widget reuses this
- `requestVideoFocus()` on AndroidAutoOrchestrator: toggles NATIVE/PROJECTED — AA focus widget calls this
- `IProjectionStatusProvider.projectionState`: enum for connection state — AA focus widget uses for disabled state detection
- `WidgetInstanceContext`: provides `hostContext` accessor → all services reachable from QML widgets

### Established Patterns
- Widget registration in `main.cpp` populates WidgetDescriptor at compile time
- Context injection via `WidgetContextFactory.createContext()` + Binding in delegate
- Span-based breakpoints: `readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1`
- MaterialIcon component for icon display with codepoints
- UiMetrics for all sizing, ThemeService for all colors

### Integration Points
- `main.cpp`: register 4 new WidgetDescriptors (theme_cycle, battery, companion_status, aa_focus)
- `qml/widgets/`: 4 new QML files following ClockWidget pattern
- `src/CMakeLists.txt`: add new QML files to qt_add_qml_module
- No new C++ classes needed — all data comes from existing services via context injection
- ThemeService is already a context property (`ThemeService` in QML) — theme cycle widget can access directly without hostContext

</code_context>

<specifics>
## Specific Ideas

- "Battery should just get larger within the widget to let the user make the indicator however large they want"
- Consistent centered icon+label at 1x1 across all four new widgets — cohesive visual family
- Theme cycle widget doubles as a live color preview (icon tinted with primary)
- Companion status at 2x1+ should be useful detail, not overwhelming — simple on/off for proxy

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 20-simple-widgets*
*Context gathered: 2026-03-17*
