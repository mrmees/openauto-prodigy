import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    property StackView stackRef: StackView.view

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        SettingsPageHeader {
            title: "System"
            stack: root.stackRef
        }

        SectionHeader { text: "Identity" }

        SettingsTextField {
            label: "Head Unit Name"
            configPath: "identity.head_unit_name"
        }

        SettingsTextField {
            label: "Manufacturer"
            configPath: "identity.manufacturer"
        }

        SettingsTextField {
            label: "Model"
            configPath: "identity.model"
        }

        SettingsTextField {
            label: "Software Version"
            configPath: "identity.sw_version"
            readOnly: true
        }

        SettingsTextField {
            label: "Car Model"
            configPath: "identity.car_model"
            placeholder: "(optional)"
        }

        SettingsTextField {
            label: "Car Year"
            configPath: "identity.car_year"
            placeholder: "(optional)"
        }

        SettingsToggle {
            label: "Left-Hand Drive"
            configPath: "identity.left_hand_drive"
        }

        SectionHeader { text: "Hardware" }

        SettingsTextField {
            label: "Hardware Profile"
            configPath: "hardware_profile"
        }

        SettingsTextField {
            label: "Touch Device"
            configPath: "touch.device"
            placeholder: "(auto-detect)"
        }

        SectionHeader { text: "Plugins" }

        Text {
            text: "Plugin management coming in a future update."
            font.pixelSize: UiMetrics.fontSmall
            font.italic: true
            color: ThemeService.descriptionFontColor
            Layout.fillWidth: true
            Layout.leftMargin: UiMetrics.marginRow
        }
    }
}
