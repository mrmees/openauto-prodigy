import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    readonly property bool blocksBackHold: true
    property string icon: ""
    property string label: ""
    property string configPath: ""
    property real from: 0
    property real to: 100
    property real stepSize: 1
    property bool restartRequired: false
    property alias value: slider.value
    signal moved()
    property real _pressValue: 0
    property bool _holdTriggered: false
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

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    Component.onCompleted: {
        _settingsMenu = _findSettingsMenu()
        if (root.configPath !== "") {
            var v = ConfigService.value(root.configPath)
            if (v !== undefined && v !== null)
                slider.value = v
        }
    }

    Timer {
        id: holdTimer
        interval: 500
        onTriggered: {
            if (!slider.pressed)
                return
            root._holdTriggered = true
            slider.value = root._pressValue
            if (_settingsMenu) {
                var pos = slider.mapToItem(_settingsMenu, slider.width / 2, slider.height / 2)
                _settingsMenu.hideHoldIndicator()
            }
            ApplicationController.requestBack()
        }
    }

    Timer {
        id: debounce
        interval: 300
        onTriggered: {
            if (root._holdTriggered) return
            if (root.configPath === "") return
            ConfigService.setValue(root.configPath, slider.value)
            ConfigService.save()
        }
    }

    Component.onDestruction: {
        if (debounce.running) {
            debounce.stop()
            if (!root._holdTriggered && root.configPath !== "") {
                ConfigService.setValue(root.configPath, slider.value)
                ConfigService.save()
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginRow
        anchors.rightMargin: UiMetrics.marginRow
        spacing: UiMetrics.gap

        MaterialIcon {
            icon: root.icon
            size: UiMetrics.iconSize
            color: ThemeService.onSurface
            visible: root.icon !== ""
            Layout.preferredWidth: UiMetrics.iconSize
        }

        Text {
            text: root.label
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.onSurface
            Layout.preferredWidth: root.width * 0.3
        }

        MaterialIcon {
            icon: "\ue86a"
            size: UiMetrics.iconSmall
            color: ThemeService.onSurfaceVariant
            visible: root.restartRequired
        }

        Slider {
            id: slider
            Layout.fillWidth: true
            from: root.from
            to: root.to
            stepSize: root.stepSize
            onPressedChanged: {
                if (pressed) {
                    root._pressValue = value
                    root._holdTriggered = false
                    if (_settingsMenu) {
                        var pos = slider.mapToItem(_settingsMenu, slider.width / 2, slider.height / 2)
                        _settingsMenu.showHoldIndicator(pos)
                    }
                    holdTimer.restart()
                    return
                }

                holdTimer.stop()
                if (_settingsMenu)
                    _settingsMenu.hideHoldIndicator()
                if (root._holdTriggered)
                    value = root._pressValue
            }
            onMoved: {
                holdTimer.stop()
                if (_settingsMenu)
                    _settingsMenu.hideHoldIndicator()
                if (root._holdTriggered) {
                    value = root._pressValue
                    return
                }
                debounce.restart()
                root.moved()
            }
        }

        Text {
            text: Math.round(slider.value * 100) / 100
            font.pixelSize: UiMetrics.fontSmall
            color: ThemeService.onSurfaceVariant
            Layout.preferredWidth: UiMetrics.rowH
            horizontalAlignment: Text.AlignRight
        }
    }

}
