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
            title: "Connection"
            stack: root.stackRef
        }

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

        SectionHeader { text: "WiFi Access Point" }

        Text {
            text: "SSID, password, and interface are configured via web panel"
            font.pixelSize: UiMetrics.fontTiny
            font.italic: true
            color: ThemeService.descriptionFontColor
            Layout.fillWidth: true
            Layout.leftMargin: UiMetrics.marginRow
        }

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

        SettingsToggle {
            label: "Discoverable"
            configPath: "connection.bt_discoverable"
        }
    }
}
