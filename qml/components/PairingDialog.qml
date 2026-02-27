import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: overlay
    anchors.fill: parent
    color: "#CC000000"
    visible: BluetoothManager ? BluetoothManager.pairingActive : false
    z: 998

    // Block touch passthrough
    MouseArea {
        anchors.fill: parent
        onClicked: {} // absorb
    }

    // Center panel
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.7, 500)
        height: col.implicitHeight + UiMetrics.marginPage * 2
        radius: 12
        color: ThemeService.backgroundColor

        ColumnLayout {
            id: col
            anchors.fill: parent
            anchors.margins: UiMetrics.marginPage
            spacing: UiMetrics.gap

            Text {
                text: "Pair with device?"
                font.pixelSize: UiMetrics.fontHeader
                color: ThemeService.normalFontColor
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: BluetoothManager ? BluetoothManager.pairingDeviceName : ""
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.descriptionFontColor
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: BluetoothManager ? BluetoothManager.pairingPasskey : ""
                font.pixelSize: UiMetrics.fontHeader * 1.8
                font.weight: Font.Bold
                font.letterSpacing: 8
                color: ThemeService.normalFontColor
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: UiMetrics.gap
                Layout.bottomMargin: UiMetrics.gap
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: UiMetrics.gap * 2

                Rectangle {
                    width: 140; height: UiMetrics.rowH
                    radius: 8
                    color: "#cc4444"
                    Text {
                        anchors.centerIn: parent
                        text: "Reject"
                        font.pixelSize: UiMetrics.fontBody
                        color: "white"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: if (BluetoothManager) BluetoothManager.rejectPairing()
                    }
                }

                Rectangle {
                    width: 140; height: UiMetrics.rowH
                    radius: 8
                    color: "#44aa44"
                    Text {
                        anchors.centerIn: parent
                        text: "Confirm"
                        font.pixelSize: UiMetrics.fontBody
                        color: "white"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: if (BluetoothManager) BluetoothManager.confirmPairing()
                    }
                }
            }
        }
    }
}
