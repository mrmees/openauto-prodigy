---
phase: 21-clock-styles-weather
plan: 02
subsystem: ui
tags: [weather, open-meteo, qml, qnetworkaccessmanager, widget, gps]

# Dependency graph
requires:
  - phase: 19-widget-instance-config
    provides: "WidgetDescriptor with configSchema, WidgetInstanceContext with effectiveConfig"
  - phase: 20-simple-widgets
    provides: "Widget QML patterns (BatteryWidget, CompanionStatusWidget)"
provides:
  - "WeatherService C++ singleton with HTTP fetch, per-location cache, subscriber-aware refresh"
  - "WeatherData QObject with Q_PROPERTYs for all current weather fields"
  - "WeatherWidget QML with 3 responsive breakpoints (1x1, 2x1, 3x3+)"
  - "Per-instance config for temperature unit and refresh interval"
affects: []

# Tech tracking
tech-stack:
  added: [Open-Meteo API, QNetworkAccessManager]
  patterns: [subscriber-aware refresh timer, per-instance config driving service behavior]

key-files:
  created:
    - src/core/services/WeatherService.hpp
    - src/core/services/WeatherService.cpp
    - qml/widgets/WeatherWidget.qml
    - tests/test_weather_service.cpp
  modified:
    - src/main.cpp
    - src/CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "Subscriber list per location tracks individual intervals (not just count) for effective interval computation"
  - "1-minute timer tick checks all locations against their individual intervals (no per-location timers)"
  - "Cache eviction protects subscribed entries unconditionally"

patterns-established:
  - "Subscriber-aware service refresh: widgets subscribe/unsubscribe on page visibility, service skips inactive locations"
  - "Per-instance widget config driving service-level behavior via subscribe(lat, lon, interval)"

requirements-completed: [WX-01, WX-02, WX-04]

# Metrics
duration: 9min
completed: 2026-03-16
---

# Phase 21 Plan 02: Weather Service & Widget Summary

**WeatherService with Open-Meteo HTTP fetch, per-location caching, subscriber-aware refresh timer, and 3-breakpoint QML widget with per-instance temperature unit and refresh interval config**

## Performance

- **Duration:** 9 min
- **Started:** 2026-03-17T03:11:10Z
- **Completed:** 2026-03-17T03:20:50Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- WeatherService fetches current conditions from Open-Meteo API with coordinate rounding and per-location cache
- Subscriber lifecycle controls refresh timer -- only active (page-visible + GPS) locations are refreshed
- WeatherWidget displays temperature at 1x1, icon+temp at 2x1/2x2, extended info (feels-like, humidity, wind, location) at 3x3+
- 14 unit tests covering parsing, rounding, caching, subscriber lifecycle, interval recomputation, and safe eviction

## Task Commits

Each task was committed atomically:

1. **Task 1: WeatherService C++ with tests** - `f4f7f46` (feat)
2. **Task 2: Weather widget QML and descriptor registration** - `12c8a4d` (feat)

## Files Created/Modified
- `src/core/services/WeatherService.hpp` - WeatherData QObject + WeatherService singleton
- `src/core/services/WeatherService.cpp` - HTTP fetch, JSON parse, per-location cache, subscriber-aware timer
- `qml/widgets/WeatherWidget.qml` - Responsive weather display with subscribe/unsubscribe lifecycle
- `tests/test_weather_service.cpp` - 14 unit tests
- `src/main.cpp` - WeatherService creation, context property, widget descriptor registration
- `src/CMakeLists.txt` - WeatherService.cpp source, WeatherWidget.qml QML module
- `tests/CMakeLists.txt` - test_weather_service registration

## Decisions Made
- Subscriber list per location stores individual interval values (not just a count) so effective interval recomputes correctly when subscribers with different intervals add/remove
- Single 1-minute timer tick iterates all locations and checks per-location effective intervals, avoiding proliferation of per-location QTimers
- Cache eviction unconditionally protects entries with active subscribers to prevent invalidating live QML bindings

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed QNetworkRequest most-vexing-parse**
- **Found during:** Task 1 (WeatherService C++ implementation)
- **Issue:** `QNetworkRequest request(QUrl(buildUrl(...)))` parsed as function declaration, not constructor call
- **Fix:** Split into separate `QUrl url(...)` and `QNetworkRequest request(url)` statements
- **Files modified:** src/core/services/WeatherService.cpp
- **Committed in:** f4f7f46

**2. [Rule 1 - Bug] Fixed cleanup test expecting behavior inconsistent with auto-cleanup**
- **Found during:** Task 1 (test development)
- **Issue:** `getWeatherData` auto-triggers cleanup when cache exceeds MAX_CACHE_SIZE, evicting the newly created entry before `subscribe()` can protect it
- **Fix:** Restructured test to subscribe to entries before adding beyond cache limit, then verify cleanup respects subscriptions
- **Files modified:** tests/test_weather_service.cpp
- **Committed in:** f4f7f46

---

**Total deviations:** 2 auto-fixed (2 bugs)
**Impact on plan:** Both fixes necessary for correct compilation and test behavior. No scope creep.

## Issues Encountered
None beyond the auto-fixed items above.

## User Setup Required
None - no external service configuration required. Open-Meteo API requires no API key.

## Next Phase Readiness
- Weather widget fully functional with GPS from companion app
- WX-03 (weather alerts) deferred per CONTEXT.md -- Open-Meteo alert coverage insufficient
- WX-05 (API key) not applicable -- Open-Meteo is free/keyless

---
*Phase: 21-clock-styles-weather*
*Completed: 2026-03-16*
