import QtQuick

/// Individual navbar control: icon (volume/brightness) or clock (center).
/// Delegates press/release to NavbarController and shows hold progress feedback.
Item {
    id: root

    property int controlIndex: 0
    property string iconText: ""
    property bool showClock: false
    property bool isVertical: false

    // Page dot data — only when showing clock on home screen
    readonly property bool showDots: root.showClock
        && WidgetGridModel.pageCount > 1
        && !PluginModel.activePluginId
        && ApplicationController.currentApplication !== 6
    readonly property int leftDotCount: showDots ? WidgetGridModel.activePage : 0
    readonly property int rightDotCount: showDots ? Math.max(0, WidgetGridModel.pageCount - WidgetGridModel.activePage - 1) : 0
    readonly property real dotSize: UiMetrics.spacing * 1.2

    // Hold progress (0.0 - 1.0) driven by NavbarController
    property real _holdProgress: 0.0
    // Pressed state for quick-tap feedback
    property bool _pressed: touchArea.pressed

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

        // Pressed-state feedback (instant, full fill)
        Rectangle {
            anchors.fill: parent
            color: ThemeService.primaryContainer
            opacity: navbar.aaActive ? 0.0 : (root._pressed && root._holdProgress === 0 ? 0.3 : 0.0)
            Behavior on opacity { NumberAnimation { duration: 100 } }
        }

        // Progress overlay -- fills from bottom to top
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * root._holdProgress
            color: ThemeService.primaryContainer
            opacity: 0.3
            visible: root._holdProgress > 0
        }
    }

    // Icon display (volume or brightness)
    MaterialIcon {
        anchors.centerIn: parent
        icon: root.iconText
        size: UiMetrics.iconSize
        color: navbar.aaActive ? navbar.barFg : (root._pressed || root._holdProgress > 0 ? ThemeService.onPrimaryContainer : navbar.barFg)
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

    // --- Clock display -- horizontal mode (with page dots flanking) ---
    Row {
        anchors.centerIn: parent
        spacing: UiMetrics.spacing
        visible: root.showClock && !root.isVertical

        Repeater {
            model: root.leftDotCount
            Rectangle {
                width: root.dotSize; height: root.dotSize; radius: root.dotSize / 2
                color: ThemeService.onSurfaceVariant; opacity: 0.4
                anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            }
        }

        Text {
            id: clockHoriz
            color: navbar.aaActive ? navbar.barFg : (root._pressed || root._holdProgress > 0 ? ThemeService.onPrimaryContainer : navbar.barFg)
            font.pixelSize: Math.round(root.height * 0.75)
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        }

        Repeater {
            model: root.rightDotCount
            Rectangle {
                width: root.dotSize; height: root.dotSize; radius: root.dotSize / 2
                color: ThemeService.onSurfaceVariant; opacity: 0.4
                anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            }
        }
    }

    // --- Clock display -- vertical mode (with page dots above/below) ---
    Column {
        anchors.centerIn: parent
        spacing: UiMetrics.spacing
        visible: root.showClock && root.isVertical

        Repeater {
            model: root.leftDotCount  // "left" = above in vertical
            Rectangle {
                width: root.dotSize; height: root.dotSize; radius: root.dotSize / 2
                color: ThemeService.onSurfaceVariant; opacity: 0.4
                anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            }
        }

        Column {
            anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            Repeater {
                id: clockVertRepeater
                model: []
                Text {
                    text: modelData
                    font.pixelSize: Math.round(root.width * 0.55)
                    font.weight: Font.DemiBold
                    color: navbar.aaActive ? navbar.barFg : (root._pressed || root._holdProgress > 0 ? ThemeService.onPrimaryContainer : navbar.barFg)
                    horizontalAlignment: Text.AlignHCenter
                    width: root.width
                }
            }
        }

        Repeater {
            model: root.rightDotCount  // "right" = below in vertical
            Rectangle {
                width: root.dotSize; height: root.dotSize; radius: root.dotSize / 2
                color: ThemeService.onSurfaceVariant; opacity: 0.4
                anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            }
        }
    }

    // Touch handling (launcher mode -- during AA, evdev zones handle this)
    MouseArea {
        id: touchArea
        anchors.fill: parent
        onPressed: NavbarController.handlePress(root.controlIndex)
        onReleased: NavbarController.handleRelease(root.controlIndex)
        onExited: NavbarController.handleCancel(root.controlIndex)
    }
}
