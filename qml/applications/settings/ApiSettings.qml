import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

// The merged "Companion" settings page (design 2026-07-14): API v1 pairing,
// live phone status (CompanionState = ApiInboundState), and the API toggles.
// The legacy CompanionSettings page died here — its status rows moved in,
// its 9876-era pairing controls were deleted (design 2026-07-11 §B2 content,
// retired early because they drove a disabled listener).
Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    // Guards: ApiService may not exist if the external API is disabled or
    // failed; CompanionState is set alongside it in main.cpp. apiRunning
    // additionally requires a live listener — main.cpp exposes ApiService
    // even when start() failed, and pairing against a dead server is the
    // zombie UI this page's merge removed.
    readonly property bool hasService: typeof ApiService !== "undefined"
    readonly property bool hasState: typeof CompanionState !== "undefined"
    readonly property bool apiRunning: hasService && ApiService.running

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: UiMetrics.settingsPageInset
        anchors.rightMargin: UiMetrics.settingsPageInset
        anchors.topMargin: UiMetrics.marginPage
        spacing: 0

        SectionHeader { text: "Remote Client Pairing" }

        // Pairing status + start/cancel button
        SettingsRow { rowIndex: 0
            RowLayout {
                anchors.fill: parent
                spacing: UiMetrics.gap

                Text {
                    text: {
                        if (!root.hasService) return "API service unavailable"
                        if (!root.apiRunning) return "API not running — enable it under Advanced"
                        return ApiService.pairingActive
                               ? "PIN: " + ApiService.pairingPin
                               : "No pairing in progress"
                    }
                    font.pixelSize: UiMetrics.fontBody
                    color: root.apiRunning && ApiService.pairingActive
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
                    opacity: root.apiRunning ? 1.0 : 0.4

                    Text {
                        id: pairLabel
                        anchors.centerIn: parent
                        text: root.apiRunning && ApiService.pairingActive
                              ? "Cancel Pairing" : "Start Pairing"
                        font.pixelSize: UiMetrics.fontSmall
                        color: ThemeService.onSurface
                    }

                    SettingsHoldArea {
                        id: pairArea
                        anchors.fill: parent
                        enabled: root.apiRunning
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

        SectionHeader { text: "Phone Status" }

        // Connection indicator (API v1 inbound state)
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
                          : "—"
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
                          : "—"
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
                    text: root.hasState ? CompanionState.proxyAddress : "—"
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
                        if (s === "degraded") return "Degraded — retrying"
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
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                }
            }
        }

        SectionHeader { text: "Advanced" }

        SettingsRow { rowIndex: 0
            SettingsToggle {
                label: "External API Enabled"
                configPath: "api.enabled"
                restartRequired: true
            }
        }
        // The API is not companion-only — web widgets and any paired remote
        // client ride the same server.
        Text {
            text: "Powers companion, web widgets, and remote clients"
            font.pixelSize: UiMetrics.fontSmall
            color: ThemeService.onSurfaceVariant
            Layout.leftMargin: UiMetrics.gap
            Layout.bottomMargin: UiMetrics.gap
        }
        SettingsRow { rowIndex: 1
            SettingsToggle {
                label: "Allow LAN Clients"
                configPath: "api.expose_lan"
                restartRequired: true
            }
        }
    }

    SettingsScrollHints {
        flickable: root
    }
}
