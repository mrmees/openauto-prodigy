import QtQuick

Item {
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: ThemeService.backgroundColor
    }

    Image {
        anchors.fill: parent
        source: ThemeService.wallpaperSource
        fillMode: Image.PreserveAspectCrop
        visible: source !== ""
        asynchronous: true
    }
}
