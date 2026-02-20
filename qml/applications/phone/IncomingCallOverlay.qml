import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/// Incoming call overlay — shown on top of any active view.
/// Shell.qml should instantiate this and show it when PhonePlugin.callState === 2 (Ringing).
Rectangle {
    id: overlay
    anchors.fill: parent
    color: "#CC000000"  // semi-transparent black
    visible: PhonePlugin && PhonePlugin.callState === 2
    z: 1000  // above everything

    MouseArea {
        anchors.fill: parent
        // Block clicks from reaching underneath
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 24
        width: parent.width * 0.6

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Incoming Call"
            font.pixelSize: 18
            color: "#AAAAAA"
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: PhonePlugin ? (PhonePlugin.callerName || PhonePlugin.callerNumber || "Unknown") : ""
            font.pixelSize: 28
            font.bold: true
            color: "white"
            elide: Text.ElideRight
            Layout.maximumWidth: parent.width
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: PhonePlugin ? PhonePlugin.callerNumber : ""
            font.pixelSize: 16
            color: "#CCCCCC"
            visible: PhonePlugin && PhonePlugin.callerName.length > 0 && PhonePlugin.callerNumber.length > 0
        }

        Item { height: 16 }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 48

            // Reject
            Button {
                Layout.preferredWidth: 72
                Layout.preferredHeight: 72
                onClicked: if (PhonePlugin) PhonePlugin.hangup()
                contentItem: MaterialIcon {
                    icon: "\uf0bc"  // call_end
                    size: 32
                    color: "white"
                }
                background: Rectangle {
                    color: parent.pressed ? "#D32F2F" : "#F44336"
                    radius: width / 2
                }
            }

            // Accept
            Button {
                Layout.preferredWidth: 72
                Layout.preferredHeight: 72
                onClicked: if (PhonePlugin) PhonePlugin.answer()
                contentItem: MaterialIcon {
                    icon: "\uf0d4"  // phone
                    size: 32
                    color: "white"
                }
                background: Rectangle {
                    color: parent.pressed ? "#388E3C" : "#4CAF50"
                    radius: width / 2
                }
            }
        }
    }
}
