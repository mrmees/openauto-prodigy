import QtQuick

Item {
    id: widgetHost

    property string paneId: ""
    property url widgetSource: ""
    property bool isMainPane: paneId === "main"
    property real paneOpacity: WidgetPlacementModel.paneOpacity(paneId)

    signal longPressed()

    // Glass card background — always visible
    Rectangle {
        anchors.fill: parent
        radius: UiMetrics.radius
        color: ThemeService.surfaceContainer
        opacity: widgetHost.paneOpacity
    }

    // Subtle border for edge definition
    Rectangle {
        anchors.fill: parent
        radius: UiMetrics.radius
        color: "transparent"
        border.width: 1
        border.color: ThemeService.outlineVariant
        opacity: 0.3
    }

    Loader {
        id: widgetLoader
        anchors.fill: parent
        source: widgetHost.widgetSource
        asynchronous: false

        // Expose a function widgets can call for long-press forwarding.
        // Widgets with their own MouseAreas (e.g. AAStatusWidget) call
        // parent.requestContextMenu() from onPressAndHold.
        function requestContextMenu() { widgetHost.longPressed() }

        onStatusChanged: {
            if (status === Loader.Error)
                console.warn("WidgetHost: failed to load", widgetHost.widgetSource)
        }
    }

    // Empty state hint
    NormalText {
        anchors.centerIn: parent
        text: "Hold to add widget"
        font.pixelSize: UiMetrics.fontSmall
        color: ThemeService.onSurfaceVariant
        opacity: 0.5
        visible: !widgetHost.widgetSource.toString()
    }

    // Long-press detector — sits behind widget content (z: -1).
    // Non-interactive widgets (Clock) fall through to this naturally.
    // Interactive widgets with their own MouseAreas must also detect
    // pressAndHold and call widgetHost.longPressed() — see AAStatusWidget.
    MouseArea {
        anchors.fill: parent
        z: -1
        pressAndHoldInterval: 500
        onPressAndHold: widgetHost.longPressed()
    }

    // Refresh opacity when placement changes
    Connections {
        target: WidgetPlacementModel
        function onPaneChanged(changedPaneId) {
            if (changedPaneId === widgetHost.paneId)
                widgetHost.paneOpacity = WidgetPlacementModel.paneOpacity(widgetHost.paneId)
        }
    }
}
