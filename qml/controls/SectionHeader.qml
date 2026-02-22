import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string text: ""

    Layout.fillWidth: true
    Layout.topMargin: UiMetrics.sectionGap
    Layout.bottomMargin: UiMetrics.spacing
    implicitHeight: UiMetrics.sectionH

    Text {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.bottomMargin: UiMetrics.spacing
        text: root.text
        font.pixelSize: UiMetrics.fontSmall
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
