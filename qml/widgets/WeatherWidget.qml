import QtQuick
import QtQuick.Layouts

Item {
    id: weatherWidget
    clip: true

    property QtObject widgetContext: null
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1
    readonly property bool isCurrentPage: widgetContext ? widgetContext.isCurrentPage : true

    // Per-instance config
    readonly property var currentEffectiveConfig: widgetContext ? widgetContext.effectiveConfig : ({})
    readonly property string tempUnit: currentEffectiveConfig.unit || "fahrenheit"
    readonly property string refreshInterval: currentEffectiveConfig.refresh || "5"

    // Breakpoints (column-driven)
    readonly property bool showIcon: colSpan >= 2
    readonly property bool showCity: colSpan >= 3
    readonly property bool showWind: colSpan >= 4
    readonly property bool showHumidity: colSpan >= 5
    readonly property bool showFeelsLike: colSpan >= 6
    readonly property bool showExtended: colSpan >= 3 && rowSpan >= 3

    // GPS + companion state (null-safe)
    // IMPORTANT: Use gpsStale to detect GPS availability. Checking lat/lon values
    // would reject valid equator coordinates. gpsStale is the proper signal from
    // CompanionListenerService for "GPS data is available and current".
    readonly property bool companionConnected: CompanionService ? CompanionService.connected : false
    readonly property bool hasGps: companionConnected
                                   && CompanionService
                                   && !CompanionService.gpsStale
    readonly property double gpsLat: CompanionService ? CompanionService.gpsLat : 0
    readonly property double gpsLon: CompanionService ? CompanionService.gpsLon : 0

    // Weather data object (requested imperatively, not in binding)
    property QtObject weatherData: null

    // Track previous subscription to unsubscribe on location change
    property double subscribedLat: NaN
    property double subscribedLon: NaN
    property int subscribedInterval: 5

    // Temperature conversion (no unit suffix — user knows what they selected)
    function displayTemp(tempC) {
        if (tempUnit === "celsius") return Math.round(tempC) + "\u00B0"
        return Math.round(tempC * 9/5 + 32) + "\u00B0"
    }

    // WMO weather code to MaterialIcon mapping (Material Symbols Outlined codepoints)
    function weatherIcon(code, isDay) {
        if (code === 0) return isDay ? "\ue81a" : "\uf159"  // sunny / bedtime
        if (code <= 2) return isDay ? "\uf15c" : "\uf174"   // cloud / partly_cloudy_night
        if (code === 3) return "\uf15c"                       // cloud
        if (code === 45 || code === 48) return "\ue818"       // foggy
        if (code >= 51 && code <= 67) return "\uf176"         // rainy
        if (code >= 71 && code <= 77) return "\ued5b"         // snowflake
        if (code >= 80 && code <= 82) return "\uf176"         // rainy
        if (code >= 85 && code <= 86) return "\ued5b"         // snowflake
        if (code >= 95) return "\uebdb"                       // thunderstorm
        return "\uf15c"                                        // cloud fallback
    }

    // WMO code to human-readable text
    function weatherText(code) {
        if (code === 0) return "Clear"
        if (code <= 2) return "Partly Cloudy"
        if (code === 3) return "Overcast"
        if (code === 45) return "Fog"
        if (code === 48) return "Rime Fog"
        if (code >= 51 && code <= 55) return "Drizzle"
        if (code >= 56 && code <= 57) return "Freezing Drizzle"
        if (code >= 61 && code <= 65) return "Rain"
        if (code >= 66 && code <= 67) return "Freezing Rain"
        if (code >= 71 && code <= 75) return "Snow"
        if (code === 77) return "Snow Grains"
        if (code >= 80 && code <= 82) return "Rain Showers"
        if (code >= 85 && code <= 86) return "Snow Showers"
        if (code === 95) return "Thunderstorm"
        if (code >= 96) return "Thunderstorm w/ Hail"
        return "Unknown"
    }

    // --- Subscribe/unsubscribe lifecycle ---

    function doUnsubscribe() {
        if (!isNaN(subscribedLat) && !isNaN(subscribedLon) && WeatherService) {
            WeatherService.unsubscribe(subscribedLat, subscribedLon, subscribedInterval)
            subscribedLat = NaN
            subscribedLon = NaN
        }
    }

    function doSubscribe() {
        if (hasGps && isCurrentPage && WeatherService) {
            doUnsubscribe()  // unsub previous if location changed
            var interval = parseInt(refreshInterval)
            weatherData = WeatherService.getWeatherData(gpsLat, gpsLon)
            WeatherService.subscribe(gpsLat, gpsLon, interval)
            subscribedLat = gpsLat
            subscribedLon = gpsLon
            subscribedInterval = interval
        }
    }

    function updateWeatherData() {
        if (hasGps && isCurrentPage && WeatherService) {
            doSubscribe()
        } else {
            doUnsubscribe()
            weatherData = null
        }
    }

    onHasGpsChanged: updateWeatherData()
    onGpsLatChanged: updateWeatherData()
    onGpsLonChanged: updateWeatherData()
    onRefreshIntervalChanged: updateWeatherData()
    onIsCurrentPageChanged: {
        updateWeatherData()
        // subscribe() internally checks staleness and fetches immediately if needed
    }

    Component.onCompleted: updateWeatherData()
    Component.onDestruction: doUnsubscribe()

    // --- Helper properties for display ---
    readonly property bool hasData: weatherData !== null && !weatherData.loading && weatherData.error === ""
    readonly property bool hasError: weatherData !== null && weatherData.error !== ""

    // --- 1x1 compact: temperature only ---
    NormalText {
        anchors.centerIn: parent
        width: parent.width - UiMetrics.spacing * 2
        horizontalAlignment: Text.AlignHCenter
        visible: !weatherWidget.showIcon && !weatherWidget.showExtended
        text: {
            if (!weatherWidget.hasGps) return "--\u00B0"
            if (weatherWidget.hasData) return weatherWidget.displayTemp(weatherWidget.weatherData.tempC)
            return "--\u00B0"
        }
        font.pixelSize: UiMetrics.fontHeading * 2.0
        font.weight: Font.Bold
        fontSizeMode: Text.Fit
        minimumPixelSize: UiMetrics.fontHeading
        color: weatherWidget.hasGps ? ThemeService.onSurface : ThemeService.onSurfaceVariant
    }

    // --- 2+ cols, single/double row: progressive horizontal layout ---
    RowLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.spacing
        spacing: UiMetrics.spacing
        visible: weatherWidget.showIcon && !weatherWidget.showExtended

        MaterialIcon {
            icon: {
                if (!weatherWidget.hasGps) return "\ue0c7"  // location_off
                if (weatherWidget.hasData)
                    return weatherWidget.weatherIcon(weatherWidget.weatherData.weatherCode,
                                                     weatherWidget.weatherData.isDay)
                return "\ue818"  // foggy (placeholder)
            }
            size: UiMetrics.iconSize * (weatherWidget.rowSpan >= 2 ? 2.5 : 1.5)
            color: weatherWidget.hasGps ? ThemeService.onSurface : ThemeService.onSurfaceVariant
        }

        NormalText {
            text: {
                if (!weatherWidget.hasGps) return "--\u00B0"
                if (weatherWidget.hasData) return weatherWidget.displayTemp(weatherWidget.weatherData.tempC)
                return "--\u00B0"
            }
            font.pixelSize: parent.height * 0.6
            font.weight: Font.Bold
            fontSizeMode: Text.Fit
            minimumPixelSize: UiMetrics.fontHeading
            color: weatherWidget.hasGps ? ThemeService.onSurface : ThemeService.onSurfaceVariant
        }

        // City name (3+ cols)
        NormalText {
            visible: weatherWidget.showCity && weatherWidget.hasData
            text: weatherWidget.hasData ? weatherWidget.weatherData.locationName : ""
            font.pixelSize: UiMetrics.fontTitle
            color: ThemeService.onSurfaceVariant
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        // Separator before wind
        NormalText {
            visible: weatherWidget.showWind && weatherWidget.hasData
            text: "-"
            font.pixelSize: UiMetrics.fontTitle
            color: ThemeService.onSurfaceVariant
        }

        // Wind (4+ cols) + air icon
        NormalText {
            visible: weatherWidget.showWind && weatherWidget.hasData
            text: weatherWidget.hasData
                  ? Math.round(weatherWidget.weatherData.windSpeedKph) + " km/h"
                  : ""
            font.pixelSize: UiMetrics.fontTitle
            color: ThemeService.onSurfaceVariant
        }
        MaterialIcon {
            visible: weatherWidget.showWind && weatherWidget.hasData
            icon: "\uefd8"  // air
            size: UiMetrics.fontTitle
            color: ThemeService.onSurfaceVariant
        }

        // Separator before humidity
        NormalText {
            visible: weatherWidget.showHumidity && weatherWidget.hasData
            text: "-"
            font.pixelSize: UiMetrics.fontTitle
            color: ThemeService.onSurfaceVariant
        }

        // Humidity (5+ cols) + water drop icon
        NormalText {
            visible: weatherWidget.showHumidity && weatherWidget.hasData
            text: weatherWidget.hasData
                  ? Math.round(weatherWidget.weatherData.humidity) + "%"
                  : ""
            font.pixelSize: UiMetrics.fontTitle
            color: ThemeService.onSurfaceVariant
        }
        MaterialIcon {
            visible: weatherWidget.showHumidity && weatherWidget.hasData
            icon: "\ue798"  // water_drop
            size: UiMetrics.fontTitle
            color: ThemeService.onSurfaceVariant
        }

        // Separator before feels like
        NormalText {
            visible: weatherWidget.showFeelsLike && weatherWidget.hasData
            text: "-"
            font.pixelSize: UiMetrics.fontTitle
            color: ThemeService.onSurfaceVariant
        }

        // Feels like (6+ cols) — heat icon + temp
        MaterialIcon {
            visible: weatherWidget.showFeelsLike && weatherWidget.hasData
            icon: "\uf537"  // heat
            size: UiMetrics.fontTitle
            color: ThemeService.onSurfaceVariant
        }
        NormalText {
            visible: weatherWidget.showFeelsLike && weatherWidget.hasData
            text: weatherWidget.hasData
                  ? weatherWidget.displayTemp(weatherWidget.weatherData.feelsLikeC)
                  : ""
            font.pixelSize: UiMetrics.fontTitle
            color: ThemeService.onSurfaceVariant
        }
    }

    // --- 3x3+ extended: full weather info ---
    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width - UiMetrics.spacing * 4
        spacing: UiMetrics.spacing
        visible: weatherWidget.showExtended

        // Row 1: Large temp + icon
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: UiMetrics.spacing * 1.5

            MaterialIcon {
                icon: {
                    if (!weatherWidget.hasGps) return "\ue0c7"  // location_off
                    if (weatherWidget.hasData)
                        return weatherWidget.weatherIcon(weatherWidget.weatherData.weatherCode,
                                                         weatherWidget.weatherData.isDay)
                    return "\ue818"
                }
                size: UiMetrics.iconSize * 3
                color: weatherWidget.hasGps ? ThemeService.onSurface : ThemeService.onSurfaceVariant
            }

            NormalText {
                text: {
                    if (!weatherWidget.hasGps) return "--\u00B0"
                    if (weatherWidget.hasData)
                        return weatherWidget.displayTemp(weatherWidget.weatherData.tempC)
                    return "--\u00B0"
                }
                font.pixelSize: UiMetrics.fontHeading * 2.5
                font.weight: Font.Bold
                fontSizeMode: Text.Fit
                minimumPixelSize: UiMetrics.fontHeading
                color: weatherWidget.hasGps ? ThemeService.onSurface : ThemeService.onSurfaceVariant
            }
        }

        // Row 2: Condition text
        NormalText {
            Layout.alignment: Qt.AlignHCenter
            text: {
                if (!weatherWidget.hasGps) return "No GPS"
                if (weatherWidget.hasError) return "Error"
                if (weatherWidget.hasData)
                    return weatherWidget.weatherText(weatherWidget.weatherData.weatherCode)
                return "Loading..."
            }
            font.pixelSize: UiMetrics.fontTitle
            color: ThemeService.onSurfaceVariant
        }

        // Row 3: Feels like, humidity, wind
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: UiMetrics.spacing * 2
            visible: weatherWidget.hasData && weatherWidget.hasGps

            // Feels like
            ColumnLayout {
                spacing: 2
                NormalText {
                    text: "Feels"
                    font.pixelSize: UiMetrics.fontSmall
                    color: ThemeService.onSurfaceVariant
                    Layout.alignment: Qt.AlignHCenter
                }
                NormalText {
                    text: weatherWidget.hasData
                          ? weatherWidget.displayTemp(weatherWidget.weatherData.feelsLikeC)
                          : "--"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // Humidity
            ColumnLayout {
                spacing: 2
                NormalText {
                    text: "Humidity"
                    font.pixelSize: UiMetrics.fontSmall
                    color: ThemeService.onSurfaceVariant
                    Layout.alignment: Qt.AlignHCenter
                }
                NormalText {
                    text: weatherWidget.hasData
                          ? Math.round(weatherWidget.weatherData.humidity) + "%"
                          : "--"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // Wind
            ColumnLayout {
                spacing: 2
                NormalText {
                    text: "Wind"
                    font.pixelSize: UiMetrics.fontSmall
                    color: ThemeService.onSurfaceVariant
                    Layout.alignment: Qt.AlignHCenter
                }
                NormalText {
                    text: weatherWidget.hasData
                          ? Math.round(weatherWidget.weatherData.windSpeedKph) + " km/h"
                          : "--"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }

        // Row 4: Location name
        NormalText {
            Layout.alignment: Qt.AlignHCenter
            text: {
                if (weatherWidget.hasData && weatherWidget.weatherData.locationName !== "")
                    return weatherWidget.weatherData.locationName
                return ""
            }
            font.pixelSize: UiMetrics.fontSmall
            color: ThemeService.onSurfaceVariant
            visible: text !== ""
        }
    }

    // pressAndHold forwarding for host context menu
    MouseArea {
        anchors.fill: parent
        z: -1
        pressAndHoldInterval: 500
        onPressAndHold: {
            if (weatherWidget.parent && weatherWidget.parent.requestContextMenu)
                weatherWidget.parent.requestContextMenu()
        }
    }
}
