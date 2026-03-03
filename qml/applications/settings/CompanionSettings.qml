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

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        SettingsToggle {
            label: "Companion Enabled"
            configPath: "companion.enabled"
            restartRequired: true
        }

        SectionHeader { text: "Status" }

        // Connection indicator + pairing button
        Item {
            Layout.fillWidth: true
            implicitHeight: UiMetrics.rowH

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginRow
                anchors.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.gap

                Rectangle {
                    width: UiMetrics.iconSmall
                    height: UiMetrics.iconSmall
                    radius: width / 2
                    color: root.hasService && CompanionService.connected
                           ? "#4CAF50" : ThemeService.descriptionFontColor
                }

                Text {
                    text: {
                        if (!root.hasService) return "Companion service disabled"
                        return CompanionService.connected ? "Phone Connected" : "Not Connected"
                    }
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.normalFontColor
                    Layout.fillWidth: true
                }

                Rectangle {
                    width: pairBtnLabel.implicitWidth + UiMetrics.gap * 2
                    height: UiMetrics.touchMin
                    radius: height / 2
                    color: pairBtnMouse.pressed
                           ? Qt.darker(ThemeService.barBackgroundColor, 1.3)
                           : ThemeService.barBackgroundColor
                    border.color: ThemeService.descriptionFontColor
                    border.width: 1
                    opacity: root.hasService ? 1.0 : 0.4

                    Text {
                        id: pairBtnLabel
                        anchors.centerIn: parent
                        text: "Generate Pairing Code"
                        font.pixelSize: UiMetrics.fontSmall
                        color: ThemeService.normalFontColor
                    }

                    MouseArea {
                        id: pairBtnMouse
                        anchors.fill: parent
                        enabled: root.hasService
                        onClicked: {
                            var pin = CompanionService.generatePairingPin()
                            pairingCodeDialog.pinCode = pin
                            pairingCodeDialog.visible = true
                        }
                    }
                }
            }
        }

        // GPS info (visible when connected and not stale)
        Item {
            Layout.fillWidth: true
            implicitHeight: UiMetrics.rowH
            visible: root.hasService && CompanionService.connected && !CompanionService.gpsStale

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginRow
                anchors.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.gap

                Text {
                    text: "GPS"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.normalFontColor
                    Layout.preferredWidth: parent.width * 0.35
                }

                Text {
                    text: root.hasService
                          ? CompanionService.gpsLat.toFixed(4) + ", " + CompanionService.gpsLon.toFixed(4)
                          : "\u2014"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.normalFontColor
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                }
            }
        }

        // Battery info (visible when connected and battery reported)
        Item {
            Layout.fillWidth: true
            implicitHeight: UiMetrics.rowH
            visible: root.hasService && CompanionService.connected && CompanionService.phoneBattery >= 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginRow
                anchors.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.gap

                Text {
                    text: "Phone Battery"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.normalFontColor
                    Layout.preferredWidth: parent.width * 0.35
                }

                Text {
                    text: root.hasService
                          ? CompanionService.phoneBattery + "%"
                            + (CompanionService.phoneCharging ? " (charging)" : "")
                          : "\u2014"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.normalFontColor
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                }
            }
        }

        // Internet proxy (visible when available)
        Item {
            Layout.fillWidth: true
            implicitHeight: UiMetrics.rowH
            visible: root.hasService && CompanionService.internetAvailable

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginRow
                anchors.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.gap

                Text {
                    text: "Internet Proxy"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.normalFontColor
                    Layout.preferredWidth: parent.width * 0.35
                }

                Text {
                    text: root.hasService ? CompanionService.proxyAddress : "\u2014"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.normalFontColor
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }
        }

        // Proxy route status (visible when internet available)
        Item {
            id: routeStatusRow
            Layout.fillWidth: true
            implicitHeight: UiMetrics.rowH
            visible: root.hasService && CompanionService.internetAvailable

            readonly property bool hasSysService: typeof SystemService !== "undefined"
            readonly property string routeStateStr: hasSysService ? SystemService.routeState : "disabled"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginRow
                anchors.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.gap

                Text {
                    text: "Route Active"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.normalFontColor
                    Layout.preferredWidth: parent.width * 0.35
                }

                Rectangle {
                    width: UiMetrics.iconSmall
                    height: UiMetrics.iconSmall
                    radius: width / 2
                    color: {
                        var s = routeStatusRow.routeStateStr
                        if (s === "active")   return "#4CAF50"
                        if (s === "degraded") return "#FF9800"
                        if (s === "failed")   return "#F44336"
                        return ThemeService.descriptionFontColor
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
                        if (s === "degraded") return "#FF9800"
                        if (s === "failed")   return "#F44336"
                        return ThemeService.normalFontColor
                    }
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

    }

    // Pairing code popup — tap anywhere to dismiss
    Rectangle {
        id: pairingCodeDialog
        property string pinCode: ""
        anchors.fill: parent
        color: "#CC000000"
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
            radius: 12
            color: ThemeService.backgroundColor

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
                    color: ThemeService.normalFontColor
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }
    }
}
