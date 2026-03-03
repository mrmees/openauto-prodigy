import QtQuick
import QtQuick.Layouts

Item {
    id: launcherMenu

    GridLayout {
        anchors.centerIn: parent
        columns: 4
        rowSpacing: UiMetrics.gridGap
        columnSpacing: UiMetrics.gridGap

        Repeater {
            model: LauncherModel

            Tile {
                Layout.preferredWidth: UiMetrics.tileW
                Layout.preferredHeight: UiMetrics.tileH
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
                            PluginModel.setActivePlugin("");
                            ApplicationController.navigateTo(6);
                        }
                    }
                }
            }
        }
    }
}
