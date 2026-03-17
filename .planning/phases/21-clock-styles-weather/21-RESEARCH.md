# Phase 21: Clock Styles & Weather - Research

**Researched:** 2026-03-17
**Domain:** QML Canvas2D rendering, Open-Meteo HTTP API, Qt Network, widget per-instance config
**Confidence:** HIGH

## Summary

This phase adds three clock visual styles (digital, analog, minimal) to the existing ClockWidget and introduces a new WeatherWidget backed by a C++ WeatherService that fetches current conditions from Open-Meteo. Both widgets use the per-instance config system built in Phase 19.

The clock work is straightforward -- the existing ClockWidget already has effectiveConfig bindings; adding a style enum and conditional rendering (Loader or inline) is well-trodden QML territory. The analog style uses QML Canvas2D, which is new to this codebase but well-supported in Qt 6.8.

The weather work is the heavier lift: a new C++ service using QNetworkAccessManager to call Open-Meteo, per-location caching with GPS coordinate rounding, and a QML widget with responsive breakpoints. Key consideration: the Pi only has internet via a SOCKS5 proxy through the companion app, so WeatherService must handle the case where internet is unavailable (companion disconnected) and should use Qt's system proxy settings which the route daemon configures.

**Primary recommendation:** Build WeatherService as a singleton C++ service exposed as a QML root context property. Use QNetworkAccessManager with Qt's default proxy (system-level routing handles SOCKS5). Cache WeatherData per rounded lat/lon. Widget binds to WeatherData Q_PROPERTYs; unit conversion happens in QML.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Three clock styles exactly: digital, analog, minimal (not flip clock, binary, word)
- Analog minimum 2x2 (not 1x1)
- Minimal = time only, no date at any size
- Same theme colors across all clock styles (no per-style accents)
- Open-Meteo as the API provider (no API key)
- No alerts in v1 (deferred)
- 1x1 weather = temp only (not icon + temp)
- 3x3+ for extended weather info (not 2x1)
- GPS-only location for v1 (manual city deferred -- needs String config field type)
- Per-instance temperature unit (not global)
- 5-minute default refresh (not 30)
- Pause refresh when page hidden
- C++ WeatherService (not QML-side fetch)
- Widget requests WeatherData from service (not injected via WidgetContextFactory)

### Claude's Discretion
- Analog clock canvas rendering details (tick mark style, hand proportions, center dot)
- MaterialIcon codepoints for weather conditions
- Exact QML layout for weather at each breakpoint
- Open-Meteo API endpoint selection and response parsing
- GPS coordinate rounding threshold for cache stability
- WeatherData object lifecycle and cleanup when no widgets reference a location

