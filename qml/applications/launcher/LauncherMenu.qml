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
            tileIcon: "\ueff7"  // directions_car
            tileEnabled: true
            onClicked: ApplicationController.navigateTo(2)  // AndroidAuto
        }

        Tile {
            Layout.preferredWidth: launcherMenu.width * 0.18
            Layout.preferredHeight: launcherMenu.width * 0.18
            tileName: "Music"
            tileIcon: "\uf01f"  // headphones
            tileEnabled: false
        }

        Tile {
            Layout.preferredWidth: launcherMenu.width * 0.18
            Layout.preferredHeight: launcherMenu.width * 0.18
            tileName: "Phone"
            tileIcon: "\uf0d4"  // phone
            tileEnabled: false
        }

        Tile {
            Layout.preferredWidth: launcherMenu.width * 0.18
            Layout.preferredHeight: launcherMenu.width * 0.18
            tileName: "Settings"
            tileIcon: "\ue8b8"  // settings
            tileEnabled: true
            onClicked: ApplicationController.navigateTo(6)  // Settings
        }
    }
}
