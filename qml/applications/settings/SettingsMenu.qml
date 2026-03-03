import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: settingsMenu

    property bool _pendingEqNav: false

    Component.onCompleted: ApplicationController.setTitle("Settings")

    function resetToGrid() {
        if (settingsStack.depth > 1) {
            settingsStack.pop(null)
        }
        ApplicationController.setTitle("Settings")
    }

    function openEqDirect() {
        _pendingEqNav = true
        if (visible) {
            _doEqNav()
        }
        // Otherwise onVisibleChanged will handle it
    }

    function _doEqNav() {
        _pendingEqNav = false
        if (settingsStack.depth > 1) {
            settingsStack.pop(null)
        }
        settingsStack.push(audioPage)
        ApplicationController.setTitle("Settings > Audio")
        Qt.callLater(function() {
            settingsStack.push(eqDirectComponent)
            ApplicationController.setTitle("Settings > Audio > Equalizer")
        })
    }

    // Reset to tile grid whenever settings becomes visible (e.g. from launcher)
    onVisibleChanged: {
        if (visible) {
            if (_pendingEqNav) {
                _doEqNav()
            } else {
                resetToGrid()
            }
        }
    }

    Connections {
        target: ApplicationController
        function onBackRequested() {
            if (settingsStack.depth > 1) {
                settingsStack.pop()
                if (settingsStack.depth > 1) {
                    // Back to a category page (depth 2) — restore category title
                    var item = settingsStack.currentItem
                    if (item && item.objectName) {
                        ApplicationController.setTitle("Settings > " + item.objectName)
                    } else {
                        ApplicationController.setTitle("Settings")
                    }
                } else {
                    ApplicationController.setTitle("Settings")
                }
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
            id: settingsGrid
            anchors.fill: parent

            readonly property int _prefTileW: UiMetrics.tileW
            readonly property int _gridCols: Math.max(1, Math.floor(width / (_prefTileW + UiMetrics.gridGap)))
            readonly property int _actualTileW: Math.floor((width - (_gridCols - 1) * UiMetrics.gridGap) / _gridCols)

            GridLayout {
                anchors.centerIn: parent
                columns: settingsGrid._gridCols
                columnSpacing: UiMetrics.gridGap
                rowSpacing: UiMetrics.gridGap

                Tile {
                    tileName: "Android Auto"
                    tileIcon: "\ue531"
                    Layout.preferredWidth: settingsGrid._actualTileW
                    Layout.preferredHeight: UiMetrics.tileH
                    onClicked: openPage("aa")
                }

                Tile {
                    tileName: "Display"
                    tileIcon: "\ueb97"
                    Layout.preferredWidth: settingsGrid._actualTileW
                    Layout.preferredHeight: UiMetrics.tileH
                    onClicked: openPage("display")
                }

                Tile {
                    tileName: "Audio"
                    tileIcon: "\ue050"
                    Layout.preferredWidth: settingsGrid._actualTileW
                    Layout.preferredHeight: UiMetrics.tileH
                    onClicked: openPage("audio")
                }

                Tile {
                    tileName: "Bluetooth"
                    tileIcon: "\ue1a7"
                    Layout.preferredWidth: settingsGrid._actualTileW
                    Layout.preferredHeight: UiMetrics.tileH
                    onClicked: openPage("connection")
                }

                Tile {
                    tileName: "Companion"
                    tileIcon: "\ue324"
                    Layout.preferredWidth: settingsGrid._actualTileW
                    Layout.preferredHeight: UiMetrics.tileH
                    onClicked: openPage("companion")
                }

                Tile {
                    tileName: "System"
                    tileIcon: "\uf8cd"
                    Layout.preferredWidth: settingsGrid._actualTileW
                    Layout.preferredHeight: UiMetrics.tileH
                    onClicked: openPage("system")
                }
            }
        }
    }

    Component { id: aaPage; AASettings {} }
    Component { id: displayPage; DisplaySettings {} }
    Component { id: audioPage; AudioSettings {} }
    Component { id: connectionPage; ConnectionSettings {} }
    Component { id: systemPage; SystemSettings {} }
    Component { id: companionPage; CompanionSettings {} }
    Component { id: eqDirectComponent; EqSettings {} }

    function openPage(pageId) {
        var titles = {
            "aa": "Android Auto",
            "display": "Display",
            "audio": "Audio",
            "connection": "Bluetooth",
            "system": "System",
            "companion": "Companion"
        }
        var pages = {
            "aa": aaPage,
            "display": displayPage,
            "audio": audioPage,
            "connection": connectionPage,
            "system": systemPage,
            "companion": companionPage
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
            var item = settingsStack.push(pages[pageId])
            if (pageId === "audio" && item && item.openEqualizer) {
                item.openEqualizer.connect(function() {
                    settingsStack.push(eqDirectComponent)
                    ApplicationController.setTitle("Settings > Audio > Equalizer")
                })
            }
        }
    }
}