### Deferred Ideas (OUT OF SCOPE)
- Weather alerts (WX-03) -- add when a better alert source is available or Open-Meteo improves coverage
- Manual city location -- requires new String ConfigSchemaField type + config sheet text input
- API key support / alternate providers -- add if users need OWM or NWS
- Multi-day forecast widget -- current conditions only for v1
- Flip clock / binary clock / word clock styles -- could be added as additional styles later
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CK-01 | Clock widget supports at least 3 visual styles (digital, analog, minimal) | Existing ClockWidget has effectiveConfig binding; add style enum to configSchema, use Loader for style switching. Canvas2D available in Qt 6.8 for analog face. |
| CK-02 | Clock style is selectable per widget instance via the widget config UI | ConfigSchemaField Enum type already supports this. Add "style" field to clock descriptor configSchema in main.cpp. WidgetConfigSheet renders it automatically. |
| WX-01 | Pi fetches current weather conditions via HTTP API using companion GPS location | New WeatherService with QNetworkAccessManager. Open-Meteo `current` parameter on `/v1/forecast`. CompanionService.gpsLat/gpsLon already exposed. |
| WX-02 | Weather widget displays temperature, condition icon, and location name | WeatherData QObject with Q_PROPERTYs. WMO weather codes mapped to MaterialIcon codepoints. Responsive breakpoints: 1x1 temp only, 2x1+ icon+temp, 3x3+ extended. |
| WX-03 | Weather widget displays active weather alerts when present | **DEFERRED** per CONTEXT.md -- Open-Meteo alert coverage insufficient for v1. |
| WX-04 | Weather data refreshes on a configurable interval (default 30 min) | **Modified per CONTEXT.md:** Default 5 minutes, options 5/15/30/60. Pause when isCurrentPage=false. Service uses shortest requested interval. |
| WX-05 | API key is configured via YAML config (not hardcoded) | **NOT APPLICABLE** per CONTEXT.md -- Open-Meteo requires no API key. Skip dead config field. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt6::Network | 6.8.2 | HTTP requests (QNetworkAccessManager) | Already linked in CMakeLists.txt. Handles SOCKS5 proxy transparently via system settings. |
| QML Canvas2D | Qt 6.8.2 | Analog clock face rendering | Built into QtQuick. No additional dependency. |
| Open-Meteo API | v1 | Current weather conditions | Free, no API key, global coverage. No library needed -- raw HTTP GET + JSON parse. |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| QJsonDocument | Qt 6.8.2 | Parse Open-Meteo JSON responses | Already available via Qt6::Core |
| QTimer | Qt 6.8.2 | Refresh interval scheduling | Already used throughout codebase |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Open-Meteo | OpenWeatherMap | Requires API key, rate limits on free tier. Open-Meteo is locked decision. |
| QML Canvas2D | QPainter via QQuickPaintedItem | Better performance for complex/frequent redraws, but clock updates once/second -- Canvas is fine. |
| QML Canvas2D | QML Rectangle/Item composition | Simpler for hands but harder for tick marks and arcs. Canvas is more natural for clock faces. |

## Architecture Patterns

### New Files
```
src/core/services/WeatherService.hpp      # Service: HTTP fetch, cache, WeatherData lifecycle
src/core/services/WeatherService.cpp      # Implementation
qml/widgets/WeatherWidget.qml             # Weather display with breakpoints
```

### Modified Files
```
src/main.cpp                              # Clock configSchema update, weather descriptor, WeatherService creation + QML exposure
src/CMakeLists.txt                        # Add WeatherService sources
qml/widgets/ClockWidget.qml              # Style switching (Loader + 3 inline components or 3 separate files)
```

### Pattern 1: WeatherService Architecture
**What:** Singleton C++ service owning QNetworkAccessManager, per-location cache, and refresh timer.
**When to use:** Any time the app needs HTTP data shared across multiple widget instances.

```cpp
// WeatherService.hpp
class WeatherData : public QObject {
    Q_OBJECT
    Q_PROPERTY(double tempC READ tempC NOTIFY dataChanged)
    Q_PROPERTY(int weatherCode READ weatherCode NOTIFY dataChanged)
    Q_PROPERTY(double humidity READ humidity NOTIFY dataChanged)
    Q_PROPERTY(double windSpeedKph READ windSpeedKph NOTIFY dataChanged)
    Q_PROPERTY(double feelsLikeC READ feelsLikeC NOTIFY dataChanged)
    Q_PROPERTY(bool isDay READ isDay NOTIFY dataChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY dataChanged)
    Q_PROPERTY(QString error READ error NOTIFY dataChanged)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY dataChanged)
    // ...
signals:
    void dataChanged();
};

class WeatherService : public QObject {
    Q_OBJECT
public:
    // Widget calls this with GPS coords; returns cached or new WeatherData
    Q_INVOKABLE QObject* getWeatherData(double lat, double lon);
    Q_INVOKABLE void requestRefresh(double lat, double lon);
private:
    QNetworkAccessManager* nam_;
    QHash<QString, WeatherData*> cache_;  // key = rounded lat/lon string
    QTimer* refreshTimer_;
};
```

### Pattern 2: Clock Style Switching via Loader
**What:** ClockWidget reads `effectiveConfig.style` and loads the appropriate sub-component.
**When to use:** When a widget has multiple distinct visual modes.

```qml
// ClockWidget.qml
readonly property string clockStyle: currentEffectiveConfig.style || "digital"

Loader {
    anchors.fill: parent
    sourceComponent: {
        switch (clockWidget.clockStyle) {
            case "analog": return analogComponent
            case "minimal": return minimalComponent
            default: return digitalComponent
        }
    }
}

Component { id: digitalComponent; /* existing layout */ }
Component { id: analogComponent;  /* Canvas-based */ }
Component { id: minimalComponent; /* oversized thin text */ }
```

