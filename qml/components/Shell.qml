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
        spacing: 0

        // Status bar — ~8% height, hidden in fullscreen
        TopBar {
            Layout.fillWidth: true
            Layout.preferredHeight: shell.fullscreenMode ? 0 : shell.height * 0.08
            visible: !shell.fullscreenMode
        }

        // Content area — C++ manages plugin views via PluginViewHost
        Item {
            id: pluginContentHost
            objectName: "pluginContentHost"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // Launcher is shown when no plugin view is loaded and not in a built-in screen
            LauncherMenu {
                id: launcherView
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

        // Nav strip — plugin-model driven, hidden in fullscreen
        NavStrip {
            Layout.fillWidth: true
            Layout.preferredHeight: shell.fullscreenMode ? 0 : shell.height * 0.12
            visible: !shell.fullscreenMode
        }
    }

    // Toast notifications
    NotificationArea {
        id: notificationArea
    }

    // Gesture overlay (on top of everything)
    GestureOverlay {
        id: gestureOverlay
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
