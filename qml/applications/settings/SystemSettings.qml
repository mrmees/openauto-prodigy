import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        SectionHeader { text: "Identity" }

        ReadOnlyField {
            label: "Head Unit Name"
            configPath: "identity.head_unit_name"
        }

        ReadOnlyField {
            label: "Manufacturer"
            configPath: "identity.manufacturer"
        }

        ReadOnlyField {
            label: "Model"
            configPath: "identity.model"
        }

        ReadOnlyField {
            label: "Software Version"
            configPath: "identity.sw_version"

        }

        ReadOnlyField {
            label: "Car Model"
            configPath: "identity.car_model"
            placeholder: "(optional)"
        }

        ReadOnlyField {
            label: "Car Year"
            configPath: "identity.car_year"
            placeholder: "(optional)"
        }

        SettingsToggle {
            label: "Left-Hand Drive"
            configPath: "identity.left_hand_drive"
        }

        SectionHeader { text: "Hardware" }

        ReadOnlyField {
            label: "Hardware Profile"
            configPath: "hardware_profile"
        }

        ReadOnlyField {
            label: "Touch Device"
            configPath: "touch.device"
            placeholder: "(auto-detect)"
        }

        SectionHeader { text: "About" }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: UiMetrics.marginRow
            spacing: UiMetrics.marginRow

            Text {
                text: "OpenAuto Prodigy"
                font.pixelSize: UiMetrics.fontHeading
                font.bold: true
                color: ThemeService.textPrimary
            }
            Text {
                text: "Version " + (ConfigService.value("identity.sw_version") || "0.0.0")
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.textSecondary
            }
        }

        Item { Layout.fillWidth: true; Layout.preferredHeight: UiMetrics.marginPage + UiMetrics.marginRow }

        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: parent.width * 0.4
            Layout.preferredHeight: UiMetrics.rowH
            onClicked: exitDialog.open()
            contentItem: RowLayout {
                spacing: UiMetrics.marginRow
                Item { Layout.fillWidth: true }
                MaterialIcon {
                    icon: "\ue5cd"
                    size: UiMetrics.iconSize
                    color: ThemeService.textPrimary
                }
                Text {
                    text: "Close App"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.textPrimary
                }
                Item { Layout.fillWidth: true }
            }
            background: Rectangle {
                color: parent.pressed ? ThemeService.primary : ThemeService.surfaceContainerLow
                radius: UiMetrics.radius
            }
        }

        SectionHeader { text: "Debug: AA Protocol" }

        Text {
            text: "Test outbound commands (requires active AA connection)"
            font.pixelSize: UiMetrics.fontCaption
            color: ThemeService.textSecondary
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }

        property bool aaConnected: AAOrchestrator !== null && AAOrchestrator.connectionState === 3

        RowLayout {
            Layout.fillWidth: true
            spacing: UiMetrics.spacing

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: UiMetrics.rowH
                enabled: aaConnected
                onClicked: AAOrchestrator.sendButtonPress(85)
                contentItem: Text {
                    text: "Play/Pause"
                    font.pixelSize: UiMetrics.fontBody
                    color: parent.enabled ? ThemeService.textPrimary : ThemeService.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.primary : ThemeService.surfaceContainerLow
                    radius: UiMetrics.radius
                    opacity: parent.enabled ? 1.0 : 0.4
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: UiMetrics.rowH
                enabled: aaConnected
                onClicked: AAOrchestrator.sendButtonPress(88)
                contentItem: Text {
                    text: "Prev"
                    font.pixelSize: UiMetrics.fontBody
                    color: parent.enabled ? ThemeService.textPrimary : ThemeService.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.primary : ThemeService.surfaceContainerLow
                    radius: UiMetrics.radius
                    opacity: parent.enabled ? 1.0 : 0.4
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: UiMetrics.rowH
                enabled: aaConnected
                onClicked: AAOrchestrator.sendButtonPress(87)
                contentItem: Text {
                    text: "Next"
                    font.pixelSize: UiMetrics.fontBody
                    color: parent.enabled ? ThemeService.textPrimary : ThemeService.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.primary : ThemeService.surfaceContainerLow
                    radius: UiMetrics.radius
                    opacity: parent.enabled ? 1.0 : 0.4
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: UiMetrics.spacing

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: UiMetrics.rowH
                enabled: aaConnected
                onClicked: AAOrchestrator.sendButtonPress(84)
                contentItem: Text {
                    text: "Search (84)"
                    font.pixelSize: UiMetrics.fontBody
                    color: parent.enabled ? ThemeService.textPrimary : ThemeService.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.primary : ThemeService.surfaceContainerLow
                    radius: UiMetrics.radius
                    opacity: parent.enabled ? 1.0 : 0.4
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: UiMetrics.rowH
                enabled: aaConnected
                onClicked: AAOrchestrator.sendButtonPress(219)
                contentItem: Text {
                    text: "Assist (219)"
                    font.pixelSize: UiMetrics.fontBody
                    color: parent.enabled ? ThemeService.textPrimary : ThemeService.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.primary : ThemeService.surfaceContainerLow
                    radius: UiMetrics.radius
                    opacity: parent.enabled ? 1.0 : 0.4
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: UiMetrics.rowH
                enabled: aaConnected
                onClicked: AAOrchestrator.sendButtonPress(231)
                contentItem: Text {
                    text: "Voice (231)"
                    font.pixelSize: UiMetrics.fontBody
                    color: parent.enabled ? ThemeService.textPrimary : ThemeService.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.primary : ThemeService.surfaceContainerLow
                    radius: UiMetrics.radius
                    opacity: parent.enabled ? 1.0 : 0.4
                }
            }
        }

        Text {
            text: aaConnected
                  ? "AA Connected — buttons active"
                  : (AAOrchestrator !== null
                     ? "AA not connected — buttons disabled"
                     : "AA orchestrator unavailable")
            font.pixelSize: UiMetrics.fontCaption
            color: aaConnected ? ThemeService.primary : ThemeService.textSecondary
            Layout.fillWidth: true
        }

    }

    ExitDialog {
        id: exitDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
    }
}
