import QtQuick
import QtQuick.Layouts

Item {
    id: clockWidget

    // WidgetInstanceContext is injected into our QQmlContext
    // Access paneSize to adapt layout
    property bool isMainPane: typeof widgetContext !== "undefined"
                              ? widgetContext.paneSize === 1 : true  // 1 = Main

    Rectangle {
        anchors.fill: parent
        radius: UiMetrics.radius
        color: "transparent"  // WidgetHost provides glass background

        ColumnLayout {
            anchors.centerIn: parent
            spacing: isMainPane ? UiMetrics.spacing : UiMetrics.spacing * 0.5

            NormalText {
                id: timeText
                font.pixelSize: isMainPane ? UiMetrics.fontHeading * 2.5 : UiMetrics.fontHeading * 1.5
                font.weight: Font.Light
                color: ThemeService.onSurface
                Layout.alignment: Qt.AlignHCenter
            }

            NormalText {
                id: dateText
                font.pixelSize: isMainPane ? UiMetrics.fontTitle : UiMetrics.fontBody
                color: ThemeService.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
            }

            NormalText {
                id: dayText
                font.pixelSize: isMainPane ? UiMetrics.fontBody : UiMetrics.fontSmall
                color: ThemeService.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
                visible: isMainPane
            }
        }
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            var now = new Date()
            timeText.text = now.toLocaleTimeString(Qt.locale(), "h:mm")
            dateText.text = now.toLocaleDateString(Qt.locale(), "MMMM d, yyyy")
            dayText.text = now.toLocaleDateString(Qt.locale(), "dddd")
        }
    }
}
