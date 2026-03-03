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

        SectionHeader { text: "Identity" }

        ReadOnlyField {
            label: "Head Unit Name"
            configPath: "identity.head_unit_name"
        }

        ReadOnlyField {
            label: "Manufacturer"
            configPath: "identity.manufacturer"
        }

        ReadOnlyField {
            label: "Model"
            configPath: "identity.model"
        }

        ReadOnlyField {
            label: "Software Version"
            configPath: "identity.sw_version"

        }

        ReadOnlyField {
            label: "Car Model"
            configPath: "identity.car_model"
            placeholder: "(optional)"
        }

        ReadOnlyField {
            label: "Car Year"
            configPath: "identity.car_year"
            placeholder: "(optional)"
        }

        SettingsToggle {
            label: "Left-Hand Drive"
            configPath: "identity.left_hand_drive"
        }

        SectionHeader { text: "Hardware" }

        ReadOnlyField {
            label: "Hardware Profile"
            configPath: "hardware_profile"
        }

        ReadOnlyField {
            label: "Touch Device"
            configPath: "touch.device"
            placeholder: "(auto-detect)"
        }

        SectionHeader { text: "About" }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: UiMetrics.marginRow
            spacing: UiMetrics.marginRow

            Text {
                text: "OpenAuto Prodigy"
                font.pixelSize: UiMetrics.fontHeading
                font.bold: true
                color: ThemeService.normalFontColor
            }
            Text {
                text: "Version " + (ConfigService.value("identity.sw_version") || "0.0.0")
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.descriptionFontColor
            }
        }

        Item { Layout.fillWidth: true; Layout.preferredHeight: UiMetrics.marginPage + UiMetrics.marginRow }

        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: parent.width * 0.4
            Layout.preferredHeight: UiMetrics.rowH
            onClicked: exitDialog.open()
            contentItem: RowLayout {
                spacing: UiMetrics.marginRow
                Item { Layout.fillWidth: true }
                MaterialIcon {
                    icon: "\ue5cd"
                    size: UiMetrics.iconSize
                    color: ThemeService.normalFontColor
                }
                Text {
                    text: "Close App"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.normalFontColor
                }
                Item { Layout.fillWidth: true }
            }
            background: Rectangle {
                color: parent.pressed ? ThemeService.highlightColor : ThemeService.barBackgroundColor
                radius: UiMetrics.radius
            }
        }

    }

    ExitDialog {
        id: exitDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
    }
}
