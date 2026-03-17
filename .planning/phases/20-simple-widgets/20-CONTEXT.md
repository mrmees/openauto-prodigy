# Phase 20: Simple Widgets - Context

**Gathered:** 2026-03-17
**Status:** Ready for planning

<domain>
## Phase Boundary

Four new utility widgets for the home screen: theme cycle, phone battery, companion status, and AA focus toggle. These are "simple" because they consume existing C++ services (ThemeService, CompanionListenerService, AndroidAutoOrchestrator) with no new backends, APIs, or protocol work. Each follows the formalized widget contract (span-based breakpoints, context injection, WidgetDescriptor registration).

**Prerequisites identified during review:** Some existing service APIs need minor extensions to be QML-bindable (see code_context section). These are small additions to existing classes, not new subsystems.

</domain>

<decisions>
## Implementation Decisions

### Theme cycle widget (TC-01, TC-02)
- Material palette icon (`\ue40a`) + current theme name text below, centered
- Icon tinted with ThemeService.primary (widget itself is a live preview of the active theme)
- Tap advances to the next theme in `ThemeService.availableThemes`, wrapping around after the last
- **Theme name reactivity:** `currentThemeId()` is currently a plain C++ getter (not a Q_PROPERTY). Phase must add a `Q_PROPERTY(QString currentThemeId READ currentThemeId NOTIFY currentThemeIdChanged)` to ThemeService and emit it from `setTheme()`. Alternatively, derive the display name from `availableThemeNames[availableThemes.indexOf(currentThemeId())]` and re-evaluate on `colorsChanged` (which already fires on theme switch).
- No configSchema needed (no per-instance settings)

