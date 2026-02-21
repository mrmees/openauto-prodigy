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
            title: "Connection"
            stack: root.stackRef
        }

        SectionHeader { text: "Android Auto" }

        SettingsToggle {
            label: "Auto-connect"
            configPath: "connection.auto_connect_aa"
        }

        SettingsSpinBox {
            label: "TCP Port"
            configPath: "connection.tcp_port"
            from: 1024; to: 65535
            restartRequired: true
        }

        SectionHeader { text: "WiFi Access Point" }

        SettingsTextField {
            label: "Interface"
            configPath: "connection.wifi_ap.interface"
            restartRequired: true
        }

        SettingsTextField {
            label: "SSID"
            configPath: "connection.wifi_ap.ssid"
            restartRequired: true
        }

        SettingsTextField {
            label: "Password"
            configPath: "connection.wifi_ap.password"
            restartRequired: true
        }

        SettingsSpinBox {
            label: "Channel"
            configPath: "connection.wifi_ap.channel"
            from: 1; to: 165
            restartRequired: true
        }

        SettingsComboBox {
            label: "Band"
            configPath: "connection.wifi_ap.band"
            options: ["a", "g"]
            restartRequired: true
        }

        SectionHeader { text: "Bluetooth" }

        SettingsToggle {
            label: "Discoverable"
            configPath: "connection.bt_discoverable"
        }
    }
}
