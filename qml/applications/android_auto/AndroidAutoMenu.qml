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
            text: "Connect Android Auto"
            color: ThemeController.specialFontColor
            font.pixelSize: 28
            font.bold: true
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Waiting for device..."
            color: ThemeController.descriptionFontColor
            font.pixelSize: 18
        }
    }
}
