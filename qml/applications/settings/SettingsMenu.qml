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

        // Swipe-right-to-go-back: left-edge drag zone
        MouseArea {
            id: swipeEdge
            width: UiMetrics.spacing * 6
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            property real startX: 0
            property real startY: 0
            property bool fired: false

            onPressed: function(mouse) {
                startX = mouse.x
                startY = mouse.y
                fired = false
            }
            onPositionChanged: function(mouse) {
                if (fired) return
                var dx = mouse.x - startX
                var dy = mouse.y - startY
                if (dx > UiMetrics.spacing * 6 && dx > Math.abs(dy) * 2) {
                    fired = true
                    if (settingsStack.depth > 1) {
                        settingsStack.pop()
                        if (settingsStack.depth > 1) {
                            var item = settingsStack.currentItem
                            if (item && item.objectName) {
                                ApplicationController.setTitle("Settings > " + item.objectName)
                            } else {
                                ApplicationController.setTitle("Settings")
                            }
                        } else {
                            ApplicationController.setTitle("Settings")
                        }
                    } else {
                        ApplicationController.navigateBack()
                    }
                }
            }
        }

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

            ListView {
                anchors.fill: parent
                clip: true
                flickableDirection: Flickable.VerticalFlick

                model: ListModel {
                    ListElement { name: "Android Auto"; icon: "\ue531"; pageId: "aa" }
                    ListElement { name: "Display"; icon: "\ueb97"; pageId: "display" }
                    ListElement { name: "Audio"; icon: "\ue050"; pageId: "audio" }
                    ListElement { name: "Bluetooth"; icon: "\ue1a7"; pageId: "connection" }
                    ListElement { name: "Theme"; icon: "\ue40a"; pageId: "theme" }
                    ListElement { name: "Companion"; icon: "\ue324"; pageId: "companion" }
                    ListElement { name: "System"; icon: "\uf8cd"; pageId: "system" }
                    ListElement { name: "Information"; icon: "\ue88e"; pageId: "information" }
                    ListElement { name: "Debug"; icon: "\ue868"; pageId: "debug" }
                }

                delegate: Rectangle {
                    width: ListView.view.width
                    height: UiMetrics.tileH * 0.55
                    color: delegateArea.pressed
                          ? ThemeService.primaryContainer
                          : (index % 2 === 0 ? ThemeService.surfaceContainer : ThemeService.surfaceContainerHigh)

                    MouseArea {
                        id: delegateArea
                        anchors.fill: parent
                        onClicked: openPage(model.pageId)
                    }

                    MaterialIcon {
                        id: rowIcon
                        icon: model.icon
                        size: parent.height * 0.7
                        color: ThemeService.onSurface
                        anchors.left: parent.left
                        anchors.leftMargin: UiMetrics.spacing * 3
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: model.name
                        font.pixelSize: Math.round(parent.height * 0.45)
                        color: ThemeService.onSurface
                        anchors.right: parent.right
                        anchors.rightMargin: UiMetrics.spacing * 3
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }

    Component { id: aaPage; AASettings {} }
    Component { id: displayPage; DisplaySettings {} }
    Component { id: audioPage; AudioSettings {} }
    Component { id: connectionPage; ConnectionSettings {} }
    Component { id: themePage; ThemeSettings {} }
    Component { id: systemPage; SystemSettings {} }
    Component { id: companionPage; CompanionSettings {} }
    Component { id: informationPage; InformationSettings {} }
    Component { id: debugPage; DebugSettings {} }
    Component { id: eqDirectComponent; EqSettings {} }

    function openPage(pageId) {
        var titles = {
            "aa": "Android Auto",
            "display": "Display",
            "audio": "Audio",
            "connection": "Bluetooth",
            "theme": "Theme",
            "system": "System",
            "companion": "Companion",
            "information": "Information",
            "debug": "Debug"
        }
        var pages = {
            "aa": aaPage,
            "display": displayPage,
            "audio": audioPage,
            "connection": connectionPage,
            "theme": themePage,
            "system": systemPage,
            "companion": companionPage,
            "information": informationPage,
            "debug": debugPage
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
