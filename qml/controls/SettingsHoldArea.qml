import QtQuick

MouseArea {
    id: root

    signal shortClicked()

    property bool enableBackHold: true
    property bool holdTriggered: false
    property var _settingsMenu: null
    property var _settingsRow: null

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

    function _findSettingsRow() {
        var p = root.parent
        while (p) {
            if (typeof p.consumeBackHoldTrigger === "function"
                    && typeof p.cancelBackHold === "function")
                return p
            p = p.parent
        }
        return null
    }

    pressAndHoldInterval: 500
    preventStealing: true

    Component.onCompleted: {
        _settingsMenu = _findSettingsMenu()
        _settingsRow = _findSettingsRow()
    }

    onPressed: function(mouse) {
        holdTriggered = false
        if (enableBackHold && _settingsMenu) {
            var pos = root.mapToItem(_settingsMenu, mouse.x, mouse.y)
            _settingsMenu.showHoldIndicator(pos)
        }
    }
    onReleased: {
        if (enableBackHold && _settingsMenu)
            _settingsMenu.hideHoldIndicator()
    }
    onCanceled: {
        holdTriggered = false
        if (enableBackHold && _settingsMenu)
            _settingsMenu.hideHoldIndicator()
    }
    onPressAndHold: {
        if (!enableBackHold)
            return
        holdTriggered = true
        if (_settingsMenu)
            _settingsMenu.hideHoldIndicator()
        ApplicationController.requestBack()
    }
    onClicked: {
        if (holdTriggered) {
            holdTriggered = false
            return
        }
        if (_settingsRow && _settingsRow.consumeBackHoldTrigger())
            return
        shortClicked()
    }
}
