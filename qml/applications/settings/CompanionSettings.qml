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

        SectionHeader { text: "Status" }

        // Connection indicator
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

        SectionHeader { text: "Pairing" }

        // Pairing button
        Item {
            Layout.fillWidth: true
            implicitHeight: UiMetrics.touchMin + UiMetrics.gap

            Rectangle {
                id: pairBtn
                anchors.horizontalCenter: parent.horizontalCenter
                width: pairLabel.implicitWidth + UiMetrics.touchMin
                height: UiMetrics.touchMin
                radius: height / 2
                color: pairMouse.pressed
                       ? Qt.darker(ThemeService.barBackgroundColor, 1.3)
                       : ThemeService.barBackgroundColor
                border.color: ThemeService.descriptionFontColor
                border.width: 1
                opacity: root.hasService ? 1.0 : 0.4

                Text {
                    id: pairLabel
                    anchors.centerIn: parent
                    text: "Generate Pairing PIN"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.normalFontColor
                }

                MouseArea {
                    id: pairMouse
                    anchors.fill: parent
                    enabled: root.hasService
                    onClicked: {
                        var pin = CompanionService.generatePairingPin()
                        pinDisplay.text = pin
                        pinDisplay.visible = true
                        pinHint.visible = true
                    }
                }
            }
        }

        // PIN display (hidden until generated)
        Text {
            id: pinDisplay
            visible: false
            Layout.alignment: Qt.AlignHCenter
            font.pixelSize: Math.round(48 * UiMetrics.scale)
            font.bold: true
            font.family: "monospace"
            font.letterSpacing: Math.round(8 * UiMetrics.scale)
            color: ThemeService.specialFontColor
        }

        Text {
            id: pinHint
            visible: false
            Layout.alignment: Qt.AlignHCenter
            text: "Enter this PIN in the companion app on your phone"
            font.pixelSize: UiMetrics.fontSmall
            font.italic: true
            color: ThemeService.descriptionFontColor
        }

        SectionHeader { text: "Configuration" }

        SettingsToggle {
            label: "Companion Enabled"
            configPath: "companion.enabled"
            restartRequired: true
        }

        ReadOnlyField {
            label: "Listen Port"
            configPath: "companion.port"
            placeholder: "9876"
        }

        InfoBanner {
            text: "The companion app shares time, GPS, battery, and internet from your phone to the head unit."
            shown: true
        }
    }
}
