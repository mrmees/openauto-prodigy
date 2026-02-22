import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property string title: ""
    property StackView stack: null

    Layout.fillWidth: true
    implicitHeight: UiMetrics.headerH

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginRow
        spacing: UiMetrics.marginRow

        Rectangle {
            width: UiMetrics.backBtnSize
            height: UiMetrics.backBtnSize
            radius: UiMetrics.backBtnSize / 2
            color: backMouse.containsMouse ? ThemeService.highlightColor : "transparent"

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue5c4"  // arrow_back
                size: UiMetrics.iconSize
                color: ThemeService.normalFontColor
            }

            MouseArea {
                id: backMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    if (root.stack) root.stack.pop()
                }
            }
        }

        Text {
            text: root.title
            font.pixelSize: UiMetrics.fontTitle
            font.bold: true
            color: ThemeService.normalFontColor
            Layout.fillWidth: true
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: ThemeService.descriptionFontColor
        opacity: 0.2
    }
}
