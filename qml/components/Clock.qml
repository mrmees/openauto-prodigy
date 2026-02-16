import QtQuick

Text {
    id: clock

    color: ThemeController.normalFontColor
    font.pixelSize: 18

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: clock.text = Qt.formatTime(new Date(), "h:mm AP")
    }
}
