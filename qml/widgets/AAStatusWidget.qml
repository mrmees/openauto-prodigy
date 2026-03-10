import QtQuick
import QtQuick.Layouts

Item {
    id: aaStatusWidget

    property bool isMainPane: typeof widgetContext !== "undefined"
                              ? widgetContext.paneSize === 1 : false
    property bool connected: typeof AAOrchestrator !== "undefined"
                             && AAOrchestrator.aaConnected

    Rectangle {
        anchors.fill: parent
        radius: UiMetrics.radius
        color: ThemeService.surfaceContainer

        ColumnLayout {
            anchors.centerIn: parent
            spacing: UiMetrics.spacing

            MaterialIcon {
                icon: connected ? "\ue531" : "\ue55d"  // phonelink / phonelink_off
                size: isMainPane ? UiMetrics.iconSize * 2 : UiMetrics.iconSize
                color: connected ? ThemeService.primary : ThemeService.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
            }

            NormalText {
                text: connected ? "Connected" : "Tap to connect"
                font.pixelSize: isMainPane ? UiMetrics.fontTitle : UiMetrics.fontBody
                color: ThemeService.onSurface
                Layout.alignment: Qt.AlignHCenter
            }

            NormalText {
                text: "Android Auto"
                font.pixelSize: UiMetrics.fontSmall
                color: ThemeService.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
                visible: isMainPane
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (!connected)
                    PluginModel.setActivePlugin("org.openauto.android-auto")
            }
        }
    }
}
