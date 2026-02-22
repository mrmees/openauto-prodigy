import QtQuick

Text {
    id: clock

    color: ThemeService.normalFontColor
    font.pixelSize: UiMetrics.fontHeading

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: clock.text = Qt.formatTime(new Date(), "h:mm AP")
    }
}
