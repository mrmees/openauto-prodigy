import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: UiMetrics.settingsPageInset
        anchors.rightMargin: UiMetrics.settingsPageInset
        anchors.topMargin: UiMetrics.marginPage
        spacing: 0

        SettingsRow { rowIndex: 0
            FullScreenPicker {
                id: themePicker
                flat: true
                label: "Theme"
                configPath: "display.theme"
                options: ThemeService.availableThemeNames
                values: ThemeService.availableThemes
                onActivated: function(index) {
                    ThemeService.setTheme(ThemeService.availableThemes[index])
                }
            }
        }

        // Delete theme row -- only visible for user/companion themes
        SettingsRow {
            id: deleteThemeWrapper
            rowIndex: 1
            visible: themePicker.currentIndex >= 0
                     && themePicker.currentIndex < ThemeService.availableThemes.length
                     && ThemeService.isUserTheme(ThemeService.availableThemes[themePicker.currentIndex])

            QtObject {
                id: deleteThemeState
                property bool confirmPending: false
            }

            function triggerDeleteThemeAction() {
                if (deleteThemeState.confirmPending) {
                    var themeId = ThemeService.availableThemes[themePicker.currentIndex]
                    ThemeService.deleteTheme(themeId)
                    deleteThemeState.confirmPending = false
                } else {
                    deleteThemeState.confirmPending = true
                }
            }

            // Reset confirmation state when theme selection changes
            Connections {
                target: themePicker
                function onCurrentIndexChanged() {
                    deleteThemeState.confirmPending = false
                }
            }

            // Auto-reset confirmation after 3 seconds
            Timer {
                running: deleteThemeState.confirmPending
                interval: 3000
                onTriggered: deleteThemeState.confirmPending = false
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginRow
                anchors.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.gap

                Text {
                    text: "Delete Theme"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                }

                Rectangle {
                    width: deleteThemeButtonText.implicitWidth + UiMetrics.gap * 2
                    height: UiMetrics.touchMin
                    radius: UiMetrics.touchMin / 2
                    color: "transparent"
                    border.color: ThemeService.error
                    border.width: 1

                    Text {
                        id: deleteThemeButtonText
                        anchors.centerIn: parent
                        text: deleteThemeState.confirmPending ? "Confirm" : "Delete"
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.error
                    }

                    SettingsHoldArea {
                        anchors.fill: parent
                        enableBackHold: false
                        onShortClicked: deleteThemeWrapper.triggerDeleteThemeAction()
                    }
                }
            }
        }

        SettingsRow { rowIndex: 2
            FullScreenPicker {
                flat: true
                label: "Wallpaper"
                configPath: "display.wallpaper_override"
                options: ThemeService.availableWallpaperNames
                values: ThemeService.availableWallpapers
                onActivated: function(index) {
                    ThemeService.setWallpaperOverride(ThemeService.availableWallpapers[index])
                }
                Component.onCompleted: ThemeService.refreshWallpapers()
            }
        }

        SettingsRow { rowIndex: 3
            SettingsToggle {
                id: forceDarkToggle
                label: "Always Use Dark Mode"
                configPath: "display.force_dark_mode"
            }
        }

        Connections {
            target: ConfigService
            function onConfigChanged(path, value) {
                if (path === "display.force_dark_mode")
                    ThemeService.forceDarkMode = (value === true || value === 1 || value === "true")
            }
        }
    }

    SettingsScrollHints {
        flickable: root
    }
}
