import QtQuick
import QtQuick.Layouts

Item {
    id: aaFocusWidget

    property QtObject widgetContext: null
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1

    // Projection state from widgetContext provider
    readonly property int projState: widgetContext && widgetContext.projectionStatus
        ? widgetContext.projectionStatus.projectionState : 0
    readonly property bool aaConnected: projState >= 3      // Connected(3) or Backgrounded(4)
    readonly property bool isProjected: projState === 3     // Connected = projected
    readonly property bool isBackgrounded: projState === 4  // Backgrounded = native/car mode

    opacity: aaConnected ? 1.0 : 0.4

    ColumnLayout {
        anchors.centerIn: parent
        spacing: UiMetrics.spacing * 0.5

        MaterialIcon {
            icon: aaFocusWidget.isProjected ? "\ue325" : "\ueff7"  // smartphone : directions_car
            size: UiMetrics.iconSize * 2
            color: {
                if (aaFocusWidget.isProjected)
                    return ThemeService.primary
                if (aaFocusWidget.isBackgrounded)
                    return ThemeService.onSurface
                return ThemeService.onSurfaceVariant
            }
            Layout.alignment: Qt.AlignHCenter
        }

        NormalText {
            text: {
                if (aaFocusWidget.isProjected)
                    return "AA"
                if (aaFocusWidget.isBackgrounded)
                    return "Car"
                return "Off"
            }
            font.pixelSize: UiMetrics.fontSmall
            color: {
                if (aaFocusWidget.isProjected)
                    return ThemeService.primary
                if (aaFocusWidget.isBackgrounded)
                    return ThemeService.onSurface
                return ThemeService.onSurfaceVariant
            }
            Layout.alignment: Qt.AlignHCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        pressAndHoldInterval: 500
        onClicked: {
            if (aaFocusWidget.isProjected)
                ActionRegistry.dispatch("aa.exitToCar")
            else if (aaFocusWidget.isBackgrounded)
                ActionRegistry.dispatch("aa.requestFocus")
            // else: not connected, ignore tap
        }
        onPressAndHold: {
            if (aaFocusWidget.parent && aaFocusWidget.parent.requestContextMenu)
                aaFocusWidget.parent.requestContextMenu()
        }
    }
}
