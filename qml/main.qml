import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

Window {
    id: root
    width: _geomW > 0 ? _geomW : 1024
    height: _geomH > 0 ? _geomH : 600
    visible: true
    visibility: _geomW > 0 ? Window.Windowed : Window.FullScreen
    title: "OpenAuto Prodigy"
    color: ThemeService.backgroundColor
    Behavior on color { ColorAnimation { duration: 300; easing.type: Easing.InOutQuad } }

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
