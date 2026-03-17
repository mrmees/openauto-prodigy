# Phase 21: Clock Styles & Weather - Context

**Gathered:** 2026-03-17
**Status:** Ready for planning

<domain>
## Phase Boundary

Enhance the existing clock widget with multiple visual styles selectable per instance, and add a new weather widget that fetches live current conditions from Open-Meteo using companion GPS coordinates. Weather alerts and manual city entry are deferred to future phases.

**Requirement adjustments from discussion:**
- WX-03 (weather alerts): **Deferred** — Open-Meteo alert coverage is limited. Ship current conditions only for v1.
- WX-05 (API key in YAML): **Not applicable** — Open-Meteo requires no API key. Skip dead config.
- Manual city location: **Deferred** — Requires a new String ConfigSchemaField type not yet in the config system. GPS-only for v1.

</domain>

<decisions>
## Implementation Decisions

### Clock visual styles (CK-01, CK-02)
- **Three styles:** digital (current behavior), analog (drawn clock face), minimal (time-only large thin font)
- **Digital:** Current clock behavior unchanged — time, date at 2x1+, day at 2x2+
- **Analog:** Canvas-rendered round face with hour/minute/second hands and tick marks at hours. Date text below face at 2x2+. **Minimum size 2x2** — analog at 1x1 is unreadable
- **Minimal:** Time only in oversized thin/light font, no date, no day even at larger sizes. Maximum glanceability. Fills the widget.
- **Style selection:** Per-instance config via config schema (enum field), alongside existing time format field
- **Colors:** All styles use same theme colors uniformly — onSurface for primary elements, onSurfaceVariant for secondary, surfaceContainer for analog face background. No per-style accent colors.

### Weather API provider (WX-01)
- **Open-Meteo** — free, no API key, global coverage, zero setup friction
- No API key config field needed (skip WX-05)
- Alerts deferred (skip WX-03) — Open-Meteo alert coverage insufficient for v1

### Weather display (WX-02)
- **1x1:** Temperature only, large text filling widget
- **2x1/2x2:** Temperature + condition icon (MaterialIcon weather symbols)
- **3x3+:** Extended info — condition text, feels-like, humidity, wind, location name
- Realistic cell sizes on 1024x600 display — don't overpack small widgets
- **No location state:** Location-off MaterialIcon + "--°" dashes. Unambiguous location issue indicator.

### Weather location
- **GPS-only for v1** — weather widget uses CompanionService.gpsLat/gpsLon exclusively
- Manual city entry deferred (requires new String ConfigSchemaField type — future phase)
- Per-instance config fields: temperature unit (°F / °C), refresh interval
- When companion disconnected or GPS unavailable: show "--°" with location-off icon

### Weather temperature units
- **Per-instance config** — each widget can independently show °F or °C
- Enum field in config schema

### Weather refresh behavior
- **Default: 5 minutes** — driving covers significant distance in 5 min, weather changes with location
- Config options: 5m / 15m / 30m / 60m
- Pause timer when widget's page isn't active (isCurrentPage=false)
- Resume + immediate fetch on page return if data is stale (older than interval)
- Immediate fetch on app start

### Claude's Discretion
- Analog clock canvas rendering details (tick mark style, hand proportions, center dot)
- MaterialIcon codepoints for weather conditions
- Exact QML layout for weather at each breakpoint
- Open-Meteo API endpoint selection and response parsing
- GPS coordinate rounding threshold for cache stability
- WeatherData object lifecycle and cleanup when no widgets reference a location
- How city name geocoding works (Open-Meteo has a geocoding API)

### Not At Claude's Discretion
- Three clock styles exactly: digital, analog, minimal (not flip clock, binary, word)
- Analog minimum 2x2 (not 1x1)
- Minimal = time only, no date at any size
- Same theme colors across all clock styles (no per-style accents)
- Open-Meteo as the API provider (no API key)
- No alerts in v1 (deferred)
- 1x1 weather = temp only (not icon + temp)
- 3x3+ for extended weather info (not 2x1)
- GPS-only location for v1 (manual city deferred — needs String config field type)
- Per-instance temperature unit (not global)
- 5-minute default refresh (not 30)
- Pause refresh when page hidden
- C++ WeatherService (not QML-side fetch)
- Widget requests WeatherData from service (not injected via WidgetContextFactory)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Widget contract and config system
- `src/core/widget/WidgetTypes.hpp` — WidgetDescriptor, ConfigSchemaField, configSchema, defaultConfig
- `src/ui/WidgetGridModel.hpp` + `.cpp` — Placement model, config roles, setWidgetConfig
- `src/ui/WidgetInstanceContext.hpp` + `.cpp` — QML context injection (effectiveConfig, isCurrentPage)
- `src/ui/WidgetContextFactory.hpp` + `.cpp` — Creates WidgetInstanceContext per placement

