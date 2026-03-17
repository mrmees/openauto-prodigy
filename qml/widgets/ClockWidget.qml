import QtQuick
import QtQuick.Layouts

Item {
    id: clockWidget
    clip: true

    // Widget contract: context injection from host
    property QtObject widgetContext: null

    // Span-based breakpoints for responsive layout
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1
    readonly property bool showDate: colSpan >= 2
    readonly property bool showDay: rowSpan >= 2

    // Load-state gating (timer is the one justified polling case)
    readonly property bool isCurrentPage: widgetContext ? widgetContext.isCurrentPage : true

    // Declarative binding to effectiveConfig — ensures QML tracks property changes.
    // Reading effectiveConfig imperatively inside Timer.onTriggered can return a stale
    // cached JS object. This binding re-evaluates when effectiveConfigChanged fires.
    readonly property var currentEffectiveConfig: widgetContext ? widgetContext.effectiveConfig : ({})
    readonly property string timeFormat: currentEffectiveConfig.format || "24h"

    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width - UiMetrics.spacing * 2
        spacing: showDay ? UiMetrics.spacing * 0.5 : 2

        NormalText {
            id: timeText
            font.pixelSize: showDay ? UiMetrics.fontHeading * 2.0
                          : showDate ? UiMetrics.fontHeading * 1.5
                          : UiMetrics.fontHeading * 2.0
            font.weight: showDate ? Font.Light : Font.Bold
            fontSizeMode: Text.Fit
            minimumPixelSize: UiMetrics.fontHeading
            color: ThemeService.onSurface
            Layout.fillWidth: true
            Layout.maximumHeight: showDay ? parent.height * 0.45
                                : showDate ? parent.height * 0.6
                                : parent.height
            horizontalAlignment: Text.AlignHCenter
        }

        NormalText {
            id: dateText
            visible: showDate
            font.pixelSize: showDay ? UiMetrics.fontTitle : UiMetrics.fontBody
            fontSizeMode: Text.Fit
            minimumPixelSize: UiMetrics.fontSmall
            color: ThemeService.onSurfaceVariant
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        NormalText {
            id: dayText
            visible: showDay
            font.pixelSize: UiMetrics.fontBody
            fontSizeMode: Text.Fit
            minimumPixelSize: UiMetrics.fontSmall
            color: ThemeService.onSurfaceVariant
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            var now = new Date()
            var timeFmt = (clockWidget.timeFormat === "12h") ? "h:mm AP" : "HH:mm"
            timeText.text = now.toLocaleTimeString(Qt.locale(), timeFmt)
            dateText.text = now.toLocaleDateString(Qt.locale(), "MMMM d")
            dayText.text = now.toLocaleDateString(Qt.locale(), "dddd")
        }
    }

    // pressAndHold forwarding for host context menu
    MouseArea {
        anchors.fill: parent
        z: -1
        pressAndHoldInterval: 500
        onPressAndHold: {
            if (clockWidget.parent && clockWidget.parent.requestContextMenu)
                clockWidget.parent.requestContextMenu()
        }
    }
}
