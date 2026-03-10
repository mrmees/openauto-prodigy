import QtQuick
import QtQuick.Effects
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

        SectionHeader { text: "Android Auto" }

        SettingsToggle {
            label: "Auto-connect"
            configPath: "connection.auto_connect_aa"
        }

        ReadOnlyField {
            label: "TCP Port"
            configPath: "connection.tcp_port"
            placeholder: "5277"
        }

        SectionHeader { text: "Protocol Capture" }

        SettingsToggle {
            label: "Enable Capture"
            configPath: "connection.protocol_capture.enabled"
        }

        SegmentedButton {
            label: "Format"
            configPath: "connection.protocol_capture.format"
            options: ["JSONL", "TSV"]
            values: ["jsonl", "tsv"]
        }

        SettingsToggle {
            label: "Include Media Frames"
            configPath: "connection.protocol_capture.include_media"
        }

        ReadOnlyField {
            label: "Capture Path"
            configPath: "connection.protocol_capture.path"
            placeholder: "/tmp/oaa-protocol-capture.jsonl"
        }

        SectionHeader { text: "WiFi Access Point" }

        FullScreenPicker {
            label: "Channel"
            configPath: "connection.wifi_ap.channel"
            options: ["1","2","3","4","5","6","7","8","9","10","11","36","40","44","48"]
            values: [1,2,3,4,5,6,7,8,9,10,11,36,40,44,48]
            restartRequired: true
        }

        SegmentedButton {
            label: "Band"
            configPath: "connection.wifi_ap.band"
            options: ["2.4 GHz", "5 GHz"]
            values: ["g", "a"]
            restartRequired: true
        }

        SectionHeader { text: "Bluetooth" }

        ReadOnlyField {
            label: "Device Name"
            configPath: "connection.bt_name"
            placeholder: "OpenAutoProdigy"
        }

        // Repurposed: controls Pairable (not Discoverable)
        Item {
            Layout.fillWidth: true
            implicitHeight: UiMetrics.rowH
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginPage
                anchors.rightMargin: UiMetrics.marginPage
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
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left; anchors.right: parent.right
                anchors.leftMargin: UiMetrics.marginPage; anchors.rightMargin: UiMetrics.marginPage
                height: 1; color: ThemeService.outlineVariant
            }
        }

        // Paired devices list
        Repeater {
            model: PairedDevicesModel
            delegate: Item {
                Layout.fillWidth: true
                implicitHeight: UiMetrics.rowH
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: UiMetrics.marginPage
                    anchors.rightMargin: UiMetrics.marginPage
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
                    Item {
                        Layout.preferredWidth: forgetText.implicitWidth + UiMetrics.gap * 2
                        Layout.preferredHeight: UiMetrics.touchMin

                        readonly property bool _isPressed: forgetArea.pressed

                        scale: _isPressed ? 0.95 : 1.0
                        Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

                        // Background source for shadow
                        Rectangle {
                            id: forgetBg
                            anchors.fill: parent
                            radius: UiMetrics.touchMin / 2
                            color: "transparent"
                            border.color: ThemeService.error
                            border.width: 1
                            layer.enabled: true
                            visible: false
                        }

                        // Shadow effect (Level 2 resting, reduced on press)
                        MultiEffect {
                            source: forgetBg
                            anchors.fill: forgetBg
                            shadowEnabled: true
                            shadowColor: ThemeService.shadow
                            shadowBlur: parent._isPressed ? 0.25 : 0.50
                            shadowVerticalOffset: parent._isPressed ? 2 : 4
                            shadowOpacity: parent._isPressed ? 0.15 : 0.30
                            shadowHorizontalOffset: 0
                            shadowScale: 1.0
                            autoPaddingEnabled: true

                            Behavior on shadowBlur { NumberAnimation { duration: UiMetrics.animDurationFast } }
                            Behavior on shadowVerticalOffset { NumberAnimation { duration: UiMetrics.animDurationFast } }
                            Behavior on shadowOpacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
                        }

                        // State layer overlay
                        Rectangle {
                            anchors.fill: parent
                            radius: UiMetrics.touchMin / 2
                            color: ThemeService.onSurface
                            opacity: parent._isPressed ? 0.10 : 0.0
                            Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
                        }

                        Text {
                            id: forgetText
                            anchors.centerIn: parent
                            text: "Forget"
                            font.pixelSize: UiMetrics.fontSmall
                            color: ThemeService.error
                        }

                        MouseArea {
                            id: forgetArea
                            anchors.fill: parent
                            onClicked: {
                                if (BluetoothManager) BluetoothManager.forgetDevice(model.address)
                            }
                        }
                    }
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.leftMargin: UiMetrics.marginPage; anchors.rightMargin: UiMetrics.marginPage
                    height: 1; color: ThemeService.outlineVariant
                }
            }
        }
    }
}
