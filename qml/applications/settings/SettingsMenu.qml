import QtQuick
import QtQuick.Layouts

Item {
    id: settingsMenu

    Component.onCompleted: ApplicationController.setTitle("Settings")

    GridLayout {
        anchors.centerIn: parent
        columns: 4
        rowSpacing: 20
        columnSpacing: 20

        Tile {
            Layout.preferredWidth: settingsMenu.width * 0.18
            Layout.preferredHeight: settingsMenu.width * 0.18
            tileName: "Display"
            tileIcon: "\ueb97"  // display_settings
            tileEnabled: false
        }

        Tile {
            Layout.preferredWidth: settingsMenu.width * 0.18
            Layout.preferredHeight: settingsMenu.width * 0.18
            tileName: "Day/Night"
            tileIcon: ThemeService.nightMode ? "\ue518" : "\ue51c"  // light_mode / dark_mode
            tileEnabled: true
            onClicked: ThemeService.toggleMode()
        }

        Tile {
            Layout.preferredWidth: settingsMenu.width * 0.18
            Layout.preferredHeight: settingsMenu.width * 0.18
            tileName: "Colors"
            tileIcon: "\ue40a"  // palette
            tileEnabled: false
        }

        Tile {
            Layout.preferredWidth: settingsMenu.width * 0.18
            Layout.preferredHeight: settingsMenu.width * 0.18
            tileName: "Audio"
            tileIcon: "\ue050"  // volume_up
            tileEnabled: false
        }

        Tile {
            Layout.preferredWidth: settingsMenu.width * 0.18
            Layout.preferredHeight: settingsMenu.width * 0.18
            tileName: "Bluetooth"
            tileIcon: "\ue1a7"  // bluetooth
            tileEnabled: false
        }

        Tile {
            Layout.preferredWidth: settingsMenu.width * 0.18
            Layout.preferredHeight: settingsMenu.width * 0.18
            tileName: "System"
            tileIcon: "\uf8cd"  // build
            tileEnabled: false
        }

        Tile {
            Layout.preferredWidth: settingsMenu.width * 0.18
            Layout.preferredHeight: settingsMenu.width * 0.18
            tileName: "About"
            tileIcon: "\ue88e"  // info
            tileEnabled: false
        }
    }
}
