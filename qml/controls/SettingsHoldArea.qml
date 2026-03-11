import QtQuick

MouseArea {
    id: root

    signal shortClicked()

    property bool holdTriggered: false
    property var _settingsMenu: null

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

    pressAndHoldInterval: 500
    preventStealing: true

    Component.onCompleted: _settingsMenu = _findSettingsMenu()

    onPressed: function(mouse) {
        holdTriggered = false
        if (_settingsMenu) {
            var pos = root.mapToItem(_settingsMenu, mouse.x, mouse.y)
            _settingsMenu.showHoldIndicator(pos)
        }
    }
    onReleased: {
        if (_settingsMenu)
            _settingsMenu.hideHoldIndicator()
    }
    onCanceled: {
        holdTriggered = false
        if (_settingsMenu)
            _settingsMenu.hideHoldIndicator()
    }
    onPressAndHold: {
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
        shortClicked()
    }
}
