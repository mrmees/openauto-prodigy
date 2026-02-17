import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Window {
    id: root
    width: 1024
    height: 600
    visible: true
    title: "OpenAuto Prodigy"
    color: ThemeController.backgroundColor

    // Hide bars when Android Auto is active and connected
    property bool aaFullscreen: ApplicationController.currentApplication === 2
                                && AndroidAutoService.connectionState === 3

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top bar — 10% height (hidden in AA fullscreen)
        TopBar {
            Layout.fillWidth: true
            Layout.preferredHeight: root.aaFullscreen ? 0 : root.height * 0.10
            visible: !root.aaFullscreen
        }

        // Content area — fills remaining space
        StackView {
            id: contentStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            initialItem: launcherComponent
        }

        // Bottom bar — 12% height (hidden in AA fullscreen)
        BottomBar {
            Layout.fillWidth: true
            Layout.preferredHeight: root.aaFullscreen ? 0 : root.height * 0.12
            visible: !root.aaFullscreen
        }
    }

    // Application screen components
    Component {
        id: launcherComponent
        LauncherMenu {}
    }

    Component {
        id: homeComponent
        HomeMenu {}
    }

    Component {
        id: androidAutoComponent
        AndroidAutoMenu {}
    }

    Component {
        id: settingsComponent
        SettingsMenu {}
    }

    // Map app type to component
    function componentForApp(appType) {
        switch (appType) {
        case 0: return launcherComponent;   // Launcher
        case 1: return homeComponent;       // Home
        case 2: return androidAutoComponent; // AndroidAuto
        case 6: return settingsComponent;   // Settings
        default: return launcherComponent;
        }
    }

    // React to navigation changes — always replace to stay in sync with C++ nav stack
    Connections {
        target: ApplicationController
        function onCurrentApplicationChanged() {
            var appType = ApplicationController.currentApplication;
            var component = componentForApp(appType);
            contentStack.replace(component);
        }
    }

    // Keyboard shortcut: Ctrl+D toggles day/night theme
    Shortcut {
        sequence: "Ctrl+D"
        onActivated: ThemeController.toggleMode()
    }
}
