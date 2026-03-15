import QtQuick

Item {
    anchors.fill: parent
    clip: true

    Rectangle {
        anchors.fill: parent
        color: ThemeService.background
        Behavior on color { ColorAnimation { duration: 300; easing.type: Easing.InOutQuad } }
    }

    Image {
        anchors.fill: parent
        source: ThemeService.wallpaperSource
        fillMode: Image.PreserveAspectCrop
        visible: source !== ""
        asynchronous: true
        retainWhileLoading: true
        sourceSize.width: DisplayInfo.windowWidth
        sourceSize.height: DisplayInfo.windowHeight
    }
}
