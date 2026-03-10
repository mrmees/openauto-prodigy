import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: phoneView
    anchors.fill: parent
    color: ThemeService.background

    property bool isConnected: PhonePlugin && PhonePlugin.phoneConnected
    property int callState: PhonePlugin ? PhonePlugin.callState : 0
    property bool inCall: callState >= 1 && callState <= 4  // Dialing through HeldActive

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.gap
        spacing: UiMetrics.marginRow

        // Status bar
        RowLayout {
            Layout.fillWidth: true
            spacing: UiMetrics.spacing

            Text {
                text: PhonePlugin ? (PhonePlugin.deviceName || "Phone") : "Phone"
                font.pixelSize: UiMetrics.fontSmall
                color: ThemeService.onSurfaceVariant
                opacity: 0.7
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                width: UiMetrics.statusDot
                height: UiMetrics.statusDot
                radius: UiMetrics.statusDot / 2
                color: isConnected ? ThemeService.success : ThemeService.error
            }

            Text {
                text: isConnected ? "Connected" : "No phone"
                font.pixelSize: UiMetrics.fontTiny
                color: ThemeService.onSurfaceVariant
            }
        }

        // Dialed number / call info display
        Rectangle {
            Layout.fillWidth: true
            height: Math.round(60 * UiMetrics.scale)
            color: ThemeService.surfaceContainerLow
            radius: UiMetrics.radiusSmall

            Text {
                anchors.centerIn: parent
                text: {
                    if (!PhonePlugin) return ""
                    if (inCall) {
                        var info = PhonePlugin.callerName || PhonePlugin.callerNumber || "Unknown"
                        if (callState === 1) return "Dialing " + info + "..."
                        if (callState === 2) return "Incoming: " + info
                        if (callState === 3) return info + "  " + formatDuration(PhonePlugin.callDuration)
                        if (callState === 5) return "Call ended"
                        return info
                    }
                    return PhonePlugin.dialedNumber || ""
                }
                font.pixelSize: inCall ? UiMetrics.fontBody : UiMetrics.fontHeading
                font.bold: true
                color: ThemeService.onSurface
                elide: Text.ElideRight
                width: parent.width - UiMetrics.sectionGap * 2
                horizontalAlignment: Text.AlignHCenter
            }
        }

        // Number pad (hidden during active call)
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 3
            rowSpacing: UiMetrics.spacing
            columnSpacing: UiMetrics.spacing
            visible: !inCall
            opacity: isConnected ? 1.0 : 0.4

            Repeater {
                model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"]

                ElevatedButton {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: modelData
                    buttonEnabled: isConnected
                    elevation: 1
                    onClicked: if (PhonePlugin) PhonePlugin.appendDigit(modelData)
                }
            }
        }

        // In-call controls
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: UiMetrics.gap
            visible: inCall

            Item { Layout.fillWidth: true }

            // Hangup button (always visible during call)
            FilledButton {
                implicitWidth: UiMetrics.callBtnSize
                implicitHeight: UiMetrics.callBtnSize
                iconCode: "\uf0bc"  // call_end
                buttonColor: ThemeService.error
                pressedColor: Qt.darker(ThemeService.error, 1.15)
                textColor: ThemeService.onError
                pressedTextColor: ThemeService.onError
                buttonRadius: UiMetrics.callBtnSize / 2
                onClicked: if (PhonePlugin) PhonePlugin.hangup()
            }

            // Answer button (only for incoming calls)
            FilledButton {
                implicitWidth: UiMetrics.callBtnSize
                implicitHeight: UiMetrics.callBtnSize
                visible: callState === 2  // Ringing
                iconCode: "\uf0d4"  // phone
                buttonColor: ThemeService.success
                pressedColor: Qt.darker(ThemeService.success, 1.2)
                textColor: ThemeService.onSuccess
                pressedTextColor: ThemeService.onSuccess
                buttonRadius: UiMetrics.callBtnSize / 2
                onClicked: if (PhonePlugin) PhonePlugin.answer()
            }

            Item { Layout.fillWidth: true }
        }

        // Bottom action row (dial / backspace)
        RowLayout {
            Layout.fillWidth: true
            height: UiMetrics.touchMin
            spacing: UiMetrics.spacing
            visible: !inCall

            // Backspace
            ElevatedButton {
                Layout.preferredWidth: Math.max(Math.round(64 * UiMetrics.scale), UiMetrics.touchMin)
                Layout.fillHeight: true
                iconCode: "\ue14a"  // backspace
                buttonEnabled: isConnected && PhonePlugin && PhonePlugin.dialedNumber.length > 0
                onClicked: if (PhonePlugin) PhonePlugin.clearDialed()
            }

            // Dial button
            FilledButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: "Call"
                iconCode: "\uf0d4"  // phone
                buttonColor: isConnected && PhonePlugin && PhonePlugin.dialedNumber.length > 0
                    ? ThemeService.success : ThemeService.onSurfaceVariant
                pressedColor: Qt.darker(ThemeService.success, 1.2)
                textColor: ThemeService.onSuccess
                pressedTextColor: ThemeService.onSuccess
                buttonEnabled: isConnected && PhonePlugin && PhonePlugin.dialedNumber.length > 0
                onClicked: if (PhonePlugin) PhonePlugin.dial(PhonePlugin.dialedNumber)
            }
        }
    }

    function formatDuration(seconds) {
        var min = Math.floor(seconds / 60)
        var sec = seconds % 60
        return min + ":" + (sec < 10 ? "0" : "") + sec
    }
}
