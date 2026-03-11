import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    objectName: "Audio"
    signal openEqualizer()
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: UiMetrics.settingsPageInset
        anchors.rightMargin: UiMetrics.settingsPageInset
        anchors.topMargin: UiMetrics.marginPage
        spacing: 0

        SectionHeader { text: "Output" }

        SettingsRow { rowIndex: 0
            SettingsSlider {
                label: "Master Volume"
                configPath: "audio.master_volume"
                from: 0; to: 100; stepSize: 1
                onMoved: {
                    if (typeof AudioService !== "undefined")
                        AudioService.setMasterVolume(value)
                }
            }
        }

        SettingsRow { rowIndex: 1
            FullScreenPicker {
                id: outputPicker
                flat: true
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

        SectionHeader { text: "Microphone" }

        SettingsRow { rowIndex: 0
            SettingsSlider {
                label: "Mic Gain"
                configPath: "audio.microphone.gain"
                from: 0.5; to: 4.0; stepSize: 0.1
            }
        }

        SettingsRow { rowIndex: 1
            FullScreenPicker {
                id: inputPicker
                flat: true
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

        SectionHeader { text: "Equalizer" }

        SettingsRow { rowIndex: 0
            SettingsListItem {
                flat: true
                icon: "\ue429"
                label: {
                    if (typeof EqualizerService === "undefined") return "Equalizer"
                    var preset = EqualizerService.activePresetForStream(0)
                    return preset !== "" ? "Equalizer \u2014 " + preset : "Equalizer"
                }
                onClicked: root.openEqualizer()
            }
        }
    }

    SettingsScrollHints {
        flickable: root
    }
}
