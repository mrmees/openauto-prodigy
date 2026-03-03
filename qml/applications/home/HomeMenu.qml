import QtQuick

Item {
    id: homeMenu

    Component.onCompleted: ApplicationController.setTitle("Dashboard")

    Text {
        anchors.centerIn: parent
        text: "Dashboard"
        color: ThemeService.specialFontColor
        font.pixelSize: UiMetrics.fontHeading
        font.bold: true
    }
}
