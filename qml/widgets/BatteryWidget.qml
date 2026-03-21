import QtQuick
import QtQuick.Layouts

Item {
    id: batteryWidget
    clip: true

    property QtObject widgetContext: null
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1

    // Null-safe companion data access
    readonly property bool companionConnected: CompanionService ? CompanionService.connected : false
    readonly property int batteryLevel: CompanionService ? CompanionService.phoneBattery : -1

    // Orientation: vertical if square or taller, horizontal if wider
    readonly property bool isVertical: height >= width

    Canvas {
        id: batteryCanvas
        anchors.fill: parent
        anchors.margins: UiMetrics.spacing

        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            ctx.clearRect(0, 0, w, h)

            var level = batteryWidget.companionConnected ? Math.max(0, Math.min(100, batteryWidget.batteryLevel)) : 0
            var connected = batteryWidget.companionConnected

            var radius = UiMetrics.radius
            var outlineWidth = 3
            var tipSize = 6  // battery tip/nub
            var inset = outlineWidth + 3  // gap between outline and fill

            if (batteryWidget.isVertical) {
                // --- Vertical battery (bottom to top) ---
                var bodyX = outlineWidth / 2
                var bodyY = tipSize + outlineWidth / 2
                var bodyW = w - outlineWidth
                var bodyH = h - tipSize - outlineWidth

                // Tip (positive terminal) at top center
                var tipW = bodyW * 0.35
                var tipX = (w - tipW) / 2
                ctx.beginPath()
                ctx.roundedRect(tipX, 0, tipW, tipSize + radius, radius / 2, radius / 2)
                ctx.fillStyle = String(ThemeService.onSurfaceVariant)
                ctx.fill()

                // Body outline
                ctx.beginPath()
                ctx.roundedRect(bodyX, bodyY, bodyW, bodyH, radius, radius)
                ctx.strokeStyle = connected ? String(ThemeService.onSurface) : String(ThemeService.onSurfaceVariant)
                ctx.lineWidth = outlineWidth
                ctx.stroke()

                // Fill level (bottom to top)
                if (connected && level > 0) {
                    var fillH = Math.max(0, (bodyH - inset * 2) * level / 100)
                    var fillY = bodyY + inset + (bodyH - inset * 2 - fillH)
                    var fillR = Math.max(0, radius - inset)
                    ctx.beginPath()
                    ctx.roundedRect(bodyX + inset, fillY, bodyW - inset * 2, fillH, fillR, fillR)
                    ctx.fillStyle = level <= 20 ? String(ThemeService.error) : String(ThemeService.primary)
                    ctx.fill()
                }
            } else {
                // --- Horizontal battery (left to right) ---
                var bodyX2 = outlineWidth / 2
                var bodyY2 = outlineWidth / 2
                var bodyW2 = w - tipSize - outlineWidth
                var bodyH2 = h - outlineWidth

                // Tip (positive terminal) at right center
                var tipH = bodyH2 * 0.35
                var tipY = (h - tipH) / 2
                ctx.beginPath()
                ctx.roundedRect(w - tipSize - radius, tipY, tipSize + radius, tipH, radius / 2, radius / 2)
                ctx.fillStyle = String(ThemeService.onSurfaceVariant)
                ctx.fill()

                // Body outline
                ctx.beginPath()
                ctx.roundedRect(bodyX2, bodyY2, bodyW2, bodyH2, radius, radius)
                ctx.strokeStyle = connected ? String(ThemeService.onSurface) : String(ThemeService.onSurfaceVariant)
                ctx.lineWidth = outlineWidth
                ctx.stroke()

                // Fill level (left to right)
                if (connected && level > 0) {
                    var fillW = Math.max(0, (bodyW2 - inset * 2) * level / 100)
                    var fillR2 = Math.max(0, radius - inset)
                    ctx.beginPath()
                    ctx.roundedRect(bodyX2 + inset, bodyY2 + inset, fillW, bodyH2 - inset * 2, fillR2, fillR2)
                    ctx.fillStyle = level <= 20 ? String(ThemeService.error) : String(ThemeService.primary)
                    ctx.fill()
                }
            }
        }

        // Repaint on any state change
        Connections {
            target: batteryWidget
            function onBatteryLevelChanged() { batteryCanvas.requestPaint() }
            function onCompanionConnectedChanged() { batteryCanvas.requestPaint() }
            function onIsVerticalChanged() { batteryCanvas.requestPaint() }
        }
        Connections {
            target: ThemeService
            function onCurrentThemeIdChanged() { batteryCanvas.requestPaint() }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    // Percentage text centered over the canvas
    NormalText {
        anchors.centerIn: parent
        text: batteryWidget.companionConnected
              ? batteryWidget.batteryLevel + "%"
              : "--"
        font.pixelSize: Math.min(parent.width, parent.height) * 0.3
        font.weight: Font.Bold
        fontSizeMode: Text.Fit
        minimumPixelSize: UiMetrics.fontSmall
        color: ThemeService.onSurface
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    // pressAndHold forwarding for host context menu
    MouseArea {
        anchors.fill: parent
        z: -1
        pressAndHoldInterval: 500
        onPressAndHold: {
            if (batteryWidget.parent && batteryWidget.parent.requestContextMenu)
                batteryWidget.parent.requestContextMenu()
        }
    }
}
