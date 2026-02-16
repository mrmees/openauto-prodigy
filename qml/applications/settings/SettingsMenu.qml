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
            tileEnabled: false
        }

        Tile {
            Layout.preferredWidth: settingsMenu.width * 0.18
            Layout.preferredHeight: settingsMenu.width * 0.18
            tileName: "Day/Night"
            tileEnabled: true
            onClicked: ThemeController.toggleMode()
        }

        Tile {
            Layout.preferredWidth: settingsMenu.width * 0.18
            Layout.preferredHeight: settingsMenu.width * 0.18
            tileName: "Colors"
            tileEnabled: false
        }

        Tile {
            Layout.preferredWidth: settingsMenu.width * 0.18
            Layout.preferredHeight: settingsMenu.width * 0.18
            tileName: "Audio"
            tileEnabled: false
        }

        Tile {
            Layout.preferredWidth: settingsMenu.width * 0.18
            Layout.preferredHeight: settingsMenu.width * 0.18
            tileName: "Bluetooth"
            tileEnabled: false
        }

        Tile {
            Layout.preferredWidth: settingsMenu.width * 0.18
            Layout.preferredHeight: settingsMenu.width * 0.18
            tileName: "System"
            tileEnabled: false
        }

        Tile {
            Layout.preferredWidth: settingsMenu.width * 0.18
            Layout.preferredHeight: settingsMenu.width * 0.18
            tileName: "About"
            tileEnabled: false
        }
    }
}
