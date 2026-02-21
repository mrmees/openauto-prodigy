import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string title: ""
    property StackView stack: null

    Layout.fillWidth: true
    implicitHeight: 40

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        spacing: 8

        Rectangle {
            width: 36
            height: 36
            radius: 18
            color: backMouse.containsMouse ? ThemeService.highlightColor : "transparent"

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue5c4"  // arrow_back
                size: 24
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
            font.pixelSize: 18
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
