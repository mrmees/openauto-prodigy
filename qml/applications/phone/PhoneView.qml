import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: phoneView
    anchors.fill: parent
    color: ThemeService.backgroundColor

    property bool isConnected: PhonePlugin && PhonePlugin.phoneConnected
    property int callState: PhonePlugin ? PhonePlugin.callState : 0
    property bool inCall: callState >= 1 && callState <= 4  // Dialing through HeldActive

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Status bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: PhonePlugin ? (PhonePlugin.deviceName || "Phone") : "Phone"
                font.pixelSize: 14
                color: ThemeService.secondaryTextColor
                opacity: 0.7
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: isConnected ? "#4CAF50" : "#F44336"
            }

            Text {
                text: isConnected ? "Connected" : "No phone"
                font.pixelSize: 12
                color: ThemeService.secondaryTextColor
            }
        }

        // Dialed number / call info display
        Rectangle {
            Layout.fillWidth: true
            height: 60
            color: ThemeService.controlBackgroundColor
            radius: 8

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
                font.pixelSize: inCall ? 18 : 28
                font.bold: true
                color: ThemeService.primaryTextColor
                elide: Text.ElideRight
                width: parent.width - 32
                horizontalAlignment: Text.AlignHCenter
            }
        }

        // Number pad (hidden during active call)
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 3
            rowSpacing: 8
            columnSpacing: 8
            visible: !inCall
            opacity: isConnected ? 1.0 : 0.4

            Repeater {
                model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"]

                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: modelData
                    font.pixelSize: 24
                    enabled: isConnected
                    onClicked: if (PhonePlugin) PhonePlugin.appendDigit(modelData)

                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: ThemeService.primaryTextColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.pressed
                               ? ThemeService.highlightColor
                               : ThemeService.controlBackgroundColor
                        radius: 8
                        border.color: ThemeService.borderColor
                        border.width: 1
                    }
                }
            }
        }

        // In-call controls
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16
            visible: inCall

            Item { Layout.fillWidth: true }

            // Hangup button (always visible during call)
            Button {
                Layout.preferredWidth: 80
                Layout.preferredHeight: 80
                onClicked: if (PhonePlugin) PhonePlugin.hangup()
                contentItem: MaterialIcon {
                    icon: "\uf0bc"  // call_end
                    size: 36
                    color: "white"
                }
                background: Rectangle {
                    color: parent.pressed ? "#D32F2F" : "#F44336"
                    radius: width / 2
                }
            }

            // Answer button (only for incoming calls)
            Button {
                Layout.preferredWidth: 80
                Layout.preferredHeight: 80
                visible: callState === 2  // Ringing
                onClicked: if (PhonePlugin) PhonePlugin.answer()
                contentItem: MaterialIcon {
                    icon: "\uf0d4"  // phone
                    size: 36
                    color: "white"
                }
                background: Rectangle {
                    color: parent.pressed ? "#388E3C" : "#4CAF50"
                    radius: width / 2
                }
            }

            Item { Layout.fillWidth: true }
        }

        // Bottom action row (dial / backspace)
        RowLayout {
            Layout.fillWidth: true
            height: 56
            spacing: 8
            visible: !inCall

            // Backspace
            Button {
                Layout.preferredWidth: 64
                Layout.fillHeight: true
                enabled: isConnected && PhonePlugin && PhonePlugin.dialedNumber.length > 0
                onClicked: if (PhonePlugin) PhonePlugin.clearDialed()
                contentItem: MaterialIcon {
                    icon: "\ue14a"  // backspace
                    size: 22
                    color: ThemeService.primaryTextColor
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.highlightColor : ThemeService.controlBackgroundColor
                    radius: 8
                }
            }

            // Dial button
            Button {
                Layout.fillWidth: true
                Layout.fillHeight: true
                enabled: isConnected && PhonePlugin && PhonePlugin.dialedNumber.length > 0
                onClicked: if (PhonePlugin) PhonePlugin.dial(PhonePlugin.dialedNumber)
                contentItem: RowLayout {
                    spacing: 8
                    Item { Layout.fillWidth: true }
                    MaterialIcon {
                        icon: "\uf0d4"  // phone
                        size: 22
                        color: "white"
                    }
                    Text {
                        text: "Call"
                        font.pixelSize: 20
                        font.bold: true
                        color: "white"
                    }
                    Item { Layout.fillWidth: true }
                }
                background: Rectangle {
                    color: parent.enabled
                           ? (parent.pressed ? "#388E3C" : "#4CAF50")
                           : "#666666"
                    radius: 8
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
