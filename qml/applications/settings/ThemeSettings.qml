import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    BackHoldArea {}
    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
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
            interactive: true
            visible: themePicker.currentIndex >= 0
                     && themePicker.currentIndex < ThemeService.availableThemes.length
                     && ThemeService.isUserTheme(ThemeService.availableThemes[themePicker.currentIndex])
            onClicked: {
                if (deleteThemeState.confirmPending) {
                    var themeId = ThemeService.availableThemes[themePicker.currentIndex]
                    ThemeService.deleteTheme(themeId)
                    deleteThemeState.confirmPending = false
                } else {
                    deleteThemeState.confirmPending = true
                }
            }

            QtObject {
                id: deleteThemeState
                property bool confirmPending: false
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
                spacing: UiMetrics.gap

                MaterialIcon {
                    icon: "\ue872"
                    size: UiMetrics.iconSmall
                    color: deleteThemeState.confirmPending ? ThemeService.error : ThemeService.onSurfaceVariant
                }

                Text {
                    text: deleteThemeState.confirmPending ? "Tap again to delete" : "Delete Theme"
                    font.pixelSize: UiMetrics.fontBody
                    color: deleteThemeState.confirmPending ? ThemeService.error : ThemeService.onSurface
                    Layout.fillWidth: true
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
}
