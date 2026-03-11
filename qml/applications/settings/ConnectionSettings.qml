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
        anchors.topMargin: UiMetrics.marginPage
        spacing: 0

        SettingsRow { rowIndex: 0
            ReadOnlyField {
                label: "Device Name"
                configPath: "connection.bt_name"
                placeholder: "OpenAutoProdigy"
            }
        }

        SettingsRow { rowIndex: 1
            Item {
                anchors.fill: parent

                RowLayout {
                    anchors.fill: parent
                    spacing: UiMetrics.gap
                    MaterialIcon { icon: "\ue1b7"; size: UiMetrics.iconSize; color: ThemeService.onSurface }
                    Text {
                        text: "Accept New Pairings"
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.onSurface
                        Layout.fillWidth: true
                    }
                    Switch {
                        id: pairableSwitch
                        checked: BluetoothManager ? BluetoothManager.pairable : false
                        onToggled: {
                            if (BluetoothManager) BluetoothManager.setPairable(checked)
                        }
                    }
                }
            }
        }

        // Paired devices list
        Repeater {
            model: PairedDevicesModel
            delegate: SettingsRow {
                rowIndex: index + 2

                Item {
                    anchors.fill: parent

                    RowLayout {
                        anchors.fill: parent
                        spacing: UiMetrics.gap
                        MaterialIcon {
                            icon: model.connected ? "\ue1ba" : "\ue1b9"
                            size: UiMetrics.iconSize
                            color: model.connected ? ThemeService.success : ThemeService.onSurfaceVariant
                        }
                        Text {
                            text: model.name || model.address
                            font.pixelSize: UiMetrics.fontBody
                            color: ThemeService.onSurface
                            Layout.fillWidth: true
                        }

                        // Simplified Forget button -- outlined, no shadow
                        Rectangle {
                            width: forgetText.implicitWidth + UiMetrics.gap * 2
                            height: UiMetrics.touchMin
                            radius: UiMetrics.touchMin / 2
                            color: "transparent"
                            border.color: ThemeService.error
                            border.width: 1

                            Text {
                                id: forgetText
                                anchors.centerIn: parent
                                text: "Forget"
                                font.pixelSize: UiMetrics.fontBody
                                color: ThemeService.error
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (BluetoothManager) BluetoothManager.forgetDevice(model.address)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
