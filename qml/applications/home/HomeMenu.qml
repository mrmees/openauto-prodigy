import QtQuick

Item {
    id: homeMenu

    Component.onCompleted: ApplicationController.setTitle("Dashboard")

    Text {
        anchors.centerIn: parent
        text: "Dashboard"
        color: ThemeController.specialFontColor
        font.pixelSize: 28
        font.bold: true
    }
}
