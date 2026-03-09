import QtQuick

/// Individual navbar control: icon (volume/brightness) or clock (center).
/// Delegates press/release to NavbarController and shows hold progress feedback.
Item {
    id: root

    property int controlIndex: 0
    property string iconText: ""
    property bool showClock: false
    property bool isVertical: false

    // Hold progress (0.0 - 1.0) driven by NavbarController
    property real _holdProgress: 0.0

    Connections {
        target: NavbarController
        function onHoldProgress(idx, progress) {
            if (idx === root.controlIndex)
                root._holdProgress = progress
        }
        function onGestureTriggered(idx, gesture) {
            if (idx === root.controlIndex)
                root._holdProgress = 0.0
        }
    }

    // Background with hold progress feedback
    Rectangle {
        anchors.fill: parent
        color: navbar.barBg
        opacity: 1.0

        // Progress overlay — fills from bottom to top
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * root._holdProgress
            color: ThemeService.tertiary
            opacity: 0.3
            visible: root._holdProgress > 0
        }
    }

    // Icon display (volume or brightness)
    MaterialIcon {
        anchors.centerIn: parent
        icon: root.iconText
        size: UiMetrics.iconSize
        color: navbar.barFg
        visible: !root.showClock
    }

    // Clock display -- horizontal mode
    Text {
        id: clockHoriz
        anchors.centerIn: parent
        visible: root.showClock && !root.isVertical
        color: navbar.barFg
        font.pixelSize: Math.round(root.height * 0.75)
        font.weight: Font.DemiBold

        Timer {
            interval: 1000
            running: root.showClock
            repeat: true
            triggeredOnStart: true
            onTriggered: {
                var v = ConfigService.value("display.clock_24h")
                var is24h = (v === true || v === 1 || v === "true")
                var timeStr
                if (is24h) {
                    timeStr = Qt.formatTime(new Date(), "HH:mm")
                } else {
                    // Qt "h:mm" still outputs 24h unless "ap" is present.
                    // Build 12h manually to avoid AM/PM suffix.
                    var now = new Date()
                    var h = now.getHours() % 12 || 12
                    var m = now.getMinutes()
                    timeStr = h + ":" + (m < 10 ? "0" : "") + m
                }
                clockHoriz.text = timeStr
                // Update vertical clock model too
                var chars = []
                for (var i = 0; i < timeStr.length; i++) {
                    chars.push(timeStr[i])
                }
                clockVertRepeater.model = chars
            }
        }
    }

    // Clock display -- vertical mode (stacked single-digit column)
    Column {
        anchors.centerIn: parent
        visible: root.showClock && root.isVertical
        spacing: 0

        Repeater {
            id: clockVertRepeater
            model: []
            Text {
                text: modelData
                font.pixelSize: Math.round(root.width * 0.55)
                font.weight: Font.DemiBold
                color: navbar.barFg
                horizontalAlignment: Text.AlignHCenter
                width: root.width
            }
        }
    }

    // Touch handling (launcher mode -- during AA, evdev zones handle this)
    MouseArea {
        anchors.fill: parent
        onPressed: NavbarController.handlePress(root.controlIndex)
        onReleased: NavbarController.handleRelease(root.controlIndex)
        onExited: NavbarController.handleCancel(root.controlIndex)
    }
}
