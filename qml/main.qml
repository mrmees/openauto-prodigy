import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Window {
    id: root
    width: 1024
    height: 600
    visible: true
    title: "OpenAuto Prodigy"
    color: ThemeController.backgroundColor

    // Fullscreen mode driven by Shell
    property bool aaFullscreen: shell.fullscreenMode

    onAaFullscreenChanged: {
        if (aaFullscreen) {
            root.visibility = Window.FullScreen;
        } else {
            root.visibility = Window.Windowed;
        }
    }

    Shell {
        id: shell
    }

    // Keyboard shortcut: Ctrl+D toggles day/night theme
    Shortcut {
        sequence: "Ctrl+D"
        onActivated: ThemeController.toggleMode()
    }
}
