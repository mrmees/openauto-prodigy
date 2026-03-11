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

        SectionHeader { text: "General" }

        SettingsRow { rowIndex: 0
            SettingsToggle {
                label: "Left-Hand Drive"
                configPath: "identity.left_hand_drive"
            }
        }

        SettingsRow { rowIndex: 1
            SettingsToggle {
                label: "24-Hour Clock"
                configPath: "display.clock_24h"
            }
        }

        SettingsRow { rowIndex: 2
            SettingsToggle {
                id: forceDarkToggle
                label: "Always Use Dark Mode"
                configPath: "display.force_dark_mode"
            }
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
            implicitHeight: nightModeCol.implicitHeight
            opacity: ThemeService.forceDarkMode ? 0.4 : 1.0
            enabled: !ThemeService.forceDarkMode
            Behavior on opacity { NumberAnimation { duration: 200 } }

            ColumnLayout {
                id: nightModeCol
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 0

                SettingsRow { rowIndex: 0
                    FullScreenPicker {
                        id: nightSource
                        flat: true
                        label: "Source"
                        configPath: "sensors.night_mode.source"
                        options: ["time", "gpio", "none"]
                    }
                }

                SettingsRow { rowIndex: 1
                    visible: nightSource.currentValue === "time"
                    ReadOnlyField {
                        label: "Day starts at"
                        configPath: "sensors.night_mode.day_start"
                        placeholder: "HH:MM"
                    }
                }

                SettingsRow { rowIndex: 2
                    visible: nightSource.currentValue === "time"
                    ReadOnlyField {
                        label: "Night starts at"
                        configPath: "sensors.night_mode.night_start"
                        placeholder: "HH:MM"
                    }
                }

                SettingsRow { rowIndex: 1
                    visible: nightSource.currentValue === "gpio"
                    SettingsSlider {
                        label: "GPIO Pin"
                        configPath: "sensors.night_mode.gpio_pin"
                        from: 0; to: 40; stepSize: 1
                    }
                }

                SettingsRow { rowIndex: 2
                    visible: nightSource.currentValue === "gpio"
                    SettingsToggle {
                        label: "GPIO Active High"
                        configPath: "sensors.night_mode.gpio_active_high"
                    }
                }
            }
        }

        SectionHeader { text: "Software" }

        SettingsRow { rowIndex: 0
            ReadOnlyField {
                label: "Version"
                configPath: "identity.sw_version"
            }
        }

        SettingsRow {
            rowIndex: 1
            interactive: true
            onClicked: exitDialog.open()

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginRow
                anchors.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.gap

                MaterialIcon {
                    icon: "\ue8ac"
                    size: UiMetrics.iconSize
                    color: ThemeService.onSurface
                }

                Text {
                    text: "Close App"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                }

                MaterialIcon {
                    icon: "\ue5cc"
                    size: UiMetrics.iconSmall
                    color: ThemeService.onSurfaceVariant
                }
            }
        }
    }

    ExitDialog {
        id: exitDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
    }
}
