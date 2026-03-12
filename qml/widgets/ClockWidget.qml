import QtQuick
import QtQuick.Layouts

Item {
    id: clockWidget
    clip: true

    // Pixel-based breakpoints for responsive layout
    readonly property bool showDate: width >= 250   // true at 2+ cells wide
    readonly property bool showDay: height >= 200    // true at 2+ cells tall

    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width - UiMetrics.spacing * 2
        spacing: showDay ? UiMetrics.spacing : UiMetrics.spacing * 0.5

        NormalText {
            id: timeText
            font.pixelSize: showDay ? UiMetrics.fontHeading * 2.5
                          : showDate ? UiMetrics.fontHeading * 1.8
                          : UiMetrics.fontHeading * 2.0
            font.weight: showDate ? Font.Light : Font.Bold
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: UiMetrics.fontHeading
            color: ThemeService.onSurface
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        NormalText {
            id: dateText
            visible: showDate
            font.pixelSize: UiMetrics.fontTitle
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: UiMetrics.fontBody
            color: ThemeService.onSurfaceVariant
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        NormalText {
            id: dayText
            visible: showDay
            font.pixelSize: UiMetrics.fontBody
            fontSizeMode: Text.HorizontalFit
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
            timeText.text = now.toLocaleTimeString(Qt.locale(), "h:mm")
            dateText.text = now.toLocaleDateString(Qt.locale(), "MMMM d")
            dayText.text = now.toLocaleDateString(Qt.locale(), "dddd")
        }
    }
}
