import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property StackView stackRef: StackView.view
    readonly property color dangerColor: "#F44336"

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.gap

        SettingsPageHeader {
            title: "About"
            stack: root.stackRef
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: UiMetrics.sectionGap
            spacing: UiMetrics.marginRow

            Text {
                text: "OpenAuto Prodigy"
                font.pixelSize: UiMetrics.fontHeading
                font.bold: true
                color: ThemeService.normalFontColor
            }

            Text {
                text: "Version " + (ConfigService.value("identity.sw_version") || "0.0.0")
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.descriptionFontColor
            }

            Text {
                text: "Open-source Android Auto head unit"
                font.pixelSize: UiMetrics.fontSmall
                color: ThemeService.descriptionFontColor
            }

            Text {
                text: "License: GPLv3"
                font.pixelSize: UiMetrics.fontSmall
                color: ThemeService.descriptionFontColor
            }
        }

        Item { Layout.fillWidth: true; Layout.preferredHeight: UiMetrics.marginPage + UiMetrics.marginRow }

        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: parent.width * 0.3
            Layout.preferredHeight: UiMetrics.rowH
            onClicked: exitDialog.open()
            contentItem: RowLayout {
                spacing: UiMetrics.marginRow
                Item { Layout.fillWidth: true }
                MaterialIcon {
                    icon: "\ue5cd"
                    size: UiMetrics.iconSize
                    color: ThemeService.normalFontColor
                }
                Text {
                    text: "Exit App"
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
    }

    Dialog {
        id: exitDialog
        anchors.centerIn: parent
        width: parent.width * 0.4
        modal: true
        title: "Exit"

        background: Rectangle {
            color: ThemeService.controlBackgroundColor
            radius: UiMetrics.gap
            border.color: ThemeService.controlForegroundColor
            border.width: 1
        }

        header: Item {
            height: UiMetrics.rowH
            Text {
                anchors.centerIn: parent
                text: "Exit OpenAuto Prodigy?"
                font.pixelSize: UiMetrics.fontTitle
                font.bold: true
                color: ThemeService.normalFontColor
            }
        }

        contentItem: ColumnLayout {
            spacing: UiMetrics.gap

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: UiMetrics.rowH
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
                Layout.preferredHeight: UiMetrics.rowH
                onClicked: ApplicationController.quit()
                contentItem: RowLayout {
                    spacing: UiMetrics.marginRow
                    Item { Layout.fillWidth: true }
                    MaterialIcon {
                        icon: "\ue5cd"
                        size: UiMetrics.iconSize
                        color: root.dangerColor
                    }
                    Text {
                        text: "Close App"
                        font.pixelSize: UiMetrics.fontBody
                        color: root.dangerColor
                    }
                    Item { Layout.fillWidth: true }
                }
                background: Rectangle {
                    color: parent.pressed ? root.dangerColor : ThemeService.barBackgroundColor
                    radius: UiMetrics.radius
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: UiMetrics.rowH
                onClicked: exitDialog.close()
                contentItem: Text {
                    text: "Cancel"
                    font.pixelSize: UiMetrics.fontSmall
                    color: ThemeService.descriptionFontColor
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    color: "transparent"
                    radius: UiMetrics.radius
                }
            }
        }

        footer: Item { height: 0 }
    }
}
