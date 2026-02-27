import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property string icon: ""
    property string label: ""
    property string subtitle: ""

    signal clicked()

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginPage
        anchors.rightMargin: UiMetrics.marginPage
        spacing: UiMetrics.gap

        MaterialIcon {
            icon: root.icon
            size: UiMetrics.iconSize
            color: ThemeService.normalFontColor
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Text {
                text: root.label
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.normalFontColor
                Layout.fillWidth: true
            }

            Text {
                text: root.subtitle
                font.pixelSize: UiMetrics.fontTiny
                color: ThemeService.descriptionFontColor
                Layout.fillWidth: true
                visible: root.subtitle !== ""
            }
        }

        MaterialIcon {
            icon: "\ue5cc"
            size: UiMetrics.iconSmall
            color: ThemeService.descriptionFontColor
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: UiMetrics.marginPage
        anchors.rightMargin: UiMetrics.marginPage
        height: 1
        color: ThemeService.descriptionFontColor
        opacity: 0.15
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
