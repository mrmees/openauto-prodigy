import QtQuick
import QtQuick.Layouts

Rectangle {
    id: topBar

    color: ThemeService.barBackgroundColor
    Behavior on color { ColorAnimation { duration: 300; easing.type: Easing.InOutQuad } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.gap
        anchors.rightMargin: UiMetrics.gap
        spacing: UiMetrics.marginRow

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

    // Bottom divider line
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: ThemeService.dividerColor
    }
}
