import QtQuick
import QtQuick.Layouts

Item {
    id: companionStatusWidget
    clip: true

    property QtObject widgetContext: null
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1

    // Null-safe companion data access
    readonly property bool companionConnected: CompanionService ? CompanionService.connected : false
    readonly property bool gpsStale: CompanionService ? CompanionService.gpsStale : true
    readonly property bool hasGps: companionConnected && !gpsStale
    readonly property int battery: CompanionService ? CompanionService.phoneBattery : -1
    readonly property bool charging: CompanionService ? CompanionService.phoneCharging : false
    readonly property string proxy: CompanionService ? CompanionService.proxyStatus : ""

    // Show detail list when wide enough (regardless of connection state)
    readonly property bool showDetails: colSpan >= 2

    function batteryIconForLevel(level) {
        if (level < 0)  return "";
        if (level === 0) return "\ue19c";       // battery_alert
        if (level <= 20) return "\uf09c";       // battery_1_bar
        if (level <= 39) return "\uf09e";       // battery_3_bar
        if (level <= 59) return "\uf09f";       // battery_4_bar
        if (level <= 79) return "\uf0a0";       // battery_5_bar
        if (level <= 95) return "\uf0a1";       // battery_6_bar
        return "\ue1a5";                         // battery_full (96+)
    }

    // --- Compact view (1 col) — icon only ---
    MaterialIcon {
        anchors.centerIn: parent
        visible: !companionStatusWidget.showDetails
        icon: companionStatusWidget.companionConnected
              ? "\uf728" : "\uf1ca"  // captive_portal / public_off
        size: Math.min(parent.width, parent.height) * 0.6
        color: companionStatusWidget.companionConnected
               ? ThemeService.primary : ThemeService.onSurfaceVariant
    }

    // --- Detail list view (2+ cols) ---
    // Scale icon and font with available height (4 rows of content)
    readonly property real detailIconSize: Math.max(UiMetrics.iconSize, (height - UiMetrics.spacing * 6) / 4 * 0.5)
    readonly property real detailFontSize: Math.max(UiMetrics.fontSmall, (height - UiMetrics.spacing * 6) / 4 * 0.35)

    Rectangle {
        anchors.fill: parent
        anchors.margins: UiMetrics.spacing
        visible: companionStatusWidget.showDetails
        color: "transparent"
        border.color: ThemeService.outlineVariant
        border.width: 1
        radius: UiMetrics.radius

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.spacing
        spacing: UiMetrics.spacing * 0.5

        // Connection status row (replaces the big icon)
        RowLayout {
            spacing: UiMetrics.spacing * 0.5
            Layout.fillHeight: true
            MaterialIcon {
                icon: companionStatusWidget.companionConnected
                      ? "\uf728" : "\uf1ca"  // captive_portal / public_off
                size: companionStatusWidget.detailIconSize
                color: companionStatusWidget.companionConnected
                       ? ThemeService.primary : ThemeService.error
            }
            NormalText {
                text: companionStatusWidget.companionConnected ? "Connected" : "Offline"
                font.pixelSize: companionStatusWidget.detailFontSize
                font.weight: Font.Bold
                fontSizeMode: Text.Fit
                minimumPixelSize: UiMetrics.fontSmall
                color: companionStatusWidget.companionConnected
                       ? ThemeService.onSurface : ThemeService.onSurfaceVariant
                Layout.fillWidth: true
            }
        }

        // GPS row
        RowLayout {
            spacing: UiMetrics.spacing * 0.5
            Layout.fillHeight: true
            MaterialIcon {
                icon: "\ueb3a"  // satellite_alt
                size: companionStatusWidget.detailIconSize
                color: companionStatusWidget.hasGps
                       ? ThemeService.primary : ThemeService.onSurfaceVariant
            }
            NormalText {
                text: companionStatusWidget.hasGps ? "GPS Active" : "No GPS"
                font.pixelSize: companionStatusWidget.detailFontSize
                fontSizeMode: Text.Fit
                minimumPixelSize: UiMetrics.fontSmall
                color: companionStatusWidget.hasGps
                       ? ThemeService.onSurface : ThemeService.onSurfaceVariant
                Layout.fillWidth: true
            }
        }

        // Battery row
        RowLayout {
            spacing: UiMetrics.spacing * 0.5
            Layout.fillHeight: true
            MaterialIcon {
                icon: companionStatusWidget.batteryIconForLevel(companionStatusWidget.battery)
                size: companionStatusWidget.detailIconSize
                color: {
                    if (companionStatusWidget.battery <= 20 && companionStatusWidget.battery >= 0)
                        return ThemeService.error
                    return ThemeService.onSurface
                }
            }
            NormalText {
                text: companionStatusWidget.battery >= 0
                      ? companionStatusWidget.battery + "%" : "--"
                font.pixelSize: companionStatusWidget.detailFontSize
                fontSizeMode: Text.Fit
                minimumPixelSize: UiMetrics.fontSmall
                color: ThemeService.onSurface
                Layout.fillWidth: true
            }
        }

        // Proxy row
        RowLayout {
            spacing: UiMetrics.spacing * 0.5
            Layout.fillHeight: true
            MaterialIcon {
                icon: companionStatusWidget.proxy === "active"
                      ? "\ue1e2" : "\uf087"  // wifi_tethering / wifi_tethering_off
                size: companionStatusWidget.detailIconSize
                color: companionStatusWidget.proxy === "active"
                       ? ThemeService.primary : ThemeService.onSurfaceVariant
            }
            NormalText {
                text: companionStatusWidget.proxy === "active" ? "Proxy On" : "Proxy Off"
                font.pixelSize: companionStatusWidget.detailFontSize
                fontSizeMode: Text.Fit
                minimumPixelSize: UiMetrics.fontSmall
                color: companionStatusWidget.proxy === "active"
                       ? ThemeService.onSurface : ThemeService.onSurfaceVariant
                Layout.fillWidth: true
            }
        }
    }
    }

    // pressAndHold forwarding for host context menu
    MouseArea {
        anchors.fill: parent
        z: -1
        pressAndHoldInterval: 500
        onPressAndHold: {
            if (companionStatusWidget.parent && companionStatusWidget.parent.requestContextMenu)
                companionStatusWidget.parent.requestContextMenu()
        }
    }
}
