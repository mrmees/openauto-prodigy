import QtQuick

Item {
    id: widgetHost

    property url widgetSource: ""
    property real hostOpacity: 0.25

    signal longPressed()

    // Glass card background
    Rectangle {
        anchors.fill: parent
        radius: UiMetrics.radius
        color: ThemeService.surfaceContainer
        opacity: widgetHost.hostOpacity
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

    // Long-press detector -- sits behind widget content (z: -1).
    // Non-interactive widgets (Clock) fall through to this naturally.
    // Interactive widgets with their own MouseAreas must also detect
    // pressAndHold and call widgetHost.longPressed() -- see AAStatusWidget.
    MouseArea {
        anchors.fill: parent
        z: -1
        pressAndHoldInterval: 500
        onPressAndHold: widgetHost.longPressed()
    }
}
