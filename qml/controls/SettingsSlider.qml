import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property string label: ""
    property string configPath: ""
    property real from: 0
    property real to: 100
    property real stepSize: 1
    property bool restartRequired: false
    property alias value: slider.value
    signal moved()

    Layout.fillWidth: true
    implicitHeight: 48

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
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 12

        Text {
            text: root.label
            font.pixelSize: 15
            color: ThemeService.normalFontColor
            Layout.preferredWidth: root.width * 0.3
        }

        MaterialIcon {
            icon: "\ue86a"
            size: 16
            color: ThemeService.descriptionFontColor
            visible: root.restartRequired
        }

        Slider {
            id: slider
            Layout.fillWidth: true
            from: root.from
            to: root.to
            stepSize: root.stepSize
            onMoved: { debounce.restart(); root.moved() }
        }

        Text {
            text: Math.round(slider.value * 100) / 100
            font.pixelSize: 14
            color: ThemeService.descriptionFontColor
            Layout.preferredWidth: 40
            horizontalAlignment: Text.AlignRight
        }
    }

    Component.onCompleted: {
        if (root.configPath !== "") {
            var v = ConfigService.value(root.configPath)
            if (v !== undefined && v !== null)
                slider.value = v
        }
    }
}
