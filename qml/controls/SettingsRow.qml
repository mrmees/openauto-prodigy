import QtQuick
import QtQuick.Layouts

Item {
    id: root
    readonly property bool blocksBackHold: true

    property int rowIndex: 0
    property bool interactive: false
    property bool _backHoldArmed: false
    property bool _backHoldTriggered: false
    property var _settingsMenu: null
    signal clicked()
    default property alias content: contentContainer.data

    function _findSettingsMenu() {
        var p = root.parent
        while (p) {
            if (typeof p.showHoldIndicator === "function"
                    && typeof p.hideHoldIndicator === "function")
                return p
            p = p.parent
        }
        return null
    }

    function _armBackHold(pos) {
        _backHoldTriggered = false
        _backHoldArmed = true
        if (_settingsMenu) {
            var mapped = root.mapToItem(_settingsMenu, pos.x, pos.y)
            _settingsMenu.showHoldIndicator(mapped)
        }
    }

    function cancelBackHold() {
        _backHoldArmed = false
        if (_settingsMenu)
            _settingsMenu.hideHoldIndicator()
    }

    function consumeBackHoldTrigger() {
        if (!_backHoldTriggered)
            return false
        _backHoldTriggered = false
        return true
    }

    function _triggerBackHold() {
        if (!_backHoldArmed)
            return
        _backHoldArmed = false
        _backHoldTriggered = true
        if (_settingsMenu)
            _settingsMenu.hideHoldIndicator()
        ApplicationController.requestBack()
    }

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    Component.onCompleted: _settingsMenu = _findSettingsMenu()

    // Alternating background: even = surfaceContainer, odd = surfaceContainerHigh
    Rectangle {
        id: bg
        anchors.fill: parent
        color: rowIndex % 2 === 0 ? ThemeService.surfaceContainer : ThemeService.surfaceContainerHigh
    }

    // Press feedback state layer (interactive rows only)
    Rectangle {
        anchors.fill: parent
        color: ThemeService.onSurface
        opacity: interactive && mouseArea.pressed ? 0.08 : 0.0
        Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
    }

    // Content container — no margins (controls provide their own)
    Item {
        id: contentContainer
        anchors.fill: parent
        // Auto-size the single content child to fill this container
        onChildrenChanged: {
            for (var i = 0; i < children.length; i++) {
                var child = children[i];
                child.anchors.left = contentContainer.left;
                child.anchors.right = contentContainer.right;
                child.anchors.top = contentContainer.top;
                child.anchors.bottom = contentContainer.bottom;
            }
        }
    }

    Item {
        id: backHoldOverlay
        anchors.fill: parent
        z: 5

        TapHandler {
            id: backHoldTouch
            target: null
            gesturePolicy: TapHandler.DragThreshold
            acceptedDevices: PointerDevice.TouchScreen
            acceptedPointerTypes: PointerDevice.Finger
            acceptedButtons: Qt.NoButton
            longPressThreshold: 0.5
            dragThreshold: Math.round(UiMetrics.touchMin * 0.5)

            onPressedChanged: {
                if (pressed)
                    root._armBackHold(point.position)
                else
                    root.cancelBackHold()
            }
            onCanceled: root.cancelBackHold()
            onLongPressed: root._triggerBackHold()
        }

        TapHandler {
            id: backHoldMouse
            target: null
            gesturePolicy: TapHandler.DragThreshold
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            acceptedButtons: Qt.LeftButton
            longPressThreshold: 0.5
            dragThreshold: 12

            onPressedChanged: {
                if (pressed)
                    root._armBackHold(point.position)
                else
                    root.cancelBackHold()
            }
            onCanceled: root.cancelBackHold()
            onLongPressed: root._triggerBackHold()
        }
    }

    // MouseArea for interactive rows
    SettingsHoldArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.interactive
        visible: root.interactive
        enableBackHold: false
        onShortClicked: root.clicked()
    }
}
