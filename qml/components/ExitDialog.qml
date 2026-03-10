import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Controls

Popup {
    id: exitDialog
    anchors.centerIn: parent
    width: Math.min(parent.width * 0.5, 400)
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: UiMetrics.gap

    background: Item {
        // Surface tint: blend 12% primary into surfaceContainerHigh for visible elevation
        Rectangle {
            id: dialogBg
            anchors.fill: parent
            radius: UiMetrics.gap
            color: Qt.rgba(
                ThemeService.surfaceContainerHigh.r * 0.88 + ThemeService.primary.r * 0.12,
                ThemeService.surfaceContainerHigh.g * 0.88 + ThemeService.primary.g * 0.12,
                ThemeService.surfaceContainerHigh.b * 0.88 + ThemeService.primary.b * 0.12,
                1.0)
            border.width: 1
            border.color: ThemeService.outlineVariant
            layer.enabled: true
            visible: false
        }

        MultiEffect {
            source: dialogBg
            anchors.fill: dialogBg
            shadowEnabled: true
            shadowColor: ThemeService.shadow
            shadowBlur: 0.85
            shadowVerticalOffset: 8
            shadowOpacity: 0.60
            shadowHorizontalOffset: 0
            shadowScale: 1.0
            autoPaddingEnabled: true
        }
    }

    contentItem: ColumnLayout {
        spacing: UiMetrics.spacing

        Text {
            Layout.fillWidth: true
            text: "Close OpenAuto Prodigy?"
            font.pixelSize: UiMetrics.fontTitle
            font.bold: true
            color: ThemeService.onSurface
            horizontalAlignment: Text.AlignHCenter
            Layout.bottomMargin: UiMetrics.spacing
        }

        ElevatedButton {
            Layout.fillWidth: true
            Layout.preferredHeight: UiMetrics.touchMin
            text: "Minimize"
            iconCode: "\ue5cd"
            onClicked: {
                exitDialog.close()
                ApplicationController.minimize()
            }
        }

        FilledButton {
            Layout.fillWidth: true
            Layout.preferredHeight: UiMetrics.touchMin
            text: "Close App"
            iconCode: "\ue5cd"
            buttonColor: ThemeService.error
            pressedColor: Qt.darker(ThemeService.error, 1.15)
            textColor: ThemeService.onError
            pressedTextColor: ThemeService.onError
            onClicked: ApplicationController.quit()
        }

        OutlinedButton {
            Layout.fillWidth: true
            Layout.preferredHeight: UiMetrics.touchMin
            text: "Cancel"
            onClicked: exitDialog.close()
        }
    }
}
