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
                color: ThemeService.textSecondary
                opacity: 0.7
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                width: UiMetrics.statusDot
                height: UiMetrics.statusDot
                radius: UiMetrics.statusDot / 2
                color: isConnected ? ThemeService.success : ThemeService.red
            }

            Text {
                text: isConnected ? "Connected" : "No phone"
                font.pixelSize: UiMetrics.fontTiny
                color: ThemeService.textSecondary
            }
        }

        // Dialed number / call info display
        Rectangle {
            Layout.fillWidth: true
            height: Math.round(60 * UiMetrics.scale)
            color: ThemeService.surfaceVariant
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
                color: ThemeService.textPrimary
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

                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: modelData
                    font.pixelSize: UiMetrics.fontTitle
                    enabled: isConnected
                    onClicked: if (PhonePlugin) PhonePlugin.appendDigit(modelData)

                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: ThemeService.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.pressed
                               ? ThemeService.primary
                               : ThemeService.surfaceVariant
                        radius: UiMetrics.radiusSmall
                        border.color: ThemeService.onSurface
                        border.width: 1
                    }
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
            Button {
                Layout.preferredWidth: UiMetrics.callBtnSize
                Layout.preferredHeight: UiMetrics.callBtnSize
                onClicked: if (PhonePlugin) PhonePlugin.hangup()
                contentItem: MaterialIcon {
                    icon: "\uf0bc"  // call_end
                    size: UiMetrics.iconSize
                    color: ThemeService.onRed
                }
                background: Rectangle {
                    color: parent.pressed ? Qt.darker(ThemeService.red, 1.15) : ThemeService.red
                    radius: width / 2
                }
            }

            // Answer button (only for incoming calls)
            Button {
                Layout.preferredWidth: UiMetrics.callBtnSize
                Layout.preferredHeight: UiMetrics.callBtnSize
                visible: callState === 2  // Ringing
                onClicked: if (PhonePlugin) PhonePlugin.answer()
                contentItem: MaterialIcon {
                    icon: "\uf0d4"  // phone
                    size: UiMetrics.iconSize
                    color: ThemeService.onSuccess
                }
                background: Rectangle {
                    color: parent.pressed ? Qt.darker(ThemeService.success, 1.2) : ThemeService.success
                    radius: width / 2
                }
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
            Button {
                Layout.preferredWidth: Math.max(Math.round(64 * UiMetrics.scale), UiMetrics.touchMin)
                Layout.fillHeight: true
                enabled: isConnected && PhonePlugin && PhonePlugin.dialedNumber.length > 0
                onClicked: if (PhonePlugin) PhonePlugin.clearDialed()
                contentItem: MaterialIcon {
                    icon: "\ue14a"  // backspace
                    size: Math.round(22 * UiMetrics.scale)
                    color: ThemeService.textPrimary
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.primary : ThemeService.surfaceVariant
                    radius: UiMetrics.radiusSmall
                }
            }

            // Dial button
            Button {
                Layout.fillWidth: true
                Layout.fillHeight: true
                enabled: isConnected && PhonePlugin && PhonePlugin.dialedNumber.length > 0
                onClicked: if (PhonePlugin) PhonePlugin.dial(PhonePlugin.dialedNumber)
                contentItem: RowLayout {
                    spacing: UiMetrics.spacing
                    Item { Layout.fillWidth: true }
                    MaterialIcon {
                        icon: "\uf0d4"  // phone
                        size: Math.round(22 * UiMetrics.scale)
                        color: ThemeService.onSuccess
                    }
                    Text {
                        text: "Call"
                        font.pixelSize: UiMetrics.fontBody
                        font.bold: true
                        color: ThemeService.onSuccess
                    }
                    Item { Layout.fillWidth: true }
                }
                background: Rectangle {
                    color: parent.enabled
                           ? (parent.pressed ? Qt.darker(ThemeService.success, 1.2) : ThemeService.success)
                           : ThemeService.textSecondary
                    radius: UiMetrics.radiusSmall
                }
            }
        }
    }

    function formatDuration(seconds) {
        var min = Math.floor(seconds / 60)
        var sec = seconds % 60
        return min + ":" + (sec < 10 ? "0" : "") + sec
    }
}
