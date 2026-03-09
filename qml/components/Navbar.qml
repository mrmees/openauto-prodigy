import QtQuick
import QtQuick.Layouts

/// Main navbar: 3 controls (volume/clock/brightness) with edge-aware layout.
/// Floats on top of shell content at the configured edge position.
Item {
    id: navbar

    property string edge: NavbarController.edge
    property bool isVertical: edge === "left" || edge === "right"
    property int barThick: UiMetrics.navbarThick

    // When AA is projecting, blend navbar with its black status bar
    readonly property bool aaActive: PluginModel.activePluginId === "org.openauto.android-auto"
    readonly property color barBg: aaActive ? "#000000" : ThemeService.surfaceContainer
    readonly property color barFg: aaActive ? "#FFFFFF" : ThemeService.onSurface
    readonly property color barShadowColor: aaActive ? "#000000" : ThemeService.barShadow

    // Hidden during fullscreen plugin (AA)
    visible: !PluginModel.activePluginFullscreen
    z: 100

    // --- Dynamic icon resolution based on service state ---
    readonly property string volumeIcon: {
        if (typeof AudioService === "undefined") return "\ue050"
        var vol = AudioService.masterVolume
        if (vol <= 0) return "\ue04f"       // volume_off
        if (vol < 40) return "\ue04d"       // volume_down
        return "\ue050"                     // volume_up
    }
    readonly property string brightnessIcon: {
        if (typeof ThemeService === "undefined") return "\ue1ac"
        return ThemeService.nightMode ? "\ue51c" : "\ue518"  // dark_mode : light_mode
    }

    // --- Control role mapping: index 0=driver, 1=center, 2=passenger ---
    readonly property string control0Icon: NavbarController.leftHandDrive ? volumeIcon : brightnessIcon
    readonly property string control2Icon: NavbarController.leftHandDrive ? brightnessIcon : volumeIcon
    readonly property bool control0IsVolume: NavbarController.leftHandDrive
    readonly property bool control2IsVolume: !NavbarController.leftHandDrive

    // --- Shadow ---
    Rectangle {
        id: shadow
        anchors.fill: barBackground
        color: navbar.barShadowColor
        radius: 0
    }

    // --- Background ---
    Rectangle {
        id: barBackground
        anchors.fill: parent
        color: navbar.barBg
    }

    // --- Controls container ---
    // Horizontal layout for top/bottom
    Row {
        id: horizLayout
        anchors.fill: parent
        visible: !navbar.isVertical

        NavbarControl {
            id: hControl0
            controlIndex: 0
            iconText: navbar.control0Icon
            showClock: false
            isVertical: false
            width: parent.width * 0.15
            height: parent.height
        }
        NavbarControl {
            id: hControl1
            controlIndex: 1
            showClock: true
            isVertical: false
            width: parent.width * 0.70
            height: parent.height
            phoneBattery: typeof AAOrchestrator !== "undefined" ? AAOrchestrator.phoneBatteryLevel : -1
            phoneSignal: typeof AAOrchestrator !== "undefined" ? AAOrchestrator.phoneSignalStrength : -1
            phoneConnected: typeof AAOrchestrator !== "undefined" ? AAOrchestrator.aaConnected : false
        }
        NavbarControl {
            id: hControl2
            controlIndex: 2
            iconText: navbar.control2Icon
            showClock: false
            isVertical: false
            width: parent.width * 0.15
            height: parent.height
        }
    }

    // Vertical layout for left/right
    Column {
        id: vertLayout
        anchors.fill: parent
        visible: navbar.isVertical

        NavbarControl {
            id: vControl0
            controlIndex: 0
            iconText: navbar.control0Icon
            showClock: false
            isVertical: true
            width: parent.width
            height: parent.height * 0.15
        }
        NavbarControl {
            id: vControl1
            controlIndex: 1
            showClock: true
            isVertical: true
            width: parent.width
            height: parent.height * 0.70
            phoneBattery: typeof AAOrchestrator !== "undefined" ? AAOrchestrator.phoneBatteryLevel : -1
            phoneSignal: typeof AAOrchestrator !== "undefined" ? AAOrchestrator.phoneSignalStrength : -1
            phoneConnected: typeof AAOrchestrator !== "undefined" ? AAOrchestrator.aaConnected : false
        }
        NavbarControl {
            id: vControl2
            controlIndex: 2
            iconText: navbar.control2Icon
            showClock: false
            isVertical: true
            width: parent.width
            height: parent.height * 0.15
        }
    }

    // --- Popup sliders ---
    NavbarPopup {
        id: popup0
        controlIndex: 0
        isVolume: navbar.control0IsVolume
        anchorItem: navbar.isVertical ? vControl0 : hControl0
        navbarEdge: navbar.edge
        parent: navbar.parent  // render in shell space, not navbar space
    }

    NavbarPopup {
        id: popup2
        controlIndex: 2
        isVolume: navbar.control2IsVolume
        anchorItem: navbar.isVertical ? vControl2 : hControl2
        navbarEdge: navbar.edge
        parent: navbar.parent
    }

    // --- Power menu popup (clock long-hold) ---
    property int powerMenuGeneration: -1

    Item {
        id: powerMenu
        visible: NavbarController.popupVisible && NavbarController.popupControlIndex === 1
        z: 200
        parent: navbar.parent

        onVisibleChanged: {
            if (visible) {
                var anchorCtrl = navbar.isVertical ? vControl1 : hControl1
                var pos = anchorCtrl.mapToItem(powerMenu.parent, 0, 0)
                var centerX = pos.x + anchorCtrl.width / 2 - powerMenuBg.width / 2

                if (navbar.edge === "bottom") {
                    powerMenu.x = centerX
                    powerMenu.y = pos.y - powerMenuBg.height - UiMetrics.spacing
                } else if (navbar.edge === "top") {
                    powerMenu.x = centerX
                    powerMenu.y = pos.y + anchorCtrl.height + UiMetrics.spacing
                } else if (navbar.edge === "left") {
                    powerMenu.x = pos.x + anchorCtrl.width + UiMetrics.spacing
                    powerMenu.y = pos.y + anchorCtrl.height / 2 - powerMenuBg.height / 2
                } else {
                    powerMenu.x = pos.x - powerMenuBg.width - UiMetrics.spacing
                    powerMenu.y = pos.y + anchorCtrl.height / 2 - powerMenuBg.height / 2
                }

                // Clamp
                if (powerMenu.x < 0) powerMenu.x = 0
                if (powerMenu.y < 0) powerMenu.y = 0

                // Report button geometry for evdev zones
                navbar.powerMenuGeneration = NavbarController.beginPopupSession(1)
                Qt.callLater(function() {
                    var regions = []
                    var actions = ["minimize", "restart", "quit"]
                    for (var i = 0; i < powerMenuCol.children.length; i++) {
                        var btn = powerMenuCol.children[i]
                        if (btn.visible && btn.width > 0 && i < actions.length) {
                            var btnPos = btn.mapToItem(navbar.parent, 0, 0)
                            regions.push({
                                id: "btn-" + actions[i],
                                type: 1,  // Button
                                x: btnPos.x,
                                y: btnPos.y,
                                w: btn.width,
                                h: btn.height,
                                action: actions[i]
                            })
                        }
                    }
                    NavbarController.setPopupRegions(1, navbar.powerMenuGeneration, regions)
                })
            } else if (navbar.powerMenuGeneration >= 0) {
                NavbarController.clearPopupRegions(1, navbar.powerMenuGeneration)
                navbar.powerMenuGeneration = -1
            }
        }

        width: powerMenuBg.width
        height: powerMenuBg.height

        Rectangle {
            id: powerMenuBg
            width: Math.round(160 * UiMetrics.scale)
            height: powerMenuCol.implicitHeight + UiMetrics.spacing * 2
            color: navbar.aaActive ? "#1A1A1A" : ThemeService.surfaceContainerHigh
            radius: UiMetrics.radius
            border.color: navbar.aaActive ? "#333333" : ThemeService.outlineVariant
            border.width: 1

            MouseArea {
                anchors.fill: parent
                onPressed: function(mouse) { mouse.accepted = true }
            }

            Column {
                id: powerMenuCol
                anchors.centerIn: parent
                spacing: UiMetrics.spacing

                Repeater {
                    model: [
                        { label: "Minimize", icon: "\ue5d0", action: "minimize" },
                        { label: "Restart", icon: "\ue5d5", action: "restart" },
                        { label: "Quit", icon: "\ue5cd", action: "quit" }
                    ]

                    Rectangle {
                        width: powerMenuBg.width - UiMetrics.spacing * 2
                        height: UiMetrics.touchMin
                        radius: UiMetrics.radiusSmall
                        color: pmMouseArea.pressed ? (navbar.aaActive ? "#333333" : ThemeService.primaryContainer) : "transparent"

                        Row {
                            anchors.centerIn: parent
                            spacing: UiMetrics.spacing

                            MaterialIcon {
                                icon: modelData.icon
                                size: UiMetrics.iconSize
                                color: navbar.barFg
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: modelData.label
                                font.pixelSize: UiMetrics.fontBody
                                color: navbar.barFg
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            id: pmMouseArea
                            anchors.fill: parent
                            onClicked: {
                                NavbarController.hidePopup()
                                Qt.callLater(function() {
                                    if (modelData.action === "minimize")
                                        ApplicationController.minimize()
                                    else if (modelData.action === "restart")
                                        ApplicationController.restart()
                                    else if (modelData.action === "quit")
                                        ApplicationController.quit()
                                })
                            }
                        }
                    }
                }
            }
        }
    }

    // --- Dismiss overlay (tapping outside popup closes it) ---
    MouseArea {
        parent: navbar.parent
        anchors.fill: parent
        visible: NavbarController.popupVisible
        z: 150
        onClicked: NavbarController.hidePopup()
    }

    // --- Edge states with AnchorChanges ---
    states: [
        State {
            name: "bottom"
            when: navbar.edge === "bottom"
            AnchorChanges {
                target: navbar
                anchors.bottom: navbar.parent.bottom
                anchors.left: navbar.parent.left
                anchors.right: navbar.parent.right
                anchors.top: undefined
            }
            PropertyChanges { target: navbar; height: barThick }
        },
        State {
            name: "top"
            when: navbar.edge === "top"
            AnchorChanges {
                target: navbar
                anchors.top: navbar.parent.top
                anchors.left: navbar.parent.left
                anchors.right: navbar.parent.right
                anchors.bottom: undefined
            }
            PropertyChanges { target: navbar; height: barThick }
        },
        State {
            name: "left"
            when: navbar.edge === "left"
            AnchorChanges {
                target: navbar
                anchors.top: navbar.parent.top
                anchors.bottom: navbar.parent.bottom
                anchors.left: navbar.parent.left
                anchors.right: undefined
            }
            PropertyChanges { target: navbar; width: barThick }
        },
        State {
            name: "right"
            when: navbar.edge === "right"
            AnchorChanges {
                target: navbar
                anchors.top: navbar.parent.top
                anchors.bottom: navbar.parent.bottom
                anchors.right: navbar.parent.right
                anchors.left: undefined
            }
            PropertyChanges { target: navbar; width: barThick }
        }
    ]

    // Report geometry changes to NavbarController for zone registration
    Component.onCompleted: Qt.callLater(registerZones)
    onXChanged: Qt.callLater(registerZones)
    onYChanged: Qt.callLater(registerZones)
    onWidthChanged: Qt.callLater(registerZones)
    onHeightChanged: Qt.callLater(registerZones)
    onEdgeChanged: Qt.callLater(registerZones)
    onVisibleChanged: Qt.callLater(registerZones)

    function registerZones() {
        if (!navbar.visible) {
            NavbarController.unregisterZones()
            return
        }
        if (navbar.parent && navbar.parent.width > 0 && navbar.parent.height > 0) {
            NavbarController.registerZones(navbar.parent.width, navbar.parent.height)
        }
    }
}