### Existing clock widget
- `qml/widgets/ClockWidget.qml` — Current digital clock implementation. This file will be extended with style switching.
- `src/main.cpp` lines 484-502 — Clock descriptor registration with configSchema pattern (format enum)

### Services consumed
- `src/core/services/ThemeService.hpp` — availableThemes, setTheme, currentThemeId (Q_PROPERTY added in Phase 20), color properties for widget theming
- `src/core/services/CompanionListenerService.hpp` — gpsLat, gpsLon Q_PROPERTYs for weather location. Exposed as `CompanionService` root context property.

### Existing widget examples
- `qml/widgets/AAStatusWidget.qml` — Connection state display pattern, ActionRegistry usage
- `qml/widgets/BatteryWidget.qml` — Null-safe CompanionService binding pattern (Phase 20)
- `qml/widgets/NowPlayingWidget.qml` — Service data binding via widgetContext providers

### Widget config sheet
- `qml/components/WidgetConfigSheet.qml` — Schema-driven config UI (Phase 19). Weather widget config fields rendered here.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `ClockWidget.qml` — Current digital clock, already has effectiveConfig binding for time format. Style switching adds another config field and conditional rendering.
- `ConfigSchemaField` — Enum, Bool, IntRange types ready to use. Weather needs Enum (unit, refresh interval). No new field types required (manual city deferred).
- `WidgetInstanceContext.isCurrentPage` — Already exposed for page visibility. Weather widget uses this to pause/resume refresh.
- `CompanionService.gpsLat/gpsLon` — GPS coordinates already exposed as Q_PROPERTYs with NOTIFY signals.

### QML API Gaps
- **No WeatherService exists** — New C++ service needed. Follows the pattern of ThemeService/AudioService but with HTTP networking (QNetworkAccessManager).
- **No QML Canvas usage in codebase** — Analog clock face will be the first Canvas-rendered widget. Qt Quick Canvas2D is available in Qt 6.8.

### Established Patterns
- Widget registration in `main.cpp` with configSchema fields
- Per-instance config via effectiveConfig binding in QML
- Singleton services exposed as root context properties
- UiMetrics for all sizing, ThemeService for all colors, MaterialIcon for icons

### Integration Points
- `ClockWidget.qml` — Add style switching (Loader or conditional components based on effectiveConfig.style)
- `src/main.cpp` — Update clock descriptor configSchema with style enum. Register weather widget descriptor with config fields.
- New `src/core/services/WeatherService.hpp/.cpp` — HTTP fetch, per-location cache, WeatherData QObjects
- New `qml/widgets/WeatherWidget.qml` — Binds to WeatherData from WeatherService
- `src/CMakeLists.txt` — WeatherService source files + QML files

</code_context>

<specifics>
## Specific Ideas

- Weather widget architecture: WeatherService owns QNetworkAccessManager, per-location cache keyed by stable location ID. WeatherData is a QObject per location with Q_PROPERTYs (temp, condition, humidity, wind, location, loading, error, lastUpdated). Widget calls `WeatherService.weatherForRequest({lat, lon})` to get/create a shared WeatherData object. Multiple widgets with same effective location share one WeatherData.
- GPS coordinate rounding: Use a movement threshold (e.g., round to ~0.01° ≈ 1km) to prevent tiny GPS changes from exploding the cache with unique location keys.
- Analog clock should feel like a clean, modern clock face — not overly ornate or skeuomorphic. Think car gauge aesthetic.

</specifics>

<deferred>
## Deferred Ideas

- Weather alerts (WX-03) — add when a better alert source is available or Open-Meteo improves coverage
- Manual city location — requires new String ConfigSchemaField type + config sheet text input
- API key support / alternate providers — add if users need OWM or NWS
- Multi-day forecast widget — current conditions only for v1
- Flip clock / binary clock / word clock styles — could be added as additional styles later

</deferred>

---

*Phase: 21-clock-styles-weather*
*Context gathered: 2026-03-17*
