import QtQuick
import QtQuick.Layouts

Item {
    id: launcherMenu

    GridLayout {
        anchors.centerIn: parent
        columns: 4
        rowSpacing: 20
        columnSpacing: 20

        Repeater {
            model: LauncherModel

            Tile {
                Layout.preferredWidth: launcherMenu.width * 0.18
                Layout.preferredHeight: launcherMenu.width * 0.18
                tileName: model.tileLabel
                tileIcon: model.tileIcon
                tileEnabled: true
                onClicked: {
                    var action = model.tileAction;
                    if (action.startsWith("plugin:")) {
                        PluginModel.setActivePlugin(action.substring(7));
                    } else if (action.startsWith("navigate:")) {
                        var target = action.substring(9);
                        if (target === "settings") {
                            ApplicationController.navigateTo(6);
                        }
                    }
                }
            }
        }
    }
}
