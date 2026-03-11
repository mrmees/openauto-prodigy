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
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        FullScreenPicker {
            id: themePicker
            label: "Theme"
            configPath: "display.theme"
            options: ThemeService.availableThemeNames
            values: ThemeService.availableThemes
            onActivated: function(index) {
                ThemeService.setTheme(ThemeService.availableThemes[index])
            }
        }

        // Delete theme row -- only visible for user/companion themes
        Item {
            id: deleteThemeRow
            Layout.fillWidth: true
            implicitHeight: UiMetrics.rowH
            visible: themePicker.currentIndex >= 0
                     && themePicker.currentIndex < ThemeService.availableThemes.length
                     && ThemeService.isUserTheme(ThemeService.availableThemes[themePicker.currentIndex])

            property bool confirmPending: false

            // Reset confirmation state when theme selection changes
            Connections {
                target: themePicker
                function onCurrentIndexChanged() {
                    deleteThemeRow.confirmPending = false
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginRow
                anchors.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.gap

                MaterialIcon {
                    icon: "\ue872"  // delete
                    size: UiMetrics.iconSmall
                    color: parent.parent.confirmPending ? ThemeService.error : ThemeService.onSurfaceVariant
                }

                Text {
                    text: parent.parent.confirmPending ? "Tap again to delete" : "Delete Theme"
                    font.pixelSize: UiMetrics.fontBody
                    color: parent.parent.confirmPending ? ThemeService.error : ThemeService.onSurface
                    Layout.fillWidth: true
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (parent.confirmPending) {
                        var themeId = ThemeService.availableThemes[themePicker.currentIndex]
                        ThemeService.deleteTheme(themeId)
                        parent.confirmPending = false
                    } else {
                        parent.confirmPending = true
                    }
                }
            }

            // Auto-reset confirmation after 3 seconds
            Timer {
                running: parent.confirmPending
                interval: 3000
                onTriggered: parent.confirmPending = false
            }
        }

        FullScreenPicker {
            label: "Wallpaper"
            configPath: "display.wallpaper_override"
            options: ThemeService.availableWallpaperNames
            values: ThemeService.availableWallpapers
            onActivated: function(index) {
                ThemeService.setWallpaperOverride(ThemeService.availableWallpapers[index])
            }
            Component.onCompleted: ThemeService.refreshWallpapers()
        }

        SettingsToggle {
            id: forceDarkToggle
            label: "Always Use Dark Mode"
            configPath: "display.force_dark_mode"
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
