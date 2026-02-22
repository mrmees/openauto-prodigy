import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: settingsMenu

    Component.onCompleted: ApplicationController.setTitle("Settings")

    Connections {
        target: ApplicationController
        function onBackRequested() {
            if (settingsStack.depth > 1) {
                settingsStack.pop()
                ApplicationController.setTitle("Settings")
                ApplicationController.setBackHandled(true)
            }
        }
    }

    StackView {
        id: settingsStack
        anchors.fill: parent
        initialItem: tileGrid
    }

    Component {
        id: tileGrid

        Item {
            GridLayout {
                anchors.centerIn: parent
                columns: 3
                rowSpacing: UiMetrics.gridGap
                columnSpacing: UiMetrics.gridGap

                Tile {
                    Layout.preferredWidth: settingsMenu.width * 0.22
                    Layout.preferredHeight: settingsMenu.width * 0.18
                    tileName: "Display"
                    tileIcon: "\ueb97"
                    onClicked: {
                        ApplicationController.setTitle("Settings > Display")
                        settingsStack.push(displayPage)
                    }
                }

                Tile {
                    Layout.preferredWidth: settingsMenu.width * 0.22
                    Layout.preferredHeight: settingsMenu.width * 0.18
                    tileName: "Audio"
                    tileIcon: "\ue050"
                    onClicked: {
                        ApplicationController.setTitle("Settings > Audio")
                        settingsStack.push(audioPage)
                    }
                }

                Tile {
                    Layout.preferredWidth: settingsMenu.width * 0.22
                    Layout.preferredHeight: settingsMenu.width * 0.18
                    tileName: "Connection"
                    tileIcon: "\ue1d8"  // wifi
                    onClicked: {
                        ApplicationController.setTitle("Settings > Connection")
                        settingsStack.push(connectionPage)
                    }
                }

                Tile {
                    Layout.preferredWidth: settingsMenu.width * 0.22
                    Layout.preferredHeight: settingsMenu.width * 0.18
                    tileName: "Video"
                    tileIcon: "\ue04b"
                    onClicked: {
                        ApplicationController.setTitle("Settings > Video")
                        settingsStack.push(videoPage)
                    }
                }

                Tile {
                    Layout.preferredWidth: settingsMenu.width * 0.22
                    Layout.preferredHeight: settingsMenu.width * 0.18
                    tileName: "System"
                    tileIcon: "\uf8cd"
                    onClicked: {
                        ApplicationController.setTitle("Settings > System")
                        settingsStack.push(systemPage)
                    }
                }

                Tile {
                    Layout.preferredWidth: settingsMenu.width * 0.22
                    Layout.preferredHeight: settingsMenu.width * 0.18
                    tileName: "About"
                    tileIcon: "\ue88e"
                    onClicked: {
                        ApplicationController.setTitle("Settings > About")
                        settingsStack.push(aboutPage)
                    }
                }
            }
        }
    }

    Component { id: displayPage; DisplaySettings {} }
    Component { id: audioPage; AudioSettings {} }
    Component { id: connectionPage; ConnectionSettings {} }
    Component { id: videoPage; VideoSettings {} }
    Component { id: systemPage; SystemSettings {} }
    Component { id: aboutPage; AboutSettings {} }
}
