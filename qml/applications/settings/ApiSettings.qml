import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    // Guard: ApiService may not exist if the external API is disabled/unavailable
    readonly property bool hasService: typeof ApiService !== "undefined"

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: UiMetrics.settingsPageInset
        anchors.rightMargin: UiMetrics.settingsPageInset
        anchors.topMargin: UiMetrics.marginPage
        spacing: 0

        SettingsRow { rowIndex: 0
            SettingsToggle {
                label: "External API Enabled"
                configPath: "api.enabled"
                restartRequired: true
            }
        }
        SettingsRow { rowIndex: 1
            SettingsToggle {
                label: "Allow LAN Clients"
                configPath: "api.expose_lan"
                restartRequired: true
            }
        }

        SectionHeader { text: "Remote Client Pairing" }

        // Pairing status + start/cancel button
        SettingsRow { rowIndex: 0
            RowLayout {
                anchors.fill: parent
                spacing: UiMetrics.gap

                Text {
                    text: {
                        if (!root.hasService) return "API service unavailable"
                        return ApiService.pairingActive
                               ? "PIN: " + ApiService.pairingPin
                               : "No pairing in progress"
                    }
                    font.pixelSize: UiMetrics.fontBody
                    color: root.hasService && ApiService.pairingActive
                           ? ThemeService.primary : ThemeService.onSurface
                    Layout.fillWidth: true
                }

                Rectangle {
                    width: pairLabel.implicitWidth + UiMetrics.gap * 2
                    height: UiMetrics.touchMin
                    radius: height / 2
                    color: pairArea.pressed
                           ? Qt.darker(ThemeService.surfaceContainerLow, 1.3)
                           : ThemeService.surfaceContainerLow
                    border.color: ThemeService.onSurfaceVariant
                    border.width: 1
                    opacity: root.hasService ? 1.0 : 0.4

                    Text {
                        id: pairLabel
                        anchors.centerIn: parent
                        text: root.hasService && ApiService.pairingActive
                              ? "Cancel Pairing" : "Start Pairing"
                        font.pixelSize: UiMetrics.fontSmall
                        color: ThemeService.onSurface
                    }

                    SettingsHoldArea {
                        id: pairArea
                        anchors.fill: parent
                        enabled: root.hasService
                        onShortClicked: ApiService.pairingActive
                                        ? ApiService.cancelPairing()
                                        : ApiService.startPairing()
                    }
                }
            }
        }

        // Scannable pairing QR (prodigy://pair?...) — companion app scans
        // this instead of typing the PIN. Shown only while a window is open.
        Image {
            visible: root.hasService && ApiService.pairingActive
                     && ApiService.pairingQrDataUri !== ""
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: UiMetrics.gap
            Layout.preferredWidth: Math.round(200 * UiMetrics.scale)
            Layout.preferredHeight: Math.round(200 * UiMetrics.scale)
            source: root.hasService ? ApiService.pairingQrDataUri : ""
            fillMode: Image.PreserveAspectFit
            smooth: false
        }
    }

    SettingsScrollHints {
        flickable: root
    }
}
