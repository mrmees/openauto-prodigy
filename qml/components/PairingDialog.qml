import QtQuick
import QtQuick.Controls
import QtQuick.Effects
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

    // Center panel with Level 3 elevation
    Item {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.7, 500)
        height: col.implicitHeight + UiMetrics.marginPage * 2

        // Surface tint: blend 7% primary into surfaceContainerHigh
        Rectangle {
            id: panelBg
            anchors.fill: parent
            radius: UiMetrics.radius
            color: Qt.rgba(
                ThemeService.surfaceContainerHigh.r * 0.93 + ThemeService.primary.r * 0.07,
                ThemeService.surfaceContainerHigh.g * 0.93 + ThemeService.primary.g * 0.07,
                ThemeService.surfaceContainerHigh.b * 0.93 + ThemeService.primary.b * 0.07,
                1.0)
            layer.enabled: true
            visible: false
        }

        MultiEffect {
            source: panelBg
            anchors.fill: panelBg
            shadowEnabled: true
            shadowColor: ThemeService.shadow
            shadowBlur: 0.70
            shadowVerticalOffset: 6
            shadowOpacity: 0.35
            shadowHorizontalOffset: 0
            shadowScale: 1.0
            autoPaddingEnabled: true
        }

        ColumnLayout {
            id: col
            anchors.fill: parent
            anchors.margins: UiMetrics.marginPage
            spacing: UiMetrics.gap

            Text {
                text: "Pair with device?"
                font.pixelSize: UiMetrics.fontHeading
                color: ThemeService.onSurface
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: BluetoothManager ? BluetoothManager.pairingDeviceName : ""
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: BluetoothManager ? BluetoothManager.pairingPasskey : ""
                font.pixelSize: UiMetrics.fontHeading * 1.8
                font.weight: Font.Bold
                font.letterSpacing: 8
                color: ThemeService.onSurface
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: UiMetrics.gap
                Layout.bottomMargin: UiMetrics.gap
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: UiMetrics.gap * 2

                FilledButton {
                    text: "Reject"
                    implicitWidth: Math.round(140 * UiMetrics.scale)
                    implicitHeight: UiMetrics.rowH
                    buttonColor: ThemeService.error
                    pressedColor: Qt.darker(ThemeService.error, 1.15)
                    textColor: ThemeService.onError
                    pressedTextColor: ThemeService.onError
                    onClicked: if (BluetoothManager) BluetoothManager.rejectPairing()
                }

                FilledButton {
                    text: "Confirm"
                    implicitWidth: Math.round(140 * UiMetrics.scale)
                    implicitHeight: UiMetrics.rowH
                    buttonColor: ThemeService.success
                    pressedColor: Qt.darker(ThemeService.success, 1.2)
                    textColor: ThemeService.onSuccess
                    pressedTextColor: ThemeService.onSuccess
                    onClicked: if (BluetoothManager) BluetoothManager.confirmPairing()
                }
            }
        }
    }
}
