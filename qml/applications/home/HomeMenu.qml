import QtQuick
import QtQuick.Layouts

Item {
    id: homeScreen
    anchors.fill: parent

    Component.onCompleted: ApplicationController.setTitle("Home")

    property bool isLandscape: width > height

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.gap

        // Pane area
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Landscape: main left, subs stacked right
            RowLayout {
                anchors.fill: parent
                spacing: UiMetrics.gap
                visible: homeScreen.isLandscape

                WidgetHost {
                    id: mainPaneLandscape
                    paneId: "main"
                    widgetSource: WidgetPlacementModel.qmlComponentForPane("main")
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width * 0.6
                    onLongPressed: widgetPicker.openForPane("main")
                }

                ColumnLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    spacing: UiMetrics.gap

                    WidgetHost {
                        id: sub1PaneLandscape
                        paneId: "sub1"
                        widgetSource: WidgetPlacementModel.qmlComponentForPane("sub1")
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        onLongPressed: widgetPicker.openForPane("sub1")
                    }

                    WidgetHost {
                        id: sub2PaneLandscape
                        paneId: "sub2"
                        widgetSource: WidgetPlacementModel.qmlComponentForPane("sub2")
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        onLongPressed: widgetPicker.openForPane("sub2")
                    }
                }
            }

            // Portrait: main top, subs side-by-side bottom
            ColumnLayout {
                anchors.fill: parent
                spacing: UiMetrics.gap
                visible: !homeScreen.isLandscape

                WidgetHost {
                    id: mainPanePortrait
                    paneId: "main"
                    widgetSource: WidgetPlacementModel.qmlComponentForPane("main")
                    Layout.fillWidth: true
                    Layout.preferredHeight: parent.height * 0.6
                    onLongPressed: widgetPicker.openForPane("main")
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: UiMetrics.gap

                    WidgetHost {
                        id: sub1PanePortrait
                        paneId: "sub1"
                        widgetSource: WidgetPlacementModel.qmlComponentForPane("sub1")
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        onLongPressed: widgetPicker.openForPane("sub1")
                    }

                    WidgetHost {
                        id: sub2PanePortrait
                        paneId: "sub2"
                        widgetSource: WidgetPlacementModel.qmlComponentForPane("sub2")
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        onLongPressed: widgetPicker.openForPane("sub2")
                    }
                }
            }
        }

        // Launcher dock
        LauncherDock {
            Layout.fillWidth: true
        }
    }

    // Refresh widget sources when placements change
    Connections {
        target: WidgetPlacementModel
        function onPaneChanged(paneId) {
            if (paneId === "main") {
                mainPaneLandscape.widgetSource = WidgetPlacementModel.qmlComponentForPane("main")
                mainPanePortrait.widgetSource = WidgetPlacementModel.qmlComponentForPane("main")
            } else if (paneId === "sub1") {
                sub1PaneLandscape.widgetSource = WidgetPlacementModel.qmlComponentForPane("sub1")
                sub1PanePortrait.widgetSource = WidgetPlacementModel.qmlComponentForPane("sub1")
            } else if (paneId === "sub2") {
                sub2PaneLandscape.widgetSource = WidgetPlacementModel.qmlComponentForPane("sub2")
                sub2PanePortrait.widgetSource = WidgetPlacementModel.qmlComponentForPane("sub2")
            }
        }
    }

    // Widget picker (Task 9 placeholder — just the Loader)
    Loader {
        id: widgetPicker
        anchors.fill: parent
        active: false
        property string targetPaneId: ""

        function openForPane(paneId) {
            targetPaneId = paneId
            active = true
        }
    }
}
