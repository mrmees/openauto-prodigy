import QtQuick

Item {
    id: widgetHost

    property string paneId: ""
    property url widgetSource: ""
    property bool isMainPane: paneId === "main"

    // Long-press detection for widget picker
    signal longPressed()

    Loader {
        id: widgetLoader
        anchors.fill: parent
        source: widgetHost.widgetSource
        asynchronous: false

        onStatusChanged: {
            if (status === Loader.Error)
                console.warn("WidgetHost: failed to load", widgetHost.widgetSource)
        }
    }

    // Empty state
    Rectangle {
        anchors.fill: parent
        color: ThemeService.surfaceContainer
        radius: UiMetrics.radius
        visible: !widgetHost.widgetSource.toString()
        opacity: 0.5
    }

    // Long-press detector — sits behind widget content.
    // Widget MouseAreas (e.g. AA Status tap-to-connect) take priority.
    // Empty panes and non-interactive widgets fall through to this.
    MouseArea {
        anchors.fill: parent
        z: -1
        pressAndHoldInterval: 500
        onPressAndHold: widgetHost.longPressed()
    }
}
