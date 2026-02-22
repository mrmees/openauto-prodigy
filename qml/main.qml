import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

Window {
    id: root
    width: 1024
    height: 600
    visible: true
    visibility: Window.FullScreen
    title: "OpenAuto Prodigy"
    color: ThemeService.backgroundColor

    Material.theme: ThemeService.nightMode ? Material.Dark : Material.Light
    Material.accent: ThemeService.highlightColor
    Material.background: ThemeService.backgroundColor

    Shell {
        id: shell
    }

    // Keyboard shortcut: Ctrl+D toggles day/night theme
    Shortcut {
        sequence: "Ctrl+D"
        onActivated: ThemeService.toggleMode()
    }

    // Keyboard shortcut: Ctrl+Q quit
    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }
}