### Pattern 3: Weather Widget GPS Binding
**What:** Weather widget binds to CompanionService GPS and requests WeatherData from the service.

```qml
// WeatherWidget.qml
readonly property bool hasGps: CompanionService && CompanionService.connected
                               && !CompanionService.gpsStale
                               && CompanionService.gpsLat !== 0
readonly property QtObject weatherData: {
    if (hasGps && WeatherService)
        return WeatherService.getWeatherData(CompanionService.gpsLat, CompanionService.gpsLon)
    return null
}
```

### Anti-Patterns to Avoid
- **QML-side HTTP fetch:** Don't use XMLHttpRequest in QML. C++ WeatherService is a locked decision and provides proper caching, error handling, and proxy support.
- **Per-widget refresh timers:** Don't put QTimer in each weather widget QML. The service owns the refresh cadence; widgets bind to WeatherData properties.
- **Storing converted temperatures:** WeatherData stores metric (Celsius). Each widget converts for display based on its per-instance unit config. Don't cache both units.
- **Re-fetching on every GPS signal:** GPS updates come frequently. Round coordinates and only re-fetch when the location key actually changes (~1km threshold).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| HTTP requests | Raw sockets or libcurl | QNetworkAccessManager | Already linked, handles SOCKS5 proxy, async signals, SSL |
| JSON parsing | Manual string parsing | QJsonDocument | Standard Qt, fast, type-safe |
| Coordinate rounding | Complex geohash | Simple lat/lon truncation to 2 decimal places (~1.1km) | Good enough for weather API granularity; weather doesn't change within 1km |
| Weather code mapping | Complex lookup table | Simple switch/map in QML | Open-Meteo uses ~20 WMO codes; a JS function with a switch is clearest |
| Clock face drawing | SVG images | Canvas2D | Dynamic theming via ThemeService colors; SVG would need separate day/night assets |

## Common Pitfalls

### Pitfall 1: SOCKS5 Proxy and Internet Availability
**What goes wrong:** WeatherService tries to fetch when companion is disconnected; Pi has no internet.
**Why it happens:** The Pi is its own WiFi AP. It only reaches the internet through the companion app's SOCKS5 proxy, which the system daemon routes at the OS level.
**How to avoid:** Check `CompanionService.internetAvailable` before fetching. Queue the fetch request and retry when internet becomes available. WeatherService should connect to `CompanionService.internetChanged` signal.
**Warning signs:** QNetworkReply errors (HostNotFound, ConnectionRefused) when companion is disconnected.

### Pitfall 2: QML Canvas2D Performance
**What goes wrong:** Analog clock Canvas repaints every second, causing performance issues.
**Why it happens:** Canvas2D uploads textures to GPU on every repaint.
**How to avoid:** For a once-per-second update, Canvas2D is fine -- the Qt docs warn about "frequent updates and animation," not 1fps redraws. Use `requestPaint()` only when time changes (timer-driven, not continuous). Keep the canvas size reasonable (bounded by widget cell size, not full screen).
**Warning signs:** GPU texture upload warnings in Qt debug output.

### Pitfall 3: GPS Coordinate Thrash
**What goes wrong:** Every tiny GPS update creates a new cache key, flooding the service with fetch requests.
**Why it happens:** GPS coordinates have many decimal places; even stationary, values fluctuate.
**How to avoid:** Round to 2 decimal places (0.01 degrees, ~1.1km). Only trigger a new fetch when the rounded key differs from the current one. This is explicitly in Claude's discretion area.
**Warning signs:** Multiple WeatherData objects in cache for essentially the same location.

### Pitfall 4: QML Binding Loops with getWeatherData()
**What goes wrong:** Calling `WeatherService.getWeatherData()` in a property binding causes infinite re-evaluation.
**Why it happens:** Q_INVOKABLE call in a binding -- if the return value triggers a signal that the binding depends on, it loops.
**How to avoid:** Use a two-step pattern: bind GPS coordinates to local properties, use `onGpsChanged` handler to call the service imperatively and store the returned WeatherData in a local property. Don't call the service in a `property: expression` binding.
**Warning signs:** "QML binding loop detected" warnings, UI freeze.

