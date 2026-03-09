import QtQuick

/// Individual navbar control: icon (volume/brightness) or clock (center).
/// Delegates press/release to NavbarController and shows hold progress feedback.
Item {
    id: root

    property int controlIndex: 0
    property string iconText: ""
    property bool showClock: false
    property bool isVertical: false

    // Phone status properties (set from Navbar.qml on center control only)
    property int phoneBattery: -1
    property int phoneSignal: -1
    property bool phoneConnected: false

    // Hold progress (0.0 - 1.0) driven by NavbarController
    property real _holdProgress: 0.0

    // --- Icon mapping helpers ---
    function batteryIcon(level) {
        if (level < 0) return ""
        if (level === 0) return "\ue19c"       // battery_alert
        if (level < 20) return "\uebd0"        // battery_1_bar
        if (level < 40) return "\uebd2"        // battery_3_bar
        if (level < 60) return "\uebd3"        // battery_4_bar
        if (level < 80) return "\uebd4"        // battery_5_bar
        if (level < 96) return "\uebd5"        // battery_6_bar
        return "\ue1a4"                         // battery_full
    }

    function signalIcon(strength) {
        if (strength < 0) return ""
        var icons = ["\uf0a8", "\uf0a9", "\uf0aa", "\uf0ab", "\uf0ac"]
        return icons[Math.min(strength, 4)]
    }

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

        // Progress overlay -- fills from bottom to top
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

    // --- Clock timer (shared between horizontal and vertical) ---
    Timer {
        id: clockTimer
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

    // --- Clock display -- horizontal mode with flanking indicators ---
    Row {
        anchors.centerIn: parent
        spacing: UiMetrics.spacing
        visible: root.showClock && !root.isVertical

        // Signal indicator (left of clock)
        MaterialIcon {
            icon: root.signalIcon(root.phoneSignal)
            size: UiMetrics.iconSize
            color: navbar.barFg
            visible: root.phoneConnected && root.phoneSignal >= 0
            anchors.verticalCenter: parent.verticalCenter
        }

        // Clock text
        Text {
            id: clockHoriz
            color: navbar.barFg
            font.pixelSize: Math.round(root.height * 0.75)
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
        }

        // Battery indicator (right of clock)
        MaterialIcon {
            icon: root.batteryIcon(root.phoneBattery)
            size: UiMetrics.iconSize
            color: root.phoneBattery >= 0 && root.phoneBattery < 20
                   ? (navbar.aaActive ? "#FF4444" : ThemeService.error)
                   : navbar.barFg
            visible: root.phoneConnected && root.phoneBattery >= 0
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // --- Clock display -- vertical mode with stacked indicators ---
    Column {
        anchors.centerIn: parent
        visible: root.showClock && root.isVertical
        spacing: Math.round(UiMetrics.spacing * 0.5)

        // Signal indicator (above clock digits)
        MaterialIcon {
            icon: root.signalIcon(root.phoneSignal)
            size: UiMetrics.iconSize
            color: navbar.barFg
            visible: root.phoneConnected && root.phoneSignal >= 0
            anchors.horizontalCenter: parent.horizontalCenter
        }

        // Stacked clock digits
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

        // Battery indicator (below clock digits)
        MaterialIcon {
            icon: root.batteryIcon(root.phoneBattery)
            size: UiMetrics.iconSize
            color: root.phoneBattery >= 0 && root.phoneBattery < 20
                   ? (navbar.aaActive ? "#FF4444" : ThemeService.error)
                   : navbar.barFg
            visible: root.phoneConnected && root.phoneBattery >= 0
            anchors.horizontalCenter: parent.horizontalCenter
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
