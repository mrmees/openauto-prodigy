import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property StackView stackRef: StackView.view

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 12

        SettingsPageHeader {
            title: "About"
            stack: root.stackRef
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 16
            spacing: 8

            Text {
                text: "OpenAuto Prodigy"
                font.pixelSize: 22
                font.bold: true
                color: ThemeService.normalFontColor
            }

            Text {
                text: "Version " + (ConfigService.value("identity.sw_version") || "0.0.0")
                font.pixelSize: 15
                color: ThemeService.descriptionFontColor
            }

            Text {
                text: "Open-source Android Auto head unit"
                font.pixelSize: 13
                color: ThemeService.descriptionFontColor
            }

            Text {
                text: "License: GPLv3"
                font.pixelSize: 13
                color: ThemeService.descriptionFontColor
            }
        }

        Item { Layout.fillWidth: true; Layout.preferredHeight: 24 }

        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 200
            Layout.preferredHeight: 48
            onClicked: exitDialog.open()
            contentItem: RowLayout {
                spacing: 8
                Item { Layout.fillWidth: true }
                MaterialIcon {
                    icon: "\ue5cd"
                    size: 20
                    color: ThemeService.normalFontColor
                }
                Text {
                    text: "Exit App"
                    font.pixelSize: 16
                    color: ThemeService.normalFontColor
                }
                Item { Layout.fillWidth: true }
            }
            background: Rectangle {
                color: parent.pressed ? ThemeService.highlightColor : ThemeService.barBackgroundColor
                radius: 8
            }
        }
    }

    Dialog {
        id: exitDialog
        anchors.centerIn: parent
        width: 320
        modal: true
        title: "Exit"

        background: Rectangle {
            color: ThemeService.controlBackgroundColor
            radius: 12
            border.color: ThemeService.controlForegroundColor
            border.width: 1
        }

        header: Item {
            height: 48
            Text {
                anchors.centerIn: parent
                text: "Exit OpenAuto Prodigy?"
                font.pixelSize: 18
                font.bold: true
                color: ThemeService.normalFontColor
            }
        }

        contentItem: ColumnLayout {
            spacing: 12

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                onClicked: {
                    exitDialog.close()
                    ApplicationController.minimize()
                }
                contentItem: RowLayout {
                    spacing: 10
                    Item { Layout.fillWidth: true }
                    MaterialIcon {
                        icon: "\ue5cd"
                        size: 20
                        color: ThemeService.normalFontColor
                    }
                    Text {
                        text: "Minimize"
                        font.pixelSize: 16
                        color: ThemeService.normalFontColor
                    }
                    Item { Layout.fillWidth: true }
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.highlightColor : ThemeService.barBackgroundColor
                    radius: 8
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                onClicked: ApplicationController.quit()
                contentItem: RowLayout {
                    spacing: 10
                    Item { Layout.fillWidth: true }
                    MaterialIcon {
                        icon: "\ue5cd"
                        size: 20
                        color: "#F44336"
                    }
                    Text {
                        text: "Close App"
                        font.pixelSize: 16
                        color: "#F44336"
                    }
                    Item { Layout.fillWidth: true }
                }
                background: Rectangle {
                    color: parent.pressed ? "#F44336" : ThemeService.barBackgroundColor
                    radius: 8
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                onClicked: exitDialog.close()
                contentItem: Text {
                    text: "Cancel"
                    font.pixelSize: 14
                    color: ThemeService.descriptionFontColor
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    color: "transparent"
                    radius: 8
                }
            }
        }

        footer: Item { height: 0 }
    }
}