### Pitfall 5: WeatherData Lifecycle / Memory Leaks
**What goes wrong:** WeatherData objects accumulate as the user drives and GPS coordinates change.
**Why it happens:** New location = new cache entry; old entries never cleaned up.
**How to avoid:** Implement a simple LRU or TTL eviction. Since the user is driving, only the current location matters. Evict entries older than the refresh interval or limit cache to ~5 entries. Use `QObject::deleteLater()` for safe cleanup.
**Warning signs:** Memory growth proportional to drive time.

### Pitfall 6: Clock Timer Running When Not Visible
**What goes wrong:** 1-second timer runs even when widget's page isn't active, wasting CPU.
**Why it happens:** Existing ClockWidget timer runs unconditionally.
**How to avoid:** Gate the timer on `isCurrentPage`. The clock already has this property but doesn't use it for the timer. Weather widget must also gate its display-staleness check on this.

## Code Examples

### Open-Meteo API Call
```cpp
// Source: https://open-meteo.com/en/docs
// URL format for current weather
QString WeatherService::buildUrl(double lat, double lon) {
    return QString(
        "https://api.open-meteo.com/v1/forecast"
        "?latitude=%1&longitude=%2"
        "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
        "weather_code,wind_speed_10m,wind_direction_10m,is_day"
        "&timezone=auto"
    ).arg(lat, 0, 'f', 4).arg(lon, 0, 'f', 4);
}

// Response format (JSON):
// {
//   "current": {
//     "temperature_2m": 22.3,
//     "relative_humidity_2m": 65,
//     "apparent_temperature": 21.8,
//     "weather_code": 3,
//     "wind_speed_10m": 12.5,
//     "wind_direction_10m": 180,
//     "is_day": 1
//   },
//   "current_units": {
//     "temperature_2m": "°C",
//     ...
//   }
// }
```

### WMO Weather Code to MaterialIcon Mapping
```javascript
// QML helper function
// Source: https://gist.github.com/stellasphere/9490c195ed2b53c707087c8c2db4ec0c
function weatherIcon(code, isDay) {
    // Clear/cloudy
    if (code === 0) return isDay ? "\ue81a" : "\uf159"  // wb_sunny / dark_mode
    if (code <= 2) return isDay ? "\ue42d" : "\uf172"   // wb_cloudy / nights_stay
    if (code === 3) return "\ue42d"                       // cloud
    // Fog
    if (code === 45 || code === 48) return "\ue818"       // foggy
    // Drizzle/Rain
    if (code >= 51 && code <= 67) return "\uf176"         // rainy
    // Snow
    if (code >= 71 && code <= 77) return "\ue80f"         // ac_unit
    // Showers
    if (code >= 80 && code <= 82) return "\uf176"         // rainy
    if (code >= 85 && code <= 86) return "\ue80f"         // ac_unit (snow showers)
    // Thunderstorm
    if (code >= 95) return "\ue31d"                       // thunderstorm
    return "\ue42d"  // cloud fallback
}
```

### Clock Descriptor with Style Config
```cpp
// Updated clock descriptor registration in main.cpp
clockDesc.defaultConfig = {{"format", "24h"}, {"style", "digital"}};
clockDesc.configSchema = {
    oap::ConfigSchemaField{
        "style", "Clock Style", oap::ConfigFieldType::Enum,
        {"Digital", "Analog", "Minimal"}, {"digital", "analog", "minimal"},
        0, 0, 0
    },
    oap::ConfigSchemaField{
        "format", "Time Format", oap::ConfigFieldType::Enum,
        {"12-hour", "24-hour"}, {"12h", "24h"},
        0, 0, 0
    }
};
```

