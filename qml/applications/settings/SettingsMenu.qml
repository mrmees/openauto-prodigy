import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

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

        Tile {
            Layout.preferredWidth: settingsMenu.width * 0.18
            Layout.preferredHeight: settingsMenu.width * 0.18
            tileName: "Exit"
            tileIcon: "\ue5cd"  // close
            tileEnabled: true
            onClicked: exitDialog.open()
        }
    }

    // Exit confirmation dialog
    Dialog {
        id: exitDialog
        anchors.centerIn: parent
        width: 320
        modal: true
        title: "Exit"

        background: Rectangle {
            color: ThemeService.controlBackgroundColor
            radius: 12
            border.color: ThemeService.borderColor
            border.width: 1
        }

        header: Item {
            height: 48
            Text {
                anchors.centerIn: parent
                text: "Exit OpenAuto Prodigy?"
                font.pixelSize: 18
                font.bold: true
                color: ThemeService.primaryTextColor
            }
        }

        contentItem: ColumnLayout {
            spacing: 12

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                onClicked: {
                    exitDialog.close()
                    ApplicationController.minimize()
                }
                contentItem: RowLayout {
                    spacing: 10
                    Item { Layout.fillWidth: true }
                    MaterialIcon {
                        icon: "\ue5cd"  // close (minimize)
                        size: 20
                        color: ThemeService.primaryTextColor
                    }
                    Text {
                        text: "Minimize"
                        font.pixelSize: 16
                        color: ThemeService.primaryTextColor
                    }
                    Item { Layout.fillWidth: true }
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.highlightColor : ThemeService.barBackgroundColor
                    radius: 8
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                onClicked: ApplicationController.quit()
                contentItem: RowLayout {
                    spacing: 10
                    Item { Layout.fillWidth: true }
                    MaterialIcon {
                        icon: "\ue5cd"  // close
                        size: 20
                        color: "#F44336"
                    }
                    Text {
                        text: "Close App"
                        font.pixelSize: 16
                        color: "#F44336"
                    }
                    Item { Layout.fillWidth: true }
                }
                background: Rectangle {
                    color: parent.pressed ? "#F44336" : ThemeService.barBackgroundColor
                    radius: 8
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                onClicked: exitDialog.close()
                contentItem: Text {
                    text: "Cancel"
                    font.pixelSize: 14
                    color: ThemeService.secondaryTextColor
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    color: "transparent"
                    radius: 8
                }
            }
        }

        // Remove default dialog footer/buttons
        footer: Item { height: 0 }
    }
}
