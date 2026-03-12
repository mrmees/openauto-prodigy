import QtQuick
import QtQuick.Layouts

Item {
    id: clockWidget

    // Pixel-based breakpoints for responsive layout
    readonly property bool showDate: width >= 250   // true at 2+ cells wide
    readonly property bool showDay: height >= 200    // true at 2+ cells tall

    ColumnLayout {
        anchors.centerIn: parent
        spacing: showDay ? UiMetrics.spacing : UiMetrics.spacing * 0.5

        NormalText {
            id: timeText
            font.pixelSize: showDay ? UiMetrics.fontHeading * 2.5
                          : showDate ? UiMetrics.fontHeading * 1.8
                          : UiMetrics.fontHeading * 2.0
            font.weight: showDate ? Font.Light : Font.Bold
            color: ThemeService.onSurface
            Layout.alignment: Qt.AlignHCenter
        }

        NormalText {
            id: dateText
            visible: showDate
            font.pixelSize: UiMetrics.fontTitle
            color: ThemeService.onSurfaceVariant
            Layout.alignment: Qt.AlignHCenter
        }

        NormalText {
            id: dayText
            visible: showDay
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.onSurfaceVariant
            Layout.alignment: Qt.AlignHCenter
        }
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            var now = new Date()
            timeText.text = now.toLocaleTimeString(Qt.locale(), "h:mm")
            dateText.text = now.toLocaleDateString(Qt.locale(), "MMMM d")
            dayText.text = now.toLocaleDateString(Qt.locale(), "dddd")
        }
    }
}
