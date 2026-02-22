import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Popup {
    id: exitDialog
    anchors.centerIn: parent
    width: Math.min(parent.width * 0.5, 400)
    modal: true
    padding: UiMetrics.gap

    readonly property color dangerColor: "#F44336"

    background: Rectangle {
        color: ThemeService.controlBackgroundColor
        radius: UiMetrics.gap
        border.color: ThemeService.controlForegroundColor
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: UiMetrics.spacing

        Text {
            Layout.fillWidth: true
            text: "Close OpenAuto Prodigy?"
            font.pixelSize: UiMetrics.fontTitle
            font.bold: true
            color: ThemeService.normalFontColor
            horizontalAlignment: Text.AlignHCenter
            Layout.bottomMargin: UiMetrics.spacing
        }

        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: UiMetrics.touchMin
            onClicked: {
                exitDialog.close()
                ApplicationController.minimize()
            }
            contentItem: RowLayout {
                spacing: UiMetrics.marginRow
                Item { Layout.fillWidth: true }
                MaterialIcon {
                    icon: "\ue5cd"
                    size: UiMetrics.iconSize
                    color: ThemeService.normalFontColor
                }
                Text {
                    text: "Minimize"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.normalFontColor
                }
                Item { Layout.fillWidth: true }
            }
            background: Rectangle {
                color: parent.pressed ? ThemeService.highlightColor : ThemeService.barBackgroundColor
                radius: UiMetrics.radius
            }
        }

        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: UiMetrics.touchMin
            onClicked: ApplicationController.quit()
            contentItem: RowLayout {
                spacing: UiMetrics.marginRow
                Item { Layout.fillWidth: true }
                MaterialIcon {
                    icon: "\ue5cd"
                    size: UiMetrics.iconSize
                    color: exitDialog.dangerColor
                }
                Text {
                    text: "Close App"
                    font.pixelSize: UiMetrics.fontBody
                    color: exitDialog.dangerColor
                }
                Item { Layout.fillWidth: true }
            }
            background: Rectangle {
                color: parent.pressed ? exitDialog.dangerColor : ThemeService.barBackgroundColor
                radius: UiMetrics.radius
                border.color: exitDialog.dangerColor
                border.width: 1
            }
        }

        Button {
            Layout.fillWidth: true
            Layout.preferredHeight: UiMetrics.touchMin
            onClicked: exitDialog.close()
            contentItem: Text {
                text: "Cancel"
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.normalFontColor
                horizontalAlignment: Text.AlignHCenter
            }
            background: Rectangle {
                color: parent.pressed ? ThemeService.highlightColor : ThemeService.barBackgroundColor
                radius: UiMetrics.radius
            }
        }
    }
}
