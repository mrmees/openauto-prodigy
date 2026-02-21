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
            title: "Display"
            stack: root.stackRef
        }

        SectionHeader { text: "General" }

        SettingsSlider {
            label: "Brightness"
            configPath: "display.brightness"
            from: 0; to: 100; stepSize: 1
        }

        SettingsComboBox {
            label: "Orientation"
            configPath: "display.orientation"
            options: ["landscape", "portrait"]
        }

        SectionHeader { text: "Day / Night Mode" }

        SettingsComboBox {
            id: nightSource
            label: "Source"
            configPath: "sensors.night_mode.source"
            options: ["time", "gpio", "none"]
        }

        SettingsTextField {
            label: "Day starts at"
            configPath: "sensors.night_mode.day_start"
            placeholder: "HH:MM"
            visible: nightSource.currentValue === "time"
        }

        SettingsTextField {
            label: "Night starts at"
            configPath: "sensors.night_mode.night_start"
            placeholder: "HH:MM"
            visible: nightSource.currentValue === "time"
        }

        SettingsSpinBox {
            label: "GPIO Pin"
            configPath: "sensors.night_mode.gpio_pin"
            from: 0; to: 40
            visible: nightSource.currentValue === "gpio"
        }

        SettingsToggle {
            label: "GPIO Active High"
            configPath: "sensors.night_mode.gpio_active_high"
            visible: nightSource.currentValue === "gpio"
        }
    }
}
