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

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    Component.onCompleted: {
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
            if (root.configPath === "") return
            ConfigService.setValue(root.configPath, slider.value)
            ConfigService.save()
        }
    }

    Component.onDestruction: {
        if (debounce.running) {
            debounce.stop()
            if (root.configPath !== "") {
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
            onMoved: {
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
