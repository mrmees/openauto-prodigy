import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + 32
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    property StackView stackRef: StackView.view

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        spacing: 4

        SettingsPageHeader {
            title: "Audio"
            stack: root.stackRef
        }

        SettingsSlider {
            label: "Master Volume"
            configPath: "audio.master_volume"
            from: 0; to: 100; stepSize: 1
            onMoved: {
                if (typeof AudioService !== "undefined")
                    AudioService.setMasterVolume(value)
            }
        }

        // Output device selector
        Item {
            Layout.fillWidth: true
            implicitHeight: 48
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 12
                Text {
                    text: "Output Device"
                    font.pixelSize: 15
                    color: ThemeService.normalFontColor
                    Layout.fillWidth: true
                }
                ComboBox {
                    id: outputCombo
                    model: typeof AudioOutputDeviceModel !== "undefined" ? AudioOutputDeviceModel : []
                    textRole: "description"
                    Layout.preferredWidth: 220
                    onActivated: function(index) {
                        var nodeName = AudioOutputDeviceModel.data(
                            AudioOutputDeviceModel.index(index, 0), Qt.UserRole + 1)
                        ConfigService.setValue("audio.output_device", nodeName)
                        ConfigService.save()
                        if (typeof AudioService !== "undefined")
                            AudioService.setOutputDevice(nodeName)
                    }
                    Component.onCompleted: {
                        if (typeof AudioOutputDeviceModel === "undefined") return
                        var current = ConfigService.value("audio.output_device")
                        if (!current) current = "auto"
                        var idx = AudioOutputDeviceModel.indexOfDevice(current)
                        if (idx >= 0) currentIndex = idx
                    }
                }
            }
        }

        // Input device selector
        Item {
            Layout.fillWidth: true
            implicitHeight: 48
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 12
                Text {
                    text: "Input Device"
                    font.pixelSize: 15
                    color: ThemeService.normalFontColor
                    Layout.fillWidth: true
                }
                ComboBox {
                    id: inputCombo
                    model: typeof AudioInputDeviceModel !== "undefined" ? AudioInputDeviceModel : []
                    textRole: "description"
                    Layout.preferredWidth: 220
                    onActivated: function(index) {
                        var nodeName = AudioInputDeviceModel.data(
                            AudioInputDeviceModel.index(index, 0), Qt.UserRole + 1)
                        ConfigService.setValue("audio.input_device", nodeName)
                        ConfigService.save()
                        if (typeof AudioService !== "undefined")
                            AudioService.setInputDevice(nodeName)
                    }
                    Component.onCompleted: {
                        if (typeof AudioInputDeviceModel === "undefined") return
                        var current = ConfigService.value("audio.input_device")
                        if (!current) current = "auto"
                        var idx = AudioInputDeviceModel.indexOfDevice(current)
                        if (idx >= 0) currentIndex = idx
                    }
                }
            }
        }

        SettingsSlider {
            label: "Mic Gain"
            configPath: "audio.microphone.gain"
            from: 0.5; to: 4.0; stepSize: 0.1
        }

        // Restart button — device changes take effect on restart
        Item {
            Layout.fillWidth: true
            Layout.topMargin: 12
            implicitHeight: 44
            Button {
                anchors.centerIn: parent
                text: "Restart to Apply Device Changes"
                onClicked: ApplicationController.restart()
            }
        }
    }
}
