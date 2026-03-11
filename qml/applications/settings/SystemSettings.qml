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

        SectionHeader { text: "General" }

        SettingsToggle {
            label: "Left-Hand Drive"
            configPath: "identity.left_hand_drive"
        }

        SettingsToggle {
            label: "24-Hour Clock"
            configPath: "display.clock_24h"
        }

        SettingsToggle {
            id: forceDarkToggle
            label: "Always Use Dark Mode"
            configPath: "display.force_dark_mode"
        }

        Connections {
            target: ConfigService
            function onConfigChanged(path, value) {
                if (path === "display.force_dark_mode")
                    ThemeService.forceDarkMode = (value === true || value === 1 || value === "true")
            }
        }

        SectionHeader { text: "Day / Night Mode" }

        Item {
            Layout.fillWidth: true
            implicitHeight: nightModeControls.implicitHeight
            opacity: ThemeService.forceDarkMode ? 0.4 : 1.0
            enabled: !ThemeService.forceDarkMode
            Behavior on opacity { NumberAnimation { duration: 200 } }

            ColumnLayout {
                id: nightModeControls
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: UiMetrics.spacing

                FullScreenPicker {
                    id: nightSource
                    label: "Source"
                    configPath: "sensors.night_mode.source"
                    options: ["time", "gpio", "none"]
                }

                ReadOnlyField {
                    label: "Day starts at"
                    configPath: "sensors.night_mode.day_start"
                    placeholder: "HH:MM"
                    visible: nightSource.currentValue === "time"
                }

                ReadOnlyField {
                    label: "Night starts at"
                    configPath: "sensors.night_mode.night_start"
                    placeholder: "HH:MM"
                    visible: nightSource.currentValue === "time"
                }

                SettingsSlider {
                    label: "GPIO Pin"
                    configPath: "sensors.night_mode.gpio_pin"
                    from: 0; to: 40; stepSize: 1
                    visible: nightSource.currentValue === "gpio"
                }

                SettingsToggle {
                    label: "GPIO Active High"
                    configPath: "sensors.night_mode.gpio_active_high"
                    visible: nightSource.currentValue === "gpio"
                }
            }
        }

        SectionHeader { text: "Software" }

        ReadOnlyField {
            label: "Version"
            configPath: "identity.sw_version"
        }

        Item { Layout.fillWidth: true; Layout.preferredHeight: UiMetrics.marginPage + UiMetrics.marginRow }

        ElevatedButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: parent.width * 0.4
            Layout.preferredHeight: UiMetrics.rowH
            text: "Close App"
            iconCode: "\ue5cd"
            onClicked: exitDialog.open()
        }
    }

    ExitDialog {
        id: exitDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
    }
}
