import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/// Incoming call overlay -- shown on top of any active view.
/// Shell.qml should instantiate this and show it when CallStateProvider.callState === 2 (Ringing).
Rectangle {
    id: overlay
    anchors.fill: parent
    color: ThemeService.scrim
    visible: CallStateProvider && CallStateProvider.callState === 2
    z: 1000  // above everything

    MouseArea {
        anchors.fill: parent
        // Block clicks from reaching underneath
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: UiMetrics.sectionGap
        width: parent.width * 0.6

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Incoming Call"
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.onSurfaceVariant
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: CallStateProvider ? (CallStateProvider.callerName || CallStateProvider.callerNumber || "Unknown") : ""
            font.pixelSize: UiMetrics.fontHeading
            font.bold: true
            color: ThemeService.onSurface
            elide: Text.ElideRight
            Layout.maximumWidth: parent.width
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: CallStateProvider ? CallStateProvider.callerNumber : ""
            font.pixelSize: UiMetrics.fontSmall
            color: ThemeService.onSurfaceVariant
            visible: CallStateProvider && CallStateProvider.callerName.length > 0 && CallStateProvider.callerNumber.length > 0
        }

        Item { height: UiMetrics.gap }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Math.round(48 * UiMetrics.scale)

            // Reject
            FilledButton {
                implicitWidth: UiMetrics.callBtnSize
                implicitHeight: UiMetrics.callBtnSize
                iconCode: "\uf0bc"  // call_end
                buttonColor: ThemeService.error
                pressedColor: Qt.darker(ThemeService.error, 1.15)
                textColor: ThemeService.onError
                pressedTextColor: ThemeService.onError
                buttonRadius: UiMetrics.callBtnSize / 2
                onClicked: if (CallStateProvider) CallStateProvider.hangup()
            }

            // Accept
            FilledButton {
                implicitWidth: UiMetrics.callBtnSize
                implicitHeight: UiMetrics.callBtnSize
                iconCode: "\uf0d4"  // phone
                buttonColor: ThemeService.success
                pressedColor: Qt.darker(ThemeService.success, 1.2)
                textColor: ThemeService.onSuccess
                pressedTextColor: ThemeService.onSuccess
                buttonRadius: UiMetrics.callBtnSize / 2
                onClicked: if (CallStateProvider) CallStateProvider.answer()
            }
        }
    }
}
