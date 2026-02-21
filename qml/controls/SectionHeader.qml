import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string text: ""

    Layout.fillWidth: true
    Layout.topMargin: 16
    Layout.bottomMargin: 4
    implicitHeight: 32

    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        text: root.text
        font.pixelSize: 13
        font.bold: true
        font.capitalization: Font.AllUppercase
        color: ThemeService.descriptionFontColor
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: ThemeService.descriptionFontColor
        opacity: 0.3
    }
}
