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
    readonly property double lat: CompanionService ? CompanionService.gpsLat : 0
    readonly property double lon: CompanionService ? CompanionService.gpsLon : 0
    readonly property int battery: CompanionService ? CompanionService.phoneBattery : -1
    readonly property bool charging: CompanionService ? CompanionService.phoneCharging : false
    readonly property string proxy: CompanionService ? CompanionService.proxyAddress : ""

    // Expanded when colSpan >= 2 AND companion is connected
    readonly property bool expanded: colSpan >= 2 && companionConnected

    readonly property real scaleFactor: Math.min(colSpan, rowSpan, 3)

    function batteryIconForLevel(level) {
        if (level < 0)  return "";
        if (level === 0) return "\ue19c";       // battery_alert
        if (level <= 19) return "\uebd0";       // battery_1_bar
        if (level <= 39) return "\uebd2";       // battery_3_bar
        if (level <= 59) return "\uebd3";       // battery_4_bar
        if (level <= 79) return "\uebd4";       // battery_5_bar
        if (level <= 95) return "\uebd5";       // battery_6_bar
        return "\ue1a4";                         // battery_full (96+)
    }

    // --- Compact 1x1 view ---
    ColumnLayout {
        anchors.centerIn: parent
        spacing: UiMetrics.spacing * 0.5
        visible: !companionStatusWidget.expanded

        Item {
            Layout.preferredWidth: compactIcon.size
            Layout.preferredHeight: compactIcon.size
            Layout.alignment: Qt.AlignHCenter

            MaterialIcon {
                id: compactIcon
                icon: "\ue325"  // smartphone
                size: UiMetrics.iconSize * 2 * companionStatusWidget.scaleFactor
                color: companionStatusWidget.companionConnected
                       ? ThemeService.onSurface : ThemeService.onSurfaceVariant
                anchors.centerIn: parent
            }

            // Colored dot indicator
            Rectangle {
                width: UiMetrics.spacing * 1.5
                height: width
                radius: width / 2
                color: companionStatusWidget.companionConnected
                       ? ThemeService.success : ThemeService.error
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: -width * 0.2
                anchors.topMargin: -height * 0.2
            }
        }

        NormalText {
            text: companionStatusWidget.companionConnected ? "Connected" : "Offline"
            font.pixelSize: UiMetrics.fontSmall * companionStatusWidget.scaleFactor
            fontSizeMode: Text.Fit
            minimumPixelSize: UiMetrics.fontSmall
            color: ThemeService.onSurface
            Layout.alignment: Qt.AlignHCenter
        }
    }

    // --- Expanded view (colSpan >= 2, connected) ---
    RowLayout {
        anchors.centerIn: parent
        width: parent.width - UiMetrics.spacing * 4
        spacing: UiMetrics.spacing * 2
        visible: companionStatusWidget.expanded

        // Left: icon + dot
        Item {
            Layout.preferredWidth: expandedIcon.size
            Layout.preferredHeight: expandedIcon.size
            Layout.alignment: Qt.AlignVCenter

            MaterialIcon {
                id: expandedIcon
                icon: "\ue325"  // smartphone
                size: UiMetrics.iconSize * 2.5
                color: ThemeService.success
                anchors.centerIn: parent
            }

            Rectangle {
                width: UiMetrics.spacing * 1.5
                height: width
                radius: width / 2
                color: ThemeService.success
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: -width * 0.2
                anchors.topMargin: -height * 0.2
            }
        }

        // Right: detail rows
        ColumnLayout {
            Layout.fillWidth: true
            spacing: UiMetrics.spacing * 0.3

            // GPS row
            RowLayout {
                spacing: UiMetrics.spacing * 0.5
                MaterialIcon {
                    icon: "\ue55c"  // navigation
                    size: UiMetrics.iconSize
                    color: (companionStatusWidget.lat !== 0 || companionStatusWidget.lon !== 0)
                           ? ThemeService.success : ThemeService.onSurfaceVariant
                }
                NormalText {
                    text: (companionStatusWidget.lat !== 0 || companionStatusWidget.lon !== 0)
                          ? "GPS Active" : "No GPS"
                    font.pixelSize: UiMetrics.fontSmall
                    color: ThemeService.onSurface
                }
            }

            // Battery row
            RowLayout {
                spacing: UiMetrics.spacing * 0.5
                MaterialIcon {
                    icon: {
                        if (companionStatusWidget.charging)
                            return "\ue1a3"  // battery_charging_full
                        return companionStatusWidget.batteryIconForLevel(companionStatusWidget.battery)
                    }
                    size: UiMetrics.iconSize
                    color: {
                        if (companionStatusWidget.battery <= 19 && companionStatusWidget.battery >= 0)
                            return ThemeService.error
                        return ThemeService.onSurface
                    }
                }
                NormalText {
                    text: companionStatusWidget.battery >= 0
                          ? companionStatusWidget.battery + "%" : "--"
                    font.pixelSize: UiMetrics.fontSmall
                    color: ThemeService.onSurface
                }
            }

            // Proxy row
            RowLayout {
                spacing: UiMetrics.spacing * 0.5
                MaterialIcon {
                    icon: "\ue8f4"  // wifi_tethering
                    size: UiMetrics.iconSize
                    color: companionStatusWidget.proxy !== ""
                           ? ThemeService.success : ThemeService.onSurfaceVariant
                }
                NormalText {
                    text: companionStatusWidget.proxy !== "" ? "Active" : "Off"
                    font.pixelSize: UiMetrics.fontSmall
                    color: ThemeService.onSurface
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
