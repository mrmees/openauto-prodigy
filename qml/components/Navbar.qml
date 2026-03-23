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
    // Widget interaction mode: gear (\ue8b8) replaces volume, trash (\ue872) replaces brightness
    readonly property string control0Icon: {
        if (NavbarController.widgetInteractionMode) {
            return NavbarController.leftHandDrive ? "\ue8b8" : "\ue872"
        }
        return NavbarController.leftHandDrive ? volumeIcon : brightnessIcon
    }
    readonly property string control2Icon: {
        if (NavbarController.widgetInteractionMode) {
            return NavbarController.leftHandDrive ? "\ue872" : "\ue8b8"
        }
        return NavbarController.leftHandDrive ? brightnessIcon : volumeIcon
    }
    readonly property bool control0IsVolume: NavbarController.leftHandDrive
    readonly property bool control2IsVolume: !NavbarController.leftHandDrive

    // Dimming for gear (no config schema) and trash (singleton)
    property string widgetDisplayName: NavbarController.widgetDisplayName
    property bool gearEnabled: NavbarController.selectedWidgetHasConfig
    property bool trashEnabled: !NavbarController.selectedWidgetIsSingleton

    // --- Gradient fade (replaces old 1px barShadow border) ---
    // Renders outside the navbar on its content-facing edge.
    // Parented to navbar.parent so it draws in the content area.
    Rectangle {
        id: navGradient
        visible: navbar.visible && !navbar.aaActive
        z: 99  // Just below navbar (z:100)
        parent: navbar.parent

        gradient: Gradient {
            orientation: navbar.isVertical ? Gradient.Horizontal : Gradient.Vertical
            GradientStop {
                id: gradStop0
                position: 0.0
                color: Qt.rgba(navbar.barBg.r, navbar.barBg.g, navbar.barBg.b, 0.8)
            }
            GradientStop {
                id: gradStop1
                position: 1.0
                color: "transparent"
            }
        }

        states: [
            State {
                name: "bottom"
                when: navbar.edge === "bottom"
                PropertyChanges {
                    target: navGradient
                    x: navbar.x
                    y: navbar.y - Math.round(10 * UiMetrics.scale)
                    width: navbar.width
                    height: Math.round(10 * UiMetrics.scale)
                }
                // Gradient: navbar color at bottom (pos 1.0), transparent at top (pos 0.0)
                PropertyChanges { target: gradStop0; color: "transparent" }
                PropertyChanges { target: gradStop1; color: Qt.rgba(navbar.barBg.r, navbar.barBg.g, navbar.barBg.b, 0.8) }
            },
            State {
                name: "top"
                when: navbar.edge === "top"
                PropertyChanges {
                    target: navGradient
                    x: navbar.x
                    y: navbar.y + navbar.height
                    width: navbar.width
                    height: Math.round(10 * UiMetrics.scale)
                }
                // Gradient: navbar color at top (pos 0.0), transparent at bottom (pos 1.0)
                PropertyChanges { target: gradStop0; color: Qt.rgba(navbar.barBg.r, navbar.barBg.g, navbar.barBg.b, 0.8) }
                PropertyChanges { target: gradStop1; color: "transparent" }
            },
            State {
                name: "left"
                when: navbar.edge === "left"
                PropertyChanges {
                    target: navGradient
                    x: navbar.x + navbar.width
                    y: navbar.y
                    width: Math.round(10 * UiMetrics.scale)
                    height: navbar.height
                }
                // Gradient: navbar color at left (pos 0.0), transparent at right (pos 1.0)
                PropertyChanges { target: gradStop0; color: Qt.rgba(navbar.barBg.r, navbar.barBg.g, navbar.barBg.b, 0.8) }
                PropertyChanges { target: gradStop1; color: "transparent" }
            },
            State {
                name: "right"
                when: navbar.edge === "right"
                PropertyChanges {
                    target: navGradient
                    x: navbar.x - Math.round(10 * UiMetrics.scale)
                    y: navbar.y
                    width: Math.round(10 * UiMetrics.scale)
                    height: navbar.height
                }
                // Gradient: navbar color at right (pos 1.0), transparent at left (pos 0.0)
                PropertyChanges { target: gradStop0; color: "transparent" }
                PropertyChanges { target: gradStop1; color: Qt.rgba(navbar.barBg.r, navbar.barBg.g, navbar.barBg.b, 0.8) }
            }
        ]
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
            width: parent.width * 0.20
            height: parent.height
            controlEnabled: {
                if (!NavbarController.widgetInteractionMode) return true
                return NavbarController.leftHandDrive ? navbar.gearEnabled : navbar.trashEnabled
            }
        }
        NavbarControl {
            id: hControl1
            controlIndex: 1
            showClock: true
            isVertical: false
            width: parent.width * 0.60
            height: parent.height
        }
        NavbarControl {
            id: hControl2
            controlIndex: 2
            iconText: navbar.control2Icon
            showClock: false
            isVertical: false
            width: parent.width * 0.20
            height: parent.height
            controlEnabled: {
                if (!NavbarController.widgetInteractionMode) return true
                return NavbarController.leftHandDrive ? navbar.trashEnabled : navbar.gearEnabled
            }
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
            height: parent.height * 0.20
            controlEnabled: {
                if (!NavbarController.widgetInteractionMode) return true
                return NavbarController.leftHandDrive ? navbar.gearEnabled : navbar.trashEnabled
            }
        }
        NavbarControl {
            id: vControl1
            controlIndex: 1
            showClock: true
            isVertical: true
            width: parent.width
            height: parent.height * 0.60
        }
        NavbarControl {
            id: vControl2
            controlIndex: 2
            iconText: navbar.control2Icon
            showClock: false
            isVertical: true
            width: parent.width
            height: parent.height * 0.20
            controlEnabled: {
                if (!NavbarController.widgetInteractionMode) return true
                return NavbarController.leftHandDrive ? navbar.trashEnabled : navbar.gearEnabled
            }
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
                    var actions = ["minimize", "restart", "exit"]
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
                        { label: "Exit", icon: "\ue5cd", action: "exit" }
                    ]

                    ElevatedButton {
                        width: powerMenuBg.width - UiMetrics.spacing * 2
                        height: UiMetrics.touchMin
                        text: modelData.label
                        iconCode: modelData.icon
                        buttonScale: 0.97
                        buttonColor: navbar.aaActive ? "#1A1A1A" : ThemeService.surfaceContainerLow
                        pressedColor: navbar.aaActive ? "#333333" : ThemeService.primaryContainer
                        textColor: navbar.barFg
                        pressedTextColor: navbar.aaActive ? "#FFFFFF" : ThemeService.onPrimaryContainer
                        elevation: 2

                        onClicked: {
                            NavbarController.hidePopup()
                            Qt.callLater(function() {
                                if (modelData.action === "minimize")
                                    ApplicationController.minimize()
                                else if (modelData.action === "restart")
                                    ApplicationController.restart()
                                else if (modelData.action === "exit")
                                    ApplicationController.exitToDesktop()
                            })
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
