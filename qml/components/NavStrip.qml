import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: navStrip
    color: ThemeService.barBackgroundColor
    Behavior on color { ColorAnimation { duration: 300; easing.type: Easing.InOutQuad } }

    signal settingsResetRequested()
    signal eqRequested()

    // Home is active when no plugin and on launcher screen
    readonly property bool homeActive: !PluginModel.activePluginId
                                       && ApplicationController.currentApplication === 0

    // Fixed buttons (home, EQ, day/night, settings) + dynamic plugins
    readonly property int _buttonCount: 4 + PluginModel.rowCount()
    readonly property real _maxButtonW: (width - 2 * UiMetrics.marginRow) / _buttonCount

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginRow
        anchors.rightMargin: UiMetrics.marginRow
        spacing: 0

        // Home button
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: Math.min(navStrip.height * 1.2, navStrip._maxButtonW)
            color: navStrip.homeActive ? ThemeService.highlightColor : "transparent"
            radius: UiMetrics.radiusSmall

            scale: homeArea.pressed ? 0.95 : 1.0
            opacity: homeArea.pressed ? 0.85 : 1.0
            Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue9b2"  // home
                size: parent.height * 0.5
                color: navStrip.homeActive ? ThemeService.highlightFontColor
                                          : ThemeService.normalFontColor
            }

            MouseArea {
                id: homeArea
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
                Layout.preferredWidth: Math.min(navStrip.height * 1.2, navStrip._maxButtonW)
                color: isActive ? ThemeService.highlightColor : "transparent"
                radius: UiMetrics.radiusSmall

                scale: pluginArea.pressed ? 0.95 : 1.0
                opacity: pluginArea.pressed ? 0.85 : 1.0
                Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }
                Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

                MaterialIcon {
                    anchors.centerIn: parent
                    icon: pluginIconText || "\ue5c3"
                    size: parent.height * 0.5
                    color: isActive ? ThemeService.highlightFontColor
                                    : ThemeService.normalFontColor
                }

                MouseArea {
                    id: pluginArea
                    anchors.fill: parent
                    onClicked: {
                        PluginModel.setActivePlugin(pluginId)
                        ApplicationController.navigateTo(0)
                    }
                }
            }
        }

        // EQ shortcut
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: Math.min(navStrip.height * 1.2, navStrip._maxButtonW)
            color: "transparent"
            radius: UiMetrics.radiusSmall

            scale: eqArea.pressed ? 0.95 : 1.0
            opacity: eqArea.pressed ? 0.85 : 1.0
            Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue429"  // equalizer
                size: parent.height * 0.5
                color: ThemeService.normalFontColor
            }

            MouseArea {
                id: eqArea
                anchors.fill: parent
                onClicked: navStrip.eqRequested()
            }
        }

        // Spacer — collapses on narrow screens so buttons don't get clipped
        Item {
            Layout.fillWidth: navStrip._maxButtonW >= navStrip.height * 0.8
            Layout.preferredWidth: navStrip._maxButtonW >= navStrip.height * 0.8 ? -1 : 0
        }

        // Back button (visible when not on launcher)
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: Math.min(navStrip.height * 1.2, navStrip._maxButtonW)
            color: "transparent"
            radius: UiMetrics.radiusSmall
            visible: ApplicationController.currentApplication !== 0

            scale: backArea.pressed ? 0.95 : 1.0
            opacity: backArea.pressed ? 0.85 : 1.0
            Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue5c4"  // arrow_back
                size: parent.height * 0.5
                color: ThemeService.normalFontColor
            }

            MouseArea {
                id: backArea
                anchors.fill: parent
                onClicked: ApplicationController.requestBack()
            }
        }

        // Day/night mode toggle
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: Math.min(navStrip.height * 1.2, navStrip._maxButtonW)
            color: "transparent"
            radius: UiMetrics.radiusSmall

            scale: modeArea.pressed ? 0.95 : 1.0
            opacity: modeArea.pressed ? 0.85 : 1.0
            Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

            MaterialIcon {
                anchors.centerIn: parent
                icon: ThemeService.nightMode ? "\ue518" : "\ue430"  // dark_mode / light_mode
                size: parent.height * 0.5
                color: ThemeService.normalFontColor
            }

            MouseArea {
                id: modeArea
                anchors.fill: parent
                onClicked: ThemeService.toggleMode()
            }
        }

        // Settings button
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: Math.min(navStrip.height * 1.2, navStrip._maxButtonW)
            color: ApplicationController.currentApplication === 6 ? ThemeService.highlightColor : "transparent"
            radius: UiMetrics.radiusSmall

            scale: settingsArea.pressed ? 0.95 : 1.0
            opacity: settingsArea.pressed ? 0.85 : 1.0
            Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue8b8"  // settings
                size: parent.height * 0.5
                color: ApplicationController.currentApplication === 6
                       ? ThemeService.highlightFontColor
                       : ThemeService.normalFontColor
            }

            MouseArea {
                id: settingsArea
                anchors.fill: parent
                onClicked: {
                    PluginModel.setActivePlugin("")
                    if (ApplicationController.currentApplication === 6) {
                        // Already in settings — reset stack to tile grid
                        settingsResetRequested()
                    } else {
                        ApplicationController.navigateTo(6)
                    }
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
