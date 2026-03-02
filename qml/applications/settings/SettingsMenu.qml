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
        initialItem: settingsList

        pushEnter: Transition {
            ParallelAnimation {
                OpacityAnimator { from: 0; to: 1; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
                XAnimator { from: settingsStack.width * 0.3; to: 0; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
            }
        }
        pushExit: Transition {
            ParallelAnimation {
                OpacityAnimator { from: 1; to: 0; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
                XAnimator { from: 0; to: -settingsStack.width * 0.3; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
            }
        }
        popEnter: Transition {
            ParallelAnimation {
                OpacityAnimator { from: 0; to: 1; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
                XAnimator { from: -settingsStack.width * 0.3; to: 0; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
            }
        }
        popExit: Transition {
            ParallelAnimation {
                OpacityAnimator { from: 1; to: 0; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
                XAnimator { from: 0; to: settingsStack.width * 0.3; duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
            }
        }
    }

    Component {
        id: settingsList

        Item {
            anchors.fill: parent

            GridLayout {
                anchors.centerIn: parent
                columns: 3
                columnSpacing: UiMetrics.gridGap
                rowSpacing: UiMetrics.gridGap

                Tile {
                    tileName: "Android Auto"
                    tileIcon: "\ue531"
                    tileSubtitle: {
                        var r = ConfigService.value("video.resolution") || "720p"
                        var f = ConfigService.value("video.fps") || 60
                        return r + " " + f + "fps"
                    }
                    Layout.preferredWidth: UiMetrics.tileW
                    Layout.preferredHeight: UiMetrics.tileH
                    onClicked: openPage("video")
                }

                Tile {
                    tileName: "Display"
                    tileIcon: "\ueb97"
                    tileSubtitle: {
                        var t = ConfigService.value("display.theme") || "default"
                        return t.charAt(0).toUpperCase() + t.slice(1)
                    }
                    Layout.preferredWidth: UiMetrics.tileW
                    Layout.preferredHeight: UiMetrics.tileH
                    onClicked: openPage("display")
                }

                Tile {
                    tileName: "Audio"
                    tileIcon: "\ue050"
                    tileSubtitle: {
                        var v = typeof AudioService !== "undefined" ? AudioService.masterVolume : 100
                        return "Vol: " + v + "%"
                    }
                    Layout.preferredWidth: UiMetrics.tileW
                    Layout.preferredHeight: UiMetrics.tileH
                    onClicked: openPage("audio")
                }

                Tile {
                    tileName: "Connectivity"
                    tileIcon: "\ue1d8"
                    tileSubtitle: {
                        if (typeof BluetoothManager === "undefined") return ""
                        var n = BluetoothManager.connectedDeviceName
                        return n !== "" ? "BT: " + n : "BT: No device"
                    }
                    Layout.preferredWidth: UiMetrics.tileW
                    Layout.preferredHeight: UiMetrics.tileH
                    onClicked: openPage("connection")
                }

                Tile {
                    tileName: "Companion"
                    tileIcon: "\ue324"
                    tileSubtitle: {
                        if (typeof CompanionService === "undefined") return "Disabled"
                        return CompanionService.connected ? "Connected" : "Not connected"
                    }
                    Layout.preferredWidth: UiMetrics.tileW
                    Layout.preferredHeight: UiMetrics.tileH
                    onClicked: openPage("companion")
                }

                Tile {
                    tileName: "System"
                    tileIcon: "\uf8cd"
                    tileSubtitle: "v" + (ConfigService.value("identity.sw_version") || "0.0.0")
                    Layout.preferredWidth: UiMetrics.tileW
                    Layout.preferredHeight: UiMetrics.tileH
                    onClicked: openPage("system")
                }
            }
        }
    }

    Component { id: displayPage; DisplaySettings {} }
    Component { id: audioPage; AudioSettings {} }
    Component { id: connectionPage; ConnectionSettings {} }
    Component { id: videoPage; VideoSettings {} }
    Component { id: systemPage; SystemSettings {} }
    Component { id: companionPage; CompanionSettings {} }
    Component { id: aboutPage; AboutSettings {} }

    function openPage(pageId) {
        var titles = {
            "display": "Display",
            "audio": "Audio",
            "connection": "Connection",
            "video": "Video",
            "system": "System",
            "companion": "Companion",
            "about": "About"
        }
        var pages = {
            "display": displayPage,
            "audio": audioPage,
            "connection": connectionPage,
            "video": videoPage,
            "system": systemPage,
            "companion": companionPage,
            "about": aboutPage
        }

        if (pageId.startsWith("plugin:")) {
            var pluginId = pageId.substring(7)
            if (typeof PluginModel === "undefined") {
                return
            }
            for (var i = 0; i < PluginModel.rowCount(); i++) {
                var idx = PluginModel.index(i, 0)
                if (PluginModel.data(idx, 257) === pluginId) {
                    var settingsUrl = PluginModel.data(idx, 264)  // SettingsQmlRole
                    if (settingsUrl && settingsUrl.toString() !== "") {
                        ApplicationController.setTitle("Settings > " + PluginModel.data(idx, 258))
                        settingsStack.push(settingsUrl)
                    }
                    return
                }
            }
            return
        }

        if (pages[pageId]) {
            ApplicationController.setTitle("Settings > " + titles[pageId])
            settingsStack.push(pages[pageId])
        }
    }
}
