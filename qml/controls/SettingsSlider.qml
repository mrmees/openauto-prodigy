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
    property var _settingsRow: null

    function _findSettingsRow() {
        var p = root.parent
        while (p) {
            if (typeof p.cancelBackHold === "function"
                    && typeof p.consumeBackHoldTrigger === "function")
                return p
            p = p.parent
        }
        return null
    }

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    Component.onCompleted: {
        _settingsRow = _findSettingsRow()
        if (root.configPath !== "") {
            var v = ConfigService.value(root.configPath)
            if (v !== undefined && v !== null)
                slider.value = v
        }
    }

    Timer {
        id: debounce
        interval: 300
        onTriggered: {
            if (_settingsRow && _settingsRow.consumeBackHoldTrigger()) {
                slider.value = root._pressValue
                return
            }
            if (root.configPath === "") return
            ConfigService.setValue(root.configPath, slider.value)
            ConfigService.save()
        }
    }

    Component.onDestruction: {
        if (debounce.running) {
            debounce.stop()
            if (!(_settingsRow && _settingsRow.consumeBackHoldTrigger()) && root.configPath !== "") {
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
                    return
                }

                if (_settingsRow && _settingsRow.consumeBackHoldTrigger())
                    value = root._pressValue
            }
            onMoved: {
                if (_settingsRow)
                    _settingsRow.cancelBackHold()
                if (_settingsRow && _settingsRow.consumeBackHoldTrigger()) {
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
