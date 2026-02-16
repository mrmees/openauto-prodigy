import QtQuick
import QtQuick.Layouts

Item {
    id: launcherMenu

    Component.onCompleted: ApplicationController.setTitle("OpenAuto Prodigy")

    GridLayout {
        anchors.centerIn: parent
        columns: 4
        rowSpacing: 20
        columnSpacing: 20

        Tile {
            Layout.preferredWidth: launcherMenu.width * 0.18
            Layout.preferredHeight: launcherMenu.width * 0.18
            tileName: "Android Auto"
            tileEnabled: true
            onClicked: ApplicationController.navigateTo(2)  // AndroidAuto
        }

        Tile {
            Layout.preferredWidth: launcherMenu.width * 0.18
            Layout.preferredHeight: launcherMenu.width * 0.18
            tileName: "Music"
            tileEnabled: false
        }

        Tile {
            Layout.preferredWidth: launcherMenu.width * 0.18
            Layout.preferredHeight: launcherMenu.width * 0.18
            tileName: "Phone"
            tileEnabled: false
        }

        Tile {
            Layout.preferredWidth: launcherMenu.width * 0.18
            Layout.preferredHeight: launcherMenu.width * 0.18
            tileName: "Settings"
            tileEnabled: true
            onClicked: ApplicationController.navigateTo(6)  // Settings
        }
    }
}
