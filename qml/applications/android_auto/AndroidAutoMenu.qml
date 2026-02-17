import QtQuick
import QtQuick.Layouts

Item {
    id: androidAutoMenu

    Component.onCompleted: ApplicationController.setTitle("Android Auto")

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: {
                switch (AndroidAutoService.connectionState) {
                case 0: return "Android Auto";
                case 1: return "Waiting for Device";
                case 2: return "Connecting...";
                case 3: return "Android Auto Active";
                default: return "Android Auto";
                }
            }
            color: ThemeController.specialFontColor
            font.pixelSize: 28
            font.bold: true
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: AndroidAutoService.statusMessage
            color: ThemeController.descriptionFontColor
            font.pixelSize: 18
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.maximumWidth: parent.parent.width * 0.8
        }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 12
            height: 12
            radius: 6
            color: {
                switch (AndroidAutoService.connectionState) {
                case 0: return "#666666";
                case 1: return "#FFA500";
                case 2: return "#FFFF00";
                case 3: return "#00FF00";
                default: return "#666666";
                }
            }

            SequentialAnimation on opacity {
                running: AndroidAutoService.connectionState === 1 || AndroidAutoService.connectionState === 2
                loops: Animation.Infinite
                NumberAnimation { to: 0.3; duration: 800 }
                NumberAnimation { to: 1.0; duration: 800 }
            }
        }
    }
}
