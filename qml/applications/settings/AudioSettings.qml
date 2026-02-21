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
        }

        SettingsTextField {
            label: "Output Device"
            configPath: "audio.output_device"
            placeholder: "auto"
        }

        SettingsTextField {
            label: "Microphone"
            configPath: "audio.microphone.device"
            placeholder: "auto"
        }

        SettingsSlider {
            label: "Mic Gain"
            configPath: "audio.microphone.gain"
            from: 0.5; to: 4.0; stepSize: 0.1
        }
    }
}