### Weather Widget Descriptor Registration
```cpp
oap::WidgetDescriptor weatherDesc;
weatherDesc.id = "org.openauto.weather";
weatherDesc.displayName = "Weather";
weatherDesc.iconName = "\ue2bd";  // thermostat (or \ue818 foggy for generic weather)
weatherDesc.category = "status";
weatherDesc.description = "Current weather conditions";
weatherDesc.minCols = 1; weatherDesc.minRows = 1;
weatherDesc.maxCols = 6; weatherDesc.maxRows = 4;
weatherDesc.defaultCols = 2; weatherDesc.defaultRows = 2;
weatherDesc.qmlComponent = QUrl("qrc:/OpenAutoProdigy/WeatherWidget.qml");
weatherDesc.defaultConfig = {{"unit", "fahrenheit"}, {"refresh", "5"}};
weatherDesc.configSchema = {
    oap::ConfigSchemaField{
        "unit", "Temperature Unit", oap::ConfigFieldType::Enum,
        {QString::fromUtf8("°F"), QString::fromUtf8("°C")}, {"fahrenheit", "celsius"},
        0, 0, 0
    },
    oap::ConfigSchemaField{
        "refresh", "Refresh Interval", oap::ConfigFieldType::Enum,
        {"5 minutes", "15 minutes", "30 minutes", "60 minutes"}, {"5", "15", "30", "60"},
        0, 0, 0
    }
};
widgetRegistry->registerWidget(weatherDesc);
```

### Analog Clock Canvas Rendering
```qml
// Analog clock component using Canvas2D
Canvas {
    id: clockCanvas
    anchors.fill: parent
    onPaint: {
        var ctx = getContext("2d")
        var w = width, h = height
        var r = Math.min(w, h) / 2 - 4
        var cx = w / 2, cy = h / 2
        ctx.reset()

        // Face background
        ctx.beginPath()
        ctx.arc(cx, cy, r, 0, 2 * Math.PI)
        ctx.fillStyle = ThemeService.surfaceContainer
        ctx.fill()

        // Hour ticks
        for (var i = 0; i < 12; i++) {
            var angle = (i * 30 - 90) * Math.PI / 180
            var inner = r * 0.85, outer = r * 0.95
            ctx.beginPath()
            ctx.moveTo(cx + inner * Math.cos(angle), cy + inner * Math.sin(angle))
            ctx.lineTo(cx + outer * Math.cos(angle), cy + outer * Math.sin(angle))
            ctx.strokeStyle = ThemeService.onSurfaceVariant
            ctx.lineWidth = 2
            ctx.stroke()
        }

        // Hands
        var now = new Date()
        var hr = now.getHours() % 12, mn = now.getMinutes(), sc = now.getSeconds()
        drawHand(ctx, cx, cy, (hr + mn/60) * 30 - 90, r * 0.5, 3, ThemeService.onSurface)   // hour
        drawHand(ctx, cx, cy, (mn + sc/60) * 6 - 90, r * 0.7, 2, ThemeService.onSurface)     // minute
        drawHand(ctx, cx, cy, sc * 6 - 90, r * 0.8, 1, ThemeService.onSurfaceVariant)         // second

        // Center dot
        ctx.beginPath()
        ctx.arc(cx, cy, 3, 0, 2 * Math.PI)
        ctx.fillStyle = ThemeService.onSurface
        ctx.fill()
    }

    function drawHand(ctx, cx, cy, angleDeg, length, width, color) {
        var angle = angleDeg * Math.PI / 180
        ctx.beginPath()
        ctx.moveTo(cx, cy)
        ctx.lineTo(cx + length * Math.cos(angle), cy + length * Math.sin(angle))
        ctx.strokeStyle = color
        ctx.lineWidth = width
        ctx.lineCap = "round"
        ctx.stroke()
    }

    Timer {
        interval: 1000
        running: clockWidget.isCurrentPage
        repeat: true
        triggeredOnStart: true
        onTriggered: clockCanvas.requestPaint()
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| QML XMLHttpRequest | C++ QNetworkAccessManager | Always for production | Proper proxy, caching, error handling |
| Canvas.Image render target | Canvas.Immediate (default) | Qt 6.x | Immediate is synchronous and simpler for low-frequency updates |
| Full Canvas redraw | requestPaint() on timer | Qt 6 best practice | Only repaints when explicitly requested |

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Qt6::Test (QTest) + CTest |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cd build && ctest --output-on-failure -R weather` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CK-01 | Clock configSchema has 3 styles | unit | `ctest -R test_widget_config` | Existing (extend) |
| CK-02 | Style config persists per-instance | unit | `ctest -R test_widget_instance_context` | Existing (extend) |
| WX-01 | WeatherService fetches and parses Open-Meteo JSON | unit | `ctest -R test_weather_service` | Wave 0 |
| WX-02 | WeatherData exposes temp, code, humidity, wind | unit | `ctest -R test_weather_service` | Wave 0 |
| WX-04 | Refresh timer respects interval config | unit | `ctest -R test_weather_service` | Wave 0 |

