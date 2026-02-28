import QtQuick
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent
    visible: BluetoothManager ? (BluetoothManager.needsFirstPairing && !PluginModel.activePluginId) : false
    z: 100

    // Floating banner — horizontally centered, near top
    Rectangle {
        id: banner
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: UiMetrics.marginPage
        width: Math.min(parent.width * 0.85, 600)
        height: content.implicitHeight + UiMetrics.marginPage * 2
        radius: UiMetrics.radius
        color: ThemeService.controlBoxBackgroundColor

        RowLayout {
            id: content
            anchors.fill: parent
            anchors.margins: UiMetrics.marginPage
            spacing: UiMetrics.gap

            // Bluetooth searching icon
            MaterialIcon {
                icon: "\ue1a7"  // bluetooth_searching
                size: UiMetrics.iconSize
                color: ThemeService.highlightColor
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: UiMetrics.spacing

                Text {
                    text: "Ready to pair"
                    font.pixelSize: UiMetrics.fontBody
                    font.weight: Font.DemiBold
                    color: ThemeService.normalFontColor
                }

                Text {
                    text: "Search for \"" + (BluetoothManager ? BluetoothManager.adapterAlias : "") + "\" on your phone"
                    font.pixelSize: UiMetrics.fontSmall
                    color: ThemeService.descriptionFontColor
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            // Dismiss button
            Rectangle {
                width: UiMetrics.backBtnSize
                height: UiMetrics.backBtnSize
                radius: width / 2
                color: dismissArea.pressed ? ThemeService.highlightColor : "transparent"

                MaterialIcon {
                    anchors.centerIn: parent
                    icon: "\ue5cd"  // close
                    size: UiMetrics.iconSmall
                    color: ThemeService.descriptionFontColor
                }

                MouseArea {
                    id: dismissArea
                    anchors.fill: parent
                    onClicked: if (BluetoothManager) BluetoothManager.dismissFirstRunBanner()
                }
            }
        }
    }
}