### Battery widget (BW-01)
- At 1x1: battery icon (from `batteryIconForLevel()` in StatusIndicatorHelper.hpp) + percentage text
- When companion disconnected: battery outline with slash/X icon, "--" instead of percentage, no stale data shown
- At larger sizes: icon and text scale up proportionally to fill the widget — user can make the indicator as large as they want
- **Data access:** `CompanionListenerService` is exposed as `CompanionService` root context property in `src/main.cpp:805` — battery widget binds to `CompanionService.phoneBattery` and `CompanionService.connected` directly. NOT via `widgetContext.hostContext` (that's a C++-only getter, not QML-accessible).
- `batteryIconForLevel()` is a C++ inline function — either expose as Q_INVOKABLE on a helper object, or reimplement the icon lookup in QML (it's a simple level→codepoint map).

### Companion status widget (CS-01, CS-02)
- Separate widget from battery (not combined)
- At 1x1: phone icon + green dot (connected) or red dot (disconnected)
- At 2x1+: expanded detail rows showing GPS status, battery, and proxy status
- Proxy status: simple "Active" / "Off" — not full ACTIVE/FAILED/DEGRADED routing state (too system-level for glanceability)
- **Data access:** Same as battery — binds to `CompanionService` root context property (`CompanionService.connected`, `CompanionService.gpsLat`, `CompanionService.gpsLon`, `CompanionService.phoneBattery`, `CompanionService.proxyAddress`)

### AA focus toggle widget (AF-01, AF-02, AF-03)
- Toggle with state indicator: phone icon + "AA" label when PROJECTED, car icon + "Car" label when NATIVE (Backgrounded)
- **Command path:** Widget dispatches via `ActionRegistry.dispatch()` (Q_INVOKABLE, already used by other widgets). Phase must register two new actions in `main.cpp`: `aa.requestFocus` (calls `orch->requestVideoFocus()`, works when Backgrounded) and `aa.exitToCar` (calls `orch->requestExitToCar()`, works when Connected). Widget calls the appropriate action based on current state.
- **State reading:** Widget reads `projectionStatus.projectionState` via `widgetContext.projectionStatus` (already exposed as Q_PROPERTY on WidgetInstanceContext). The `IProjectionStatusProvider.ProjectionState` enum includes `Backgrounded` (value 4) — this is the NATIVE state. `Connected` (value 3) is the PROJECTED state. Widget toggles between these two.
- When AA not connected (projectionState < Connected): widget shows disabled/greyed out (dimmed icon, no tap response)
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
- Whether to add currentThemeId Q_PROPERTY vs derive name from availableThemes + colorsChanged

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
- `src/ui/WidgetInstanceContext.hpp` + `.cpp` — QML context injection (effectiveConfig, projectionStatus, navigationProvider, mediaStatus). NOTE: `hostContext()` is C++-only, NOT QML-accessible.
- `src/ui/WidgetContextFactory.hpp` + `.cpp` — Creates WidgetInstanceContext, tracks live contexts per instanceId
- `src/main.cpp` — Widget descriptor registration with WidgetRegistry (see clock widget for configSchema pattern)

### Services consumed
- `src/core/services/ThemeService.hpp` — `availableThemes` (Q_PROPERTY), `availableThemeNames` (Q_PROPERTY), `setTheme()` (Q_INVOKABLE), `currentThemeId()` (plain getter — NOT a Q_PROPERTY, no NOTIFY signal). `colorsChanged` signal fires on theme switch.
- `src/core/services/CompanionListenerService.hpp` — `connected` (Q_PROPERTY), `phoneBattery` (Q_PROPERTY), `phoneCharging` (Q_PROPERTY), `gpsLat/Lon` (Q_PROPERTY), `proxyAddress` (Q_PROPERTY). Exposed to QML as `CompanionService` context property in `src/main.cpp:805`.
- `src/core/aa/AndroidAutoOrchestrator.hpp` — `requestVideoFocus()` (restores from Backgrounded→Connected), `requestExitToCar()` (exits from Connected→Backgrounded). Neither is directly QML-accessible — must be wired through ActionRegistry.
- `src/core/services/IProjectionStatusProvider.hpp` — `projectionState` Q_PROPERTY with enum: Disconnected(0), WaitingForDevice(1), Connecting(2), Connected(3), Backgrounded(4). Exposed to widget QML via `widgetContext.projectionStatus`.
- `src/core/services/ActionRegistry.hpp` — `dispatch()` is Q_INVOKABLE. Already used by existing widgets (AAStatusWidget, NavigationWidget, etc.). Exposed as `ActionRegistry` context property.
- `src/core/aa/StatusIndicatorHelper.hpp` — `batteryIconForLevel()` inline helper (C++ only, not QML-callable)

### Existing widget examples
- `qml/widgets/ClockWidget.qml` — Span-based breakpoints, effectiveConfig reading, context injection pattern
- `qml/widgets/AAStatusWidget.qml` — IProjectionStatusProvider consumption, connection state display, ActionRegistry.dispatch usage
- `qml/widgets/NowPlayingWidget.qml` — Service data binding via widgetContext providers

### Edit mode
- `qml/applications/home/HomeMenu.qml` — Widget delegate, edit mode overlays, gear icon pattern

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `ThemeService` (context property `ThemeService` in QML): `availableThemes` QStringList + `availableThemeNames` parallel list + `setTheme(id)` Q_INVOKABLE — theme cycle widget can call directly from QML
- `CompanionListenerService` (context property `CompanionService` in QML): battery/charging/connected/GPS/proxy all exposed as Q_PROPERTYs with NOTIFY signals — battery and companion widgets bind directly
- `ActionRegistry` (context property `ActionRegistry` in QML): `dispatch(actionId, payload)` Q_INVOKABLE — AA focus widget uses this to command focus changes
- `IProjectionStatusProvider` (via `widgetContext.projectionStatus`): `projectionState` Q_PROPERTY — AA focus widget reads connection/focus state
- `batteryIconForLevel()` in StatusIndicatorHelper.hpp: maps 0-100 to Material Symbols battery codepoints — C++ only, needs QML equivalent or wrapper

### QML API Gaps (must be addressed in this phase)
- **ThemeService.currentThemeId**: plain C++ getter, not a Q_PROPERTY. Theme cycle widget needs either a new Q_PROPERTY with NOTIFY, or must derive the name from `availableThemes.indexOf()` + `colorsChanged` signal.
- **AA focus commands**: `requestVideoFocus()` and `requestExitToCar()` are on AndroidAutoOrchestrator, which is not QML-accessible from home widgets. Must register `aa.requestFocus` and `aa.exitToCar` actions in ActionRegistry (main.cpp).
- **batteryIconForLevel()**: inline C++ function, not QML-callable. Either reimplement as a JS function in the widget, or expose via a Q_INVOKABLE helper.

### Established Patterns
- Widget registration in `main.cpp` populates WidgetDescriptor at compile time
- Context injection via `WidgetContextFactory.createContext()` + Binding in delegate
- Span-based breakpoints: `readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1`
- MaterialIcon component for icon display with codepoints
- UiMetrics for all sizing, ThemeService for all colors
- ActionRegistry dispatch for widget commands (see AAStatusWidget, NavigationWidget)

### Integration Points
- `main.cpp`: register 4 new WidgetDescriptors + 2 new ActionRegistry actions (aa.requestFocus, aa.exitToCar)
- `qml/widgets/`: 4 new QML files following ClockWidget/AAStatusWidget patterns
- `src/CMakeLists.txt`: add new QML files to qt_add_qml_module
- `src/core/services/ThemeService.hpp`: add currentThemeId Q_PROPERTY (or planner may choose the derive-from-colorsChanged approach)
- Battery and companion widgets bind to `CompanionService` context property (NOT hostContext)
- AA focus widget uses `widgetContext.projectionStatus.projectionState` for state + `ActionRegistry.dispatch()` for commands

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
