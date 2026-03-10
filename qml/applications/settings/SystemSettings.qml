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
                color: ThemeService.onSurface
            }
            Text {
                text: "Version " + (ConfigService.value("identity.sw_version") || "0.0.0")
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.onSurfaceVariant
            }
        }

        Item { Layout.fillWidth: true; Layout.preferredHeight: UiMetrics.marginPage + UiMetrics.marginRow }

        ElevatedButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: parent.width * 0.4
            Layout.preferredHeight: UiMetrics.rowH
            text: "Close App"
            iconCode: "\ue5cd"
            onClicked: exitDialog.open()
        }

        SectionHeader { text: "Debug: AA Protocol" }

        Text {
            text: "Test outbound commands (requires active AA connection)"
            font.pixelSize: UiMetrics.fontCaption
            color: ThemeService.onSurfaceVariant
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }

        property bool aaConnected: AAOrchestrator !== null && AAOrchestrator.connectionState === 3

        RowLayout {
            Layout.fillWidth: true
            spacing: UiMetrics.spacing

            ElevatedButton {
                Layout.fillWidth: true
                Layout.preferredHeight: UiMetrics.rowH
                text: "Play/Pause"
                buttonEnabled: aaConnected
                onClicked: AAOrchestrator.sendButtonPress(85)
            }

            ElevatedButton {
                Layout.fillWidth: true
                Layout.preferredHeight: UiMetrics.rowH
                text: "Prev"
                buttonEnabled: aaConnected
                onClicked: AAOrchestrator.sendButtonPress(88)
            }

            ElevatedButton {
                Layout.fillWidth: true
                Layout.preferredHeight: UiMetrics.rowH
                text: "Next"
                buttonEnabled: aaConnected
                onClicked: AAOrchestrator.sendButtonPress(87)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: UiMetrics.spacing

            ElevatedButton {
                Layout.fillWidth: true
                Layout.preferredHeight: UiMetrics.rowH
                text: "Search (84)"
                buttonEnabled: aaConnected
                onClicked: AAOrchestrator.sendButtonPress(84)
            }

            ElevatedButton {
                Layout.fillWidth: true
                Layout.preferredHeight: UiMetrics.rowH
                text: "Assist (219)"
                buttonEnabled: aaConnected
                onClicked: AAOrchestrator.sendButtonPress(219)
            }

            ElevatedButton {
                Layout.fillWidth: true
                Layout.preferredHeight: UiMetrics.rowH
                text: "Voice (231)"
                buttonEnabled: aaConnected
                onClicked: AAOrchestrator.sendButtonPress(231)
            }
        }

        Text {
            text: aaConnected
                  ? "AA Connected -- buttons active"
                  : (AAOrchestrator !== null
                     ? "AA not connected -- buttons disabled"
                     : "AA orchestrator unavailable")
            font.pixelSize: UiMetrics.fontCaption
            color: aaConnected ? ThemeService.primary : ThemeService.onSurfaceVariant
            Layout.fillWidth: true
        }

    }

    ExitDialog {
        id: exitDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
    }
}
