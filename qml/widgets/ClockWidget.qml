import QtQuick
import QtQuick.Layouts

Item {
    id: clockWidget
    clip: true

    // Widget contract: context injection from host
    property QtObject widgetContext: null

    // Span-based breakpoints for responsive layout
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1

    // Load-state gating (timer is the one justified polling case)
    readonly property bool isCurrentPage: widgetContext ? widgetContext.isCurrentPage : true

    // Declarative binding to effectiveConfig — ensures QML tracks property changes.
    readonly property var currentEffectiveConfig: widgetContext ? widgetContext.effectiveConfig : ({})
    readonly property string timeFormat: currentEffectiveConfig.format || "24h"
    readonly property string clockStyle: currentEffectiveConfig.style || "digital"

    // Time property updated by the root-level Timer
    property string currentTime: ""

    Timer {
        interval: 1000
        running: clockWidget.isCurrentPage
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            var now = new Date()
            var timeFmt = (clockWidget.timeFormat === "12h") ? "h:mm AP" : "HH:mm"
            clockWidget.currentTime = now.toLocaleTimeString(Qt.locale(), timeFmt)
        }
    }

    Loader {
        anchors.fill: parent
        sourceComponent: {
            switch (clockWidget.clockStyle) {
                case "analog": return analogComponent
                case "minimal": return minimalComponent
                default: return digitalComponent
            }
        }
    }

    // --- Digital style: time-only display ---
    Component {
        id: digitalComponent
        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - UiMetrics.spacing * 2
            spacing: 0

            NormalText {
                text: clockWidget.currentTime
                font.pixelSize: UiMetrics.fontHeading * 2.5
                font.weight: Font.Light
                fontSizeMode: Text.Fit
                minimumPixelSize: UiMetrics.fontHeading
                color: ThemeService.onSurface
                Layout.fillWidth: true
                Layout.fillHeight: true
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // --- Analog style: canvas clock face ---
    Component {
        id: analogComponent
        Item {
            anchors.fill: parent

            readonly property bool fullSize: (widgetContext ? widgetContext.colSpan : 1) >= 2
                                          && (widgetContext ? widgetContext.rowSpan : 1) >= 2

            // Small-size guard: show resize hint at sub-2x2
            MaterialIcon {
                anchors.centerIn: parent
                visible: !parent.fullSize
                icon: "\ue5c9"  // open_in_full
                font.pixelSize: UiMetrics.fontHeading * 1.5
                color: ThemeService.onSurfaceVariant
            }

            // Full analog face: only when 2x2+
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: UiMetrics.spacing
                visible: parent.fullSize
                spacing: 0

                Canvas {
                    id: analogCanvas
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    onPaint: {
                        var ctx = getContext("2d")
                        var w = width
                        var h = height
                        ctx.clearRect(0, 0, w, h)

                        var size = Math.min(w, h)
                        var cx = w / 2
                        var cy = h / 2
                        var radius = size / 2 - 4

                        // Face background
                        ctx.beginPath()
                        ctx.arc(cx, cy, radius, 0, 2 * Math.PI)
                        ctx.fillStyle = ThemeService.surfaceContainer
                        ctx.fill()

                        // Hour tick marks (12 ticks)
                        for (var i = 0; i < 12; i++) {
                            var angle = (i * 30 - 90) * Math.PI / 180
                            var innerR = radius * 0.85
                            var outerR = radius * 0.95
                            ctx.beginPath()
                            ctx.moveTo(cx + innerR * Math.cos(angle), cy + innerR * Math.sin(angle))
                            ctx.lineTo(cx + outerR * Math.cos(angle), cy + outerR * Math.sin(angle))
                            ctx.strokeStyle = ThemeService.onSurfaceVariant
                            ctx.lineWidth = 2
                            ctx.stroke()
                        }

                        var now = new Date()
                        var hours = now.getHours() % 12
                        var minutes = now.getMinutes()
                        var seconds = now.getSeconds()

                        // Hour hand
                        drawHand(ctx, cx, cy,
                            (hours + minutes / 60) * 30 - 90,
                            radius * 0.50, 3, ThemeService.onSurface)

                        // Minute hand
                        drawHand(ctx, cx, cy,
                            (minutes + seconds / 60) * 6 - 90,
                            radius * 0.70, 2, ThemeService.onSurface)

                        // Second hand
                        drawHand(ctx, cx, cy,
                            seconds * 6 - 90,
                            radius * 0.80, 1, ThemeService.onSurfaceVariant)

                        // Center dot
                        ctx.beginPath()
                        ctx.arc(cx, cy, 3, 0, 2 * Math.PI)
                        ctx.fillStyle = ThemeService.onSurface
                        ctx.fill()
                    }

                    function drawHand(ctx, cx, cy, angleDeg, length, lineWidth, color) {
                        var angle = angleDeg * Math.PI / 180
                        ctx.beginPath()
                        ctx.moveTo(cx, cy)
                        ctx.lineTo(cx + length * Math.cos(angle), cy + length * Math.sin(angle))
                        ctx.strokeStyle = color
                        ctx.lineWidth = lineWidth
                        ctx.lineCap = "round"
                        ctx.stroke()
                    }

                    Connections {
                        target: clockWidget
                        function onCurrentTimeChanged() {
                            analogCanvas.requestPaint()
                        }
                    }
                }
            }
        }
    }

    // --- Minimal style: time only, large thin font ---
    Component {
        id: minimalComponent
        NormalText {
            anchors.fill: parent
            text: clockWidget.currentTime
            font.pixelSize: UiMetrics.fontHeading * 3.0
            font.weight: Font.Thin
            fontSizeMode: Text.Fit
            minimumPixelSize: UiMetrics.fontHeading
            color: ThemeService.onSurface
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // pressAndHold forwarding for host context menu
    MouseArea {
        anchors.fill: parent
        z: -1
        pressAndHoldInterval: 500
        onPressAndHold: {
            if (clockWidget.parent && clockWidget.parent.requestContextMenu)
                clockWidget.parent.requestContextMenu()
        }
    }
}
