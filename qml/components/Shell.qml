import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: shell
    anchors.fill: parent

    // Fullscreen: active plugin requested it (generic — not AA-specific)
    property bool fullscreenMode: PluginModel.activePluginFullscreen

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: navbar.visible && navbar.edge === "top" ? navbar.barThick : 0
        anchors.bottomMargin: navbar.visible && navbar.edge === "bottom" ? navbar.barThick : 0
        anchors.leftMargin: navbar.visible && navbar.edge === "left" ? navbar.barThick : 0
        anchors.rightMargin: navbar.visible && navbar.edge === "right" ? navbar.barThick : 0
        spacing: 0

        // Content area — C++ manages plugin views via PluginViewHost
        Item {
            id: pluginContentHost
            objectName: "pluginContentHost"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // Wallpaper layer — behind all content, visible on launcher only
            Wallpaper {
                anchors.fill: parent
                visible: !PluginModel.activePluginId
                         && ApplicationController.currentApplication !== 6
            }

            // Home screen with widget panes and launcher dock
            HomeMenu {
                id: homeView
                anchors.fill: parent
                visible: !PluginModel.activePluginId
                         && ApplicationController.currentApplication !== 6
            }

            // Settings (built-in screen, not a plugin)
            SettingsMenu {
                id: settingsView
                anchors.fill: parent
                visible: !PluginModel.activePluginId
                         && ApplicationController.currentApplication === 6
            }

            // First-run pairing banner (shown on launcher when no devices paired)
            FirstRunBanner {
                id: firstRunBanner
            }
        }

    }

    // Navbar (floating, any edge)
    Navbar {
        id: navbar
        z: 100
    }

    Connections {
        target: NavbarController
        function onSettingsPageRequested(pageId) {
            settingsView.openPage(pageId)
        }
    }

    // Toast notifications
    NotificationArea {
        id: notificationArea
    }

    // Software brightness dimming overlay
    Rectangle {
        anchors.fill: parent
        color: "black"
        opacity: typeof DisplayService !== "undefined" ? DisplayService.dimOverlayOpacity : 0
        visible: opacity > 0
        z: 998
        enabled: false  // Don't capture mouse/touch events
    }

    // Gesture overlay (on top of everything)
    GestureOverlay {
        id: gestureOverlay
        objectName: "gestureOverlay"
    }

    // Bluetooth pairing confirmation dialog
    PairingDialog {
        id: pairingDialog
    }

    // Incoming call overlay (still uses global PhonePlugin until Priority 3)
    IncomingCallOverlay {
        id: callOverlay
    }
}
