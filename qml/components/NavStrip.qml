import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: navStrip
    color: ThemeService.barBackgroundColor

    // Home is active when no plugin and on launcher screen
    readonly property bool homeActive: !PluginModel.activePluginId
                                       && ApplicationController.currentApplication === 0

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        // Home button
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: navStrip.height * 1.2
            color: navStrip.homeActive ? ThemeService.highlightColor : "transparent"
            radius: 8

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue9b2"  // home
                size: parent.height * 0.5
                color: navStrip.homeActive ? ThemeService.highlightFontColor
                                          : ThemeService.normalFontColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    PluginModel.setActivePlugin("")
                    ApplicationController.navigateTo(0)
                }
            }
        }

        // Plugin tiles
        Repeater {
            model: PluginModel

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: navStrip.height * 1.2
                color: isActive ? ThemeService.highlightColor : "transparent"
                radius: 8

                MaterialIcon {
                    anchors.centerIn: parent
                    icon: pluginIconText || "\ue5c3"
                    size: parent.height * 0.5
                    color: isActive ? ThemeService.highlightFontColor
                                    : ThemeService.normalFontColor
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        PluginModel.setActivePlugin(pluginId)
                        ApplicationController.navigateTo(0)
                    }
                }
            }
        }

        // Spacer
        Item { Layout.fillWidth: true }

        // Back button (visible when not on launcher)
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: navStrip.height * 1.2
            color: "transparent"
            radius: 8
            visible: ApplicationController.currentApplication !== 0

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue5c4"  // arrow_back
                size: parent.height * 0.5
                color: ThemeService.normalFontColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: ApplicationController.requestBack()
            }
        }

        // Settings button
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: navStrip.height * 1.2
            color: ApplicationController.currentApplication === 6 ? ThemeService.highlightColor : "transparent"
            radius: 8

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue8b8"  // settings
                size: parent.height * 0.5
                color: ApplicationController.currentApplication === 6
                       ? ThemeService.highlightFontColor
                       : ThemeService.normalFontColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    PluginModel.setActivePlugin("")
                    ApplicationController.navigateTo(6)
                }
                onPressAndHold: exitDialog.open()
            }
        }
    }

    ExitDialog {
        id: exitDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
    }
}