### Sampling Rate
- **Per task commit:** `cd build && ctest --output-on-failure -R "weather\|widget_config\|widget_instance"`
- **Per wave merge:** `cd build && ctest --output-on-failure`
- **Phase gate:** Full suite green before verify

### Wave 0 Gaps
- [ ] `tests/test_weather_service.cpp` -- covers WX-01, WX-02, WX-04 (WeatherData parsing, coordinate rounding, refresh logic)
- [ ] `tests/data/open_meteo_response.json` -- test fixture for API response parsing
- Framework install: None needed -- Qt6::Test already configured

## Open Questions

1. **Location name display**
   - What we know: Open-Meteo `/v1/forecast` does NOT return a location name in the response. It returns lat/lon/elevation only.
   - What's unclear: How to display the location name at 3x3+ size. Options: (a) use Open-Meteo's geocoding API (`https://geocoding-api.open-meteo.com/v1/reverse?latitude=X&longitude=Y`) to reverse-geocode, (b) skip location name and show coordinates, (c) show "Current Location" as static text.
   - Recommendation: Use Open-Meteo's reverse geocoding API. It's free, no key needed, and the same provider. One extra HTTP call per location change. Cache the name alongside WeatherData.

2. **QNetworkAccessManager and system proxy**
   - What we know: The Pi routes internet through a SOCKS5 proxy set up by the system daemon. `QNetworkAccessManager` respects `QNetworkProxy::setApplicationProxy()` or system environment variables.
   - What's unclear: Whether the OS-level routing (iptables/redsocks) makes the proxy transparent to QNAM without explicit proxy configuration.
   - Recommendation: Start with default QNAM (no explicit proxy). If the system-level routing works (it should -- it's OS-level, not application-level), no proxy config needed in WeatherService. If it doesn't, fall back to reading `CompanionService.proxyAddress` and setting `QNetworkProxy` on the QNAM.

## Sources

### Primary (HIGH confidence)
- Open-Meteo API docs (https://open-meteo.com/en/docs) -- endpoint format, current weather variables, parameters
- Qt 6 Canvas QML Type docs (https://doc.qt.io/qt-6/qml-qtquick-canvas.html) -- Canvas2D API, performance notes
- Project source: `src/core/widget/WidgetTypes.hpp` -- ConfigSchemaField, WidgetDescriptor
- Project source: `qml/widgets/ClockWidget.qml` -- existing clock implementation
- Project source: `src/core/services/CompanionListenerService.hpp` -- GPS and internet availability

### Secondary (MEDIUM confidence)
- WMO weather code mapping (https://gist.github.com/stellasphere/9490c195ed2b53c707087c8c2db4ec0c) -- code-to-description and icon mapping for Open-Meteo
- Qt Quick Clocks demo (https://doc.qt.io/qt-6/qtdoc-demos-clocks-example.html) -- official analog clock example

### Tertiary (LOW confidence)
- MaterialIcon codepoints for weather -- verified against Material Symbols set but exact availability in the project's bundled font should be confirmed during implementation

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- Qt6::Network already linked, Open-Meteo is well-documented, Canvas2D is standard Qt
- Architecture: HIGH -- follows established widget/service patterns in codebase, CONTEXT.md is highly specific
- Pitfalls: HIGH -- proxy/internet issue is well-understood from existing CompanionService code; Canvas2D perf concern is low-risk at 1fps

**Research date:** 2026-03-17
**Valid until:** 2026-04-17 (Open-Meteo API is stable; Qt 6.8 is stable)
