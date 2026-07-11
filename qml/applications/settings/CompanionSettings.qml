import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    // Guard: CompanionService may not exist if disabled in config
    readonly property bool hasService: typeof CompanionService !== "undefined"
    // Guard: CompanionState (API inbound status) is set unconditionally; null-guarded per house style
    readonly property bool hasState: typeof CompanionState !== "undefined"

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: UiMetrics.settingsPageInset
        anchors.rightMargin: UiMetrics.settingsPageInset
        anchors.topMargin: UiMetrics.marginPage
        spacing: 0

        // Legacy pairing/enable rows: removed with CompanionListenerService at 9876 retirement (design 2026-07-11 §B2).
        SettingsRow { rowIndex: 0
            SettingsToggle {
                label: "Companion Enabled"
                configPath: "companion.enabled"
                restartRequired: true
            }
        }

        SectionHeader { text: "Status" }

        // Connection indicator (API state) + legacy pairing button
        SettingsRow { rowIndex: 0
            RowLayout {
                anchors.fill: parent
                spacing: UiMetrics.gap

                Rectangle {
                    width: UiMetrics.iconSmall
                    height: UiMetrics.iconSmall
                    radius: width / 2
                    color: root.hasState && CompanionState.connected
                           ? ThemeService.success : ThemeService.onSurfaceVariant
                }

                Text {
                    text: {
                        if (!root.hasState) return "Companion state unavailable"
                        return CompanionState.connected ? "Phone Connected" : "Not Connected"
                    }
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                }

                // Legacy pairing/enable rows: removed with CompanionListenerService at 9876 retirement (design 2026-07-11 §B2).
                Rectangle {
                    width: pairBtnLabel.implicitWidth + UiMetrics.gap * 2
                    height: UiMetrics.touchMin
                    radius: height / 2
                    color: pairBtnMouse.pressed
                           ? Qt.darker(ThemeService.surfaceContainerLow, 1.3)
                           : ThemeService.surfaceContainerLow
                    border.color: ThemeService.onSurfaceVariant
                    border.width: 1
                    opacity: root.hasService ? 1.0 : 0.4

                    Text {
                        id: pairBtnLabel
                        anchors.centerIn: parent
                        text: "Generate Pairing Code"
                        font.pixelSize: UiMetrics.fontSmall
                        color: ThemeService.onSurface
                    }

                    SettingsHoldArea {
                        id: pairBtnMouse
                        anchors.fill: parent
                        enabled: root.hasService
                        onShortClicked: {
                            var pin = CompanionService.generatePairingPin()
                            pairingCodeDialog.pinCode = pin
                            pairingCodeDialog.visible = true
                        }
                    }
                }
            }
        }

        // GPS info (visible when connected and not stale)
        SettingsRow {
            rowIndex: 1
            visible: root.hasState && CompanionState.connected && !CompanionState.gpsStale

            RowLayout {
                anchors.fill: parent
                spacing: UiMetrics.gap

                Text {
                    text: "GPS"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.preferredWidth: root.width * 0.35
                }

                Text {
                    text: root.hasState
                          ? CompanionState.gpsLat.toFixed(4) + ", " + CompanionState.gpsLon.toFixed(4)
                          : "\u2014"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                }
            }
        }

        // Battery info (visible when connected and battery reported)
        SettingsRow {
            rowIndex: 2
            visible: root.hasState && CompanionState.connected && CompanionState.phoneBattery >= 0

            RowLayout {
                anchors.fill: parent
                spacing: UiMetrics.gap

                Text {
                    text: "Phone Battery"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.preferredWidth: root.width * 0.35
                }

                Text {
                    text: root.hasState
                          ? CompanionState.phoneBattery + "%"
                            + (CompanionState.phoneCharging ? " (charging)" : "")
                          : "\u2014"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                }
            }
        }

        // Internet proxy (visible when available)
        SettingsRow {
            rowIndex: 3
            visible: root.hasState && CompanionState.internetAvailable

            RowLayout {
                anchors.fill: parent
                spacing: UiMetrics.gap

                Text {
                    text: "Internet Proxy"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.preferredWidth: root.width * 0.35
                }

                Text {
                    text: root.hasState ? CompanionState.proxyAddress : "\u2014"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }
        }

        // Proxy route status (visible when internet available)
        SettingsRow {
            id: routeStatusRow
            rowIndex: 4
            visible: root.hasState && CompanionState.internetAvailable

            readonly property bool hasSysService: typeof SystemService !== "undefined"
            readonly property string routeStateStr: hasSysService ? SystemService.routeState : "disabled"

            RowLayout {
                anchors.fill: parent
                spacing: UiMetrics.gap

                Text {
                    text: "Route Active"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.preferredWidth: root.width * 0.35
                }

                Rectangle {
                    width: UiMetrics.iconSmall
                    height: UiMetrics.iconSmall
                    radius: width / 2
                    color: {
                        var s = routeStatusRow.routeStateStr
                        if (s === "active")   return ThemeService.success
                        if (s === "degraded") return ThemeService.warning
                        if (s === "failed")   return ThemeService.error
                        return ThemeService.onSurfaceVariant
                    }
                }

                Text {
                    text: {
                        var s = routeStatusRow.routeStateStr
                        if (s === "active")   return "Routing via phone"
                        if (s === "degraded") return "Degraded \u2014 retrying"
                        if (s === "failed")   return "Failed"
                        return "Inactive"
                    }
                    font.pixelSize: UiMetrics.fontBody
                    color: {
                        var s = routeStatusRow.routeStateStr
                        if (s === "degraded") return ThemeService.warning
                        if (s === "failed")   return ThemeService.error
                        return ThemeService.onSurface
                    }
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

    }

    // Legacy pairing/enable rows: removed with CompanionListenerService at 9876 retirement (design 2026-07-11 §B2).
    // Pairing code popup -- tap anywhere to dismiss
    Rectangle {
        id: pairingCodeDialog
        property string pinCode: ""
        anchors.fill: parent
        color: ThemeService.scrim
        visible: false
        z: 998

        // Close on successful pairing
        Connections {
            target: root.hasService ? CompanionService : null
            function onConnectedChanged() {
                if (CompanionService.connected) pairingCodeDialog.visible = false
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: pairingCodeDialog.visible = false
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.7, 500)
            height: dialogCol.implicitHeight + UiMetrics.marginPage * 2
            radius: UiMetrics.radius
            color: ThemeService.background

            ColumnLayout {
                id: dialogCol
                anchors.fill: parent
                anchors.margins: UiMetrics.marginPage
                spacing: UiMetrics.gap

                Image {
                    visible: root.hasService && CompanionService.qrCodeDataUri !== ""
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.round(200 * UiMetrics.scale)
                    Layout.preferredHeight: Math.round(200 * UiMetrics.scale)
                    source: root.hasService ? CompanionService.qrCodeDataUri : ""
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                }

                Text {
                    text: pairingCodeDialog.pinCode
                    font.pixelSize: UiMetrics.fontHeading * 1.8
                    font.weight: Font.Bold
                    font.letterSpacing: 8
                    color: ThemeService.onSurface
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }
    }

    SettingsScrollHints {
        flickable: root
    }
}
