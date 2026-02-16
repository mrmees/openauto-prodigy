import QtQuick
import QtQuick.Window

Window {
    id: root
    width: 1024
    height: 600
    visible: true
    title: "OpenAuto Prodigy"
    color: ThemeController.backgroundColor

    Text {
        anchors.centerIn: parent
        text: "OpenAuto Prodigy v0.1.0"
        color: ThemeController.specialFontColor
        font.pixelSize: 32
    }

    // Temporary: keyboard shortcut to toggle theme
    Shortcut {
        sequence: "Ctrl+D"
        onActivated: ThemeController.toggleMode()
    }
}
