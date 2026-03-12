import QtQuick
import QtQuick.Layouts

Item {
    id: aaStatusWidget

    // Pixel-based breakpoint for responsive layout
    readonly property bool showText: width >= 250   // true at 2+ cells wide

    property bool connected: typeof AAOrchestrator !== "undefined"
                             && AAOrchestrator.aaConnected

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
                PluginModel.setActivePlugin("org.openauto.android-auto")
        }
        onPressAndHold: {
            if (aaStatusWidget.parent && aaStatusWidget.parent.requestContextMenu)
                aaStatusWidget.parent.requestContextMenu()
        }
    }
}
