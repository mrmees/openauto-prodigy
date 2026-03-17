import QtQuick
import QtQuick.Layouts

Item {
    id: batteryWidget
    clip: true

    property QtObject widgetContext: null
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1

    // Null-safe companion data access
    readonly property bool companionConnected: CompanionService ? CompanionService.connected : false
    readonly property int batteryLevel: CompanionService ? CompanionService.phoneBattery : -1
    readonly property bool charging: CompanionService ? CompanionService.phoneCharging : false

    // Scale factor for larger sizes
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

    ColumnLayout {
        anchors.centerIn: parent
        spacing: UiMetrics.spacing * 0.5

        // Disconnected: battery outline + X overlay (per CONTEXT: "slash/X icon")
        // Connected: normal battery icon or charging icon
        Item {
            Layout.preferredWidth: batteryIcon.size
            Layout.preferredHeight: batteryIcon.size
            Layout.alignment: Qt.AlignHCenter

            MaterialIcon {
                id: batteryIcon
                icon: {
                    if (!batteryWidget.companionConnected)
                        return "\uebd0"  // battery_1_bar (outline shape)
                    if (batteryWidget.charging)
                        return "\ue1a3"  // battery_charging_full
                    return batteryWidget.batteryIconForLevel(batteryWidget.batteryLevel)
                }
                size: UiMetrics.iconSize * 2 * batteryWidget.scaleFactor
                color: {
                    if (!batteryWidget.companionConnected)
                        return ThemeService.onSurfaceVariant
                    if (batteryWidget.batteryLevel <= 19)
                        return ThemeService.error
                    return ThemeService.onSurface
                }
                anchors.centerIn: parent
            }

            // X overlay when disconnected
            MaterialIcon {
                icon: "\ue5cd"  // close (X)
                size: batteryIcon.size * 0.5
                color: ThemeService.error
                anchors.centerIn: parent
                visible: !batteryWidget.companionConnected
            }
        }

        NormalText {
            text: batteryWidget.companionConnected
                  ? batteryWidget.batteryLevel + "%"
                  : "--"
            font.pixelSize: UiMetrics.fontTitle * batteryWidget.scaleFactor
            fontSizeMode: Text.Fit
            minimumPixelSize: UiMetrics.fontSmall
            color: ThemeService.onSurface
            Layout.alignment: Qt.AlignHCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        z: -1
        pressAndHoldInterval: 500
        onPressAndHold: {
            if (batteryWidget.parent && batteryWidget.parent.requestContextMenu)
                batteryWidget.parent.requestContextMenu()
        }
    }
}
