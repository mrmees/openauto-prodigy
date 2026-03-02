import QtQuick

Item {
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: ThemeService.backgroundColor
        Behavior on color { ColorAnimation { duration: 300; easing.type: Easing.InOutQuad } }
    }

    Image {
        anchors.fill: parent
        source: ThemeService.wallpaperSource
        fillMode: Image.PreserveAspectCrop
        visible: source !== ""
        asynchronous: true
    }
}
