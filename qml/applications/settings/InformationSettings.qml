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
        anchors.leftMargin: UiMetrics.settingsPageInset
        anchors.rightMargin: UiMetrics.settingsPageInset
        anchors.topMargin: UiMetrics.marginPage
        spacing: 0

        SectionHeader { text: "Identity" }

        SettingsRow { rowIndex: 0
            ReadOnlyField {
                label: "Head Unit Name"
                configPath: "identity.head_unit_name"
            }
        }

        SettingsRow { rowIndex: 1
            ReadOnlyField {
                label: "Manufacturer"
                configPath: "identity.manufacturer"
            }
        }

        SettingsRow { rowIndex: 2
            ReadOnlyField {
                label: "Model"
                configPath: "identity.model"
            }
        }

        SettingsRow { rowIndex: 3
            ReadOnlyField {
                label: "Car Model"
                configPath: "identity.car_model"
                placeholder: "(optional)"
            }
        }

        SettingsRow { rowIndex: 4
            ReadOnlyField {
                label: "Car Year"
                configPath: "identity.car_year"
                placeholder: "(optional)"
            }
        }

        SectionHeader { text: "Hardware" }

        SettingsRow { rowIndex: 0
            ReadOnlyField {
                label: "Hardware Profile"
                configPath: "hardware_profile"
            }
        }

        SettingsRow { rowIndex: 1
            ReadOnlyField {
                label: "Touch Device"
                configPath: "touch.device"
                placeholder: "(auto-detect)"
            }
        }
    }

    SettingsScrollHints {
        flickable: root
    }
}
