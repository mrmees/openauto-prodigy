import QtQuick
import QtQuick.Layouts

Item {
    id: aaStatusWidget

    // Widget contract: context injection from host
    property QtObject widgetContext: null

    // Span-based breakpoint for responsive layout
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1
    readonly property bool showText: colSpan >= 2

    // Provider access via widgetContext
    property bool connected: widgetContext && widgetContext.projectionStatus
                             ? (widgetContext.projectionStatus.projectionState === 3
                                || widgetContext.projectionStatus.projectionState === 4)
                             : false

    ColumnLayout {
        anchors.centerIn: parent
        spacing: UiMetrics.spacing

        MaterialIcon {
            icon: connected ? "\ue531" : "\ue55d"  // phonelink / phonelink_off
            size: showText ? UiMetrics.iconSize * 1.5 : UiMetrics.iconSize * 2
            color: connected ? ThemeService.primary : ThemeService.onSurfaceVariant
            Layout.alignment: Qt.AlignHCenter
        }

        NormalText {
            text: connected ? "Connected" : "Tap to connect"
            visible: showText
            font.pixelSize: UiMetrics.fontTitle
            color: ThemeService.onSurface
            Layout.alignment: Qt.AlignHCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        pressAndHoldInterval: 500
        onClicked: {
            if (!connected)
                ActionRegistry.dispatch("app.launchPlugin", "org.openauto.android-auto")
        }
        onPressAndHold: {
            if (aaStatusWidget.parent && aaStatusWidget.parent.requestContextMenu)
                aaStatusWidget.parent.requestContextMenu()
        }
    }
}
