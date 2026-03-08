import QtQuick

Item {
    id: root

    property int bandIndex: 0
    property real gainValue: 0.0
    property string freqLabel: ""
    property bool bypassed: false

    signal gainChanged(real dB)
    signal resetRequested()

    // Internal state
    property bool isDragging: false
    property bool showLabel: false

    // Layout constants derived from UiMetrics
    readonly property int labelAreaH: UiMetrics.fontTiny + UiMetrics.spacing
    readonly property int trackTopY: UiMetrics.spacing
    readonly property int minTrackH: Math.round(60 * UiMetrics.scale)
    readonly property int trackHeight: Math.max(height - labelAreaH - UiMetrics.spacing * 2, minTrackH)
    readonly property int trackWidth: Math.max(UiMetrics.spacing / 2, 2)
    readonly property int thumbH: Math.round(UiMetrics.touchMin * 0.6)
    readonly property int thumbW: Math.round(UiMetrics.touchMin * 0.8)

    // Map gain value (-12..+12) to Y position within track
    function gainToY(dB) {
        return trackTopY + (1.0 - (dB + 12.0) / 24.0) * trackHeight
    }

    // Map Y position to gain value
    function yToGain(y) {
        var raw = 12.0 - (y - trackTopY) / trackHeight * 24.0
        var snapped = Math.round(raw / 0.5) * 0.5
        return Math.max(-12.0, Math.min(12.0, snapped))
    }

    // Track + thumb group with bypass dimming
    Item {
        id: trackGroup
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height - labelAreaH

        opacity: root.bypassed ? 0.4 : 1.0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        // Track line
        Rectangle {
            id: track
            anchors.horizontalCenter: parent.horizontalCenter
            y: root.trackTopY
            width: root.trackWidth
            height: root.trackHeight
            color: ThemeService.textSecondary
            opacity: 0.5
            radius: width / 2
        }

        // 0 dB reference line
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: UiMetrics.spacing / 2
            anchors.rightMargin: UiMetrics.spacing / 2
            y: root.gainToY(0) - 1
            height: 1
            color: ThemeService.outlineVariant
        }

        // Thumb
        Rectangle {
            id: thumb
            width: root.thumbW
            height: root.thumbH
            radius: UiMetrics.radius / 2
            color: ThemeService.primary
            x: (parent.width - width) / 2
            y: root.gainToY(root.gainValue) - height / 2

            Behavior on y {
                enabled: !root.isDragging
                NumberAnimation { duration: 100 }
            }
        }

        // Floating dB label above thumb
        Rectangle {
            id: floatingLabel
            width: floatingText.implicitWidth + UiMetrics.spacing * 2
            height: floatingText.implicitHeight + UiMetrics.spacing
            radius: UiMetrics.radius / 2
            color: ThemeService.surfaceVariant
            opacity: root.showLabel ? 0.9 : 0.0
            visible: opacity > 0
            x: (parent.width - width) / 2
            y: thumb.y - height - UiMetrics.spacing / 2

            Behavior on opacity { NumberAnimation { duration: 150 } }

            Text {
                id: floatingText
                anchors.centerIn: parent
                text: {
                    var v = root.gainValue
                    var sign = v > 0 ? "+" : ""
                    return sign + v.toFixed(1)
                }
                font.pixelSize: UiMetrics.fontSmall
                color: ThemeService.textPrimary
            }
        }

        // Touch area
        MouseArea {
            anchors.fill: parent

            onPressed: function(mouse) {
                root.isDragging = false
                root.showLabel = true
                var dB = root.yToGain(mouse.y)
                root.gainValue = dB
                root.gainChanged(dB)
            }

            onPositionChanged: function(mouse) {
                root.isDragging = true
                var dB = root.yToGain(mouse.y)
                root.gainValue = dB
                root.gainChanged(dB)
            }

            onReleased: {
                root.showLabel = false
            }

            onDoubleClicked: {
                if (!root.isDragging) {
                    root.gainValue = 0
                    root.resetRequested()
                }
            }
        }
    }

    // Frequency label below track
    Text {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.freqLabel
        font.pixelSize: UiMetrics.fontTiny
        color: ThemeService.textSecondary
    }
}
