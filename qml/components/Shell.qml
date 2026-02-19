import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: shell
    anchors.fill: parent

    // Fullscreen mode: active plugin requested fullscreen AND is connected
    // Currently hardcoded to AA; will be generic once plugin system drives this
    property bool fullscreenMode: ApplicationController.currentApplication === 2
                                  && AndroidAutoService.connectionState === 3

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Status bar — ~8% height, hidden in fullscreen
        TopBar {
            Layout.fillWidth: true
            Layout.preferredHeight: shell.fullscreenMode ? 0 : shell.height * 0.08
            visible: !shell.fullscreenMode
        }

        // Content area — fills remaining space
        StackView {
            id: contentStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            initialItem: launcherComponent
        }

        // Nav strip — ~12% height, hidden in fullscreen
        // Currently uses the old BottomBar; will be replaced by
        // PluginModel-driven nav strip in Task 13
        BottomBar {
            Layout.fillWidth: true
            Layout.preferredHeight: shell.fullscreenMode ? 0 : shell.height * 0.12
            visible: !shell.fullscreenMode
        }
    }

    // Application screen components (will be replaced by plugin QML loader)
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

    function componentForApp(appType) {
        switch (appType) {
        case 0: return launcherComponent;   // Launcher
        case 1: return homeComponent;       // Home
        case 2: return androidAutoComponent; // AndroidAuto
        case 6: return settingsComponent;   // Settings
        default: return launcherComponent;
        }
    }

    // React to navigation changes
    Connections {
        target: ApplicationController
        function onCurrentApplicationChanged() {
            var appType = ApplicationController.currentApplication;
            var component = componentForApp(appType);
            contentStack.replace(component);
        }
    }
}
