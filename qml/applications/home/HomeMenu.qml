import QtQuick
import QtQuick.Layouts

Item {
    id: homeScreen
    anchors.fill: parent

    Component.onCompleted: ApplicationController.setTitle("Home")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        // Grid container -- fills available space above dock
        Item {
            id: gridContainer
            Layout.fillWidth: true
            Layout.fillHeight: true

            readonly property real cellWidth: width / WidgetGridModel.gridColumns
            readonly property real cellHeight: height / WidgetGridModel.gridRows

            Repeater {
                model: WidgetGridModel

                delegate: Item {
                    x: model.column * gridContainer.cellWidth
                    y: model.row * gridContainer.cellHeight
                    width: model.colSpan * gridContainer.cellWidth
                    height: model.rowSpan * gridContainer.cellHeight
                    visible: model.visible

                    // Inner content with gutter margins (half-spacing per side = full spacing between neighbors)
                    Item {
                        anchors.fill: parent
                        anchors.margins: UiMetrics.spacing / 2

                        // Glass card background
                        Rectangle {
                            anchors.fill: parent
                            radius: UiMetrics.radius
                            color: ThemeService.surfaceContainer
                            opacity: model.opacity
                        }

                        // Widget content
                        Loader {
                            anchors.fill: parent
                            source: model.qmlComponent
                            asynchronous: false

                            onStatusChanged: {
                                if (status === Loader.Error)
                                    console.warn("HomeMenu: failed to load widget", model.qmlComponent)
                            }
                        }
                    }
                }
            }
        }

        // Launcher dock -- fixed at bottom
        LauncherDock {
            Layout.fillWidth: true
        }
    }
}
