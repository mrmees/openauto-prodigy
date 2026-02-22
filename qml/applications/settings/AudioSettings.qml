import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        SectionHeader { text: "Volume" }

        SettingsSlider {
            label: "Master Volume"
            configPath: "audio.master_volume"
            from: 0; to: 100; stepSize: 1
            onMoved: {
                if (typeof AudioService !== "undefined")
                    AudioService.setMasterVolume(value)
            }
        }

        SettingsSlider {
            label: "Mic Gain"
            configPath: "audio.microphone.gain"
            from: 0.5; to: 4.0; stepSize: 0.1
        }

        SectionHeader { text: "Devices" }

        FullScreenPicker {
            id: outputPicker
            label: "Output Device"
            model: typeof AudioOutputDeviceModel !== "undefined" ? AudioOutputDeviceModel : null
            textRole: "description"
            onActivated: function(index) {
                if (typeof AudioOutputDeviceModel === "undefined") return
                var nodeName = AudioOutputDeviceModel.data(
                    AudioOutputDeviceModel.index(index, 0), Qt.UserRole + 1)
                ConfigService.setValue("audio.output_device", nodeName)
                ConfigService.save()
                if (typeof AudioService !== "undefined")
                    AudioService.setOutputDevice(nodeName)
                restartBanner.shown = true
            }
            Component.onCompleted: {
                if (typeof AudioOutputDeviceModel === "undefined") return
                var current = ConfigService.value("audio.output_device")
                if (!current) current = "auto"
                var idx = AudioOutputDeviceModel.indexOfDevice(current)
                if (idx >= 0) currentIndex = idx
            }
        }

        FullScreenPicker {
            id: inputPicker
            label: "Input Device"
            model: typeof AudioInputDeviceModel !== "undefined" ? AudioInputDeviceModel : null
            textRole: "description"
            onActivated: function(index) {
                if (typeof AudioInputDeviceModel === "undefined") return
                var nodeName = AudioInputDeviceModel.data(
                    AudioInputDeviceModel.index(index, 0), Qt.UserRole + 1)
                ConfigService.setValue("audio.input_device", nodeName)
                ConfigService.save()
                if (typeof AudioService !== "undefined")
                    AudioService.setInputDevice(nodeName)
                restartBanner.shown = true
            }
            Component.onCompleted: {
                if (typeof AudioInputDeviceModel === "undefined") return
                var current = ConfigService.value("audio.input_device")
                if (!current) current = "auto"
                var idx = AudioInputDeviceModel.indexOfDevice(current)
                if (idx >= 0) currentIndex = idx
            }
        }

        InfoBanner {
            id: restartBanner
            text: "Restart required to apply device changes"
            shown: false
        }
    }
}
