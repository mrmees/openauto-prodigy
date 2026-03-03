import QtQuick
import QtQuick.Layouts

Item {
    id: launcherMenu

    GridView {
        id: tileGrid
        anchors.fill: parent
        anchors.margins: UiMetrics.gridGap / 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        readonly property int columns: Math.max(1, Math.floor(width / (UiMetrics.tileW + UiMetrics.gridGap)))
        cellWidth: width / columns
        cellHeight: UiMetrics.tileH + UiMetrics.gridGap

        model: LauncherModel

        delegate: Item {
            width: tileGrid.cellWidth
            height: tileGrid.cellHeight

            Tile {
                anchors.centerIn: parent
                width: tileGrid.cellWidth - UiMetrics.gridGap
                height: tileGrid.cellHeight - UiMetrics.gridGap
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
