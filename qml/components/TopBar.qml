import QtQuick
import QtQuick.Layouts

Rectangle {
    id: topBar

    color: ThemeService.barBackgroundColor
    Behavior on color { ColorAnimation { duration: 300; easing.type: Easing.InOutQuad } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        // Title
        Text {
            Layout.fillWidth: true
            text: ApplicationController.currentTitle
            color: ThemeService.specialFontColor
            font.pixelSize: UiMetrics.fontTitle
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        // Clock
        Clock {
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
