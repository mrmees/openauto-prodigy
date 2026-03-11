import QtQuick

// Background long-press detector for settings pages.
// Place as a child of a Flickable/Item. Long-press (500ms) on non-control
// areas triggers back navigation with expanding ripple feedback.
Item {
    id: root
    anchors.fill: parent

    // Find the top-level window overlay for ripple rendering
    property var _overlay: null
    Component.onCompleted: {
        var p = root
        while (p.parent) p = p.parent
        _overlay = p  // Application window contentItem
    }

    // MouseArea behind content so controls handle taps normally
    MouseArea {
        id: holdArea
        anchors.fill: parent
        z: -1
        pressAndHoldInterval: 500
        preventStealing: false

        onPressed: function(mouse) {
            if (!root._overlay) return
            var global = root.mapToItem(root._overlay, mouse.x, mouse.y)
            ripple.parent = root._overlay
            ripple.x = global.x
            ripple.y = global.y
            ripple.width = 0
            ripple.visible = true
            rippleGrow.start()
        }
        onReleased: { ripple.visible = false; rippleGrow.stop() }
        onCanceled: { ripple.visible = false; rippleGrow.stop() }
        onPressAndHold: {
            ripple.visible = false
            rippleGrow.stop()
            ApplicationController.requestBack()
        }
    }

    // Expanding ripple indicator — reparented to window overlay on press
    Rectangle {
        id: ripple
        visible: false
        width: 0; height: width
        radius: width / 2
        color: "transparent"
        border.width: Math.max(2, UiMetrics.spacing * 0.5)
        border.color: ThemeService.primary
        opacity: 0.5
        z: 10000

        // Re-center as it grows
        transform: Translate {
            x: -ripple.width / 2
            y: -ripple.height / 2
        }

        MaterialIcon {
            anchors.centerIn: parent
            icon: "\ue5c4"  // arrow_back
            size: UiMetrics.fontTitle
            color: ThemeService.primary
            opacity: Math.min(1.0, ripple.width / (UiMetrics.touchMin * 1.2))
        }

        NumberAnimation on width {
            id: rippleGrow
            running: false
            from: 0; to: UiMetrics.touchMin * 1.5
            duration: 500
            easing.type: Easing.OutCubic
        }
    }
}
