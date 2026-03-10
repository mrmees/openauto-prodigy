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

    // Long-press detector overlay
    MouseArea {
        anchors.fill: parent
        pressAndHoldInterval: 500
        propagateComposedEvents: true
        onPressAndHold: widgetHost.longPressed()
        onClicked: function(mouse) { mouse.accepted = false }
        onPressed: function(mouse) { mouse.accepted = false }
        onReleased: function(mouse) { mouse.accepted = false }
    }
}
