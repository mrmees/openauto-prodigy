import QtQuick
import QtQuick.Layouts

Item {
    id: aaFocusWidget
    clip: true

    property QtObject widgetContext: null
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1

    // Projection state from widgetContext provider
    readonly property int projState: widgetContext && widgetContext.projectionStatus
        ? widgetContext.projectionStatus.projectionState : 0
    readonly property bool aaConnected: projState >= 3      // Connected(3) or Backgrounded(4)

    opacity: aaConnected ? 1.0 : 0.4

    // Icon only, scales with layout
    MaterialIcon {
        anchors.centerIn: parent
        icon: aaFocusWidget.aaConnected ? "\ue859" : "\uf034"  // android / mobiledata_off
        size: Math.min(parent.width, parent.height) * 0.6
        color: aaFocusWidget.aaConnected ? ThemeService.primary : ThemeService.onSurfaceVariant
    }

    MouseArea {
        anchors.fill: parent
        pressAndHoldInterval: 500
        onClicked: {
            if (aaFocusWidget.aaConnected)
                ActionRegistry.dispatch("aa.requestFocus")
            // else: not connected, ignore tap
        }
        onPressAndHold: {
            if (aaFocusWidget.parent && aaFocusWidget.parent.requestContextMenu)
                aaFocusWidget.parent.requestContextMenu()
        }
    }
}
