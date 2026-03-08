import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: overlay
    anchors.fill: parent
    color: ThemeService.scrim
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
        radius: UiMetrics.radius
        color: ThemeService.background

        ColumnLayout {
            id: col
            anchors.fill: parent
            anchors.margins: UiMetrics.marginPage
            spacing: UiMetrics.gap

            Text {
                text: "Pair with device?"
                font.pixelSize: UiMetrics.fontHeading
                color: ThemeService.textPrimary
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: BluetoothManager ? BluetoothManager.pairingDeviceName : ""
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.textSecondary
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: BluetoothManager ? BluetoothManager.pairingPasskey : ""
                font.pixelSize: UiMetrics.fontHeading * 1.8
                font.weight: Font.Bold
                font.letterSpacing: 8
                color: ThemeService.textPrimary
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: UiMetrics.gap
                Layout.bottomMargin: UiMetrics.gap
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: UiMetrics.gap * 2

                Rectangle {
                    width: Math.round(140 * UiMetrics.scale); height: UiMetrics.rowH
                    radius: UiMetrics.radiusSmall
                    color: ThemeService.red
                    Text {
                        anchors.centerIn: parent
                        text: "Reject"
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.onRed
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: if (BluetoothManager) BluetoothManager.rejectPairing()
                    }
                }

                Rectangle {
                    width: Math.round(140 * UiMetrics.scale); height: UiMetrics.rowH
                    radius: UiMetrics.radiusSmall
                    color: ThemeService.success
                    Text {
                        anchors.centerIn: parent
                        text: "Confirm"
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.onSuccess
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
