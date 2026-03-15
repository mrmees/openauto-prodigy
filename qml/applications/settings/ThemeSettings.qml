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

        // Row 0: Theme picker (unchanged)
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

        // Row 1: Custom Wallpaper toggle
        SettingsRow { rowIndex: 1
            id: wpToggleRow

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginRow
                anchors.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.gap

                Text {
                    text: "Custom Wallpaper"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                }

                Switch {
                    id: wpToggle
                    onToggled: {
                        if (!checked) {
                            // Toggle OFF: clear override, revert to theme wallpaper
                            ThemeService.setWallpaperOverride("")
                            ConfigService.setValue("display.wallpaper_override", "")
                            ConfigService.save()
                        }
                        // Toggle ON: just reveals the picker, user picks there
                    }
                }
            }

            SettingsHoldArea {
                anchors.fill: parent
                onShortClicked: wpToggle.checked = !wpToggle.checked
            }
        }

        // Row 2: Wallpaper picker (conditional on toggle)
        SettingsRow { rowIndex: 2
            visible: wpToggle.checked

            FullScreenPicker {
                id: wallpaperPicker
                flat: true
                label: "Wallpaper"
                configPath: "display.wallpaper_override"
                options: {
                    // Filter out "Theme Default" (index 0) since toggle OFF already means theme default
                    var names = ThemeService.availableWallpaperNames
                    return names.length > 0 ? names.slice(1) : names
                }
                values: {
                    var vals = ThemeService.availableWallpapers
                    return vals.length > 0 ? vals.slice(1) : vals
                }
                onActivated: function(index) {
                    // index is into the sliced array, so offset by 1 to get the real value
                    var realValues = ThemeService.availableWallpapers
                    if (realValues.length > 1)
                        ThemeService.setWallpaperOverride(realValues[index + 1])
                }
                Component.onCompleted: ThemeService.refreshWallpapers()
            }
        }

        // Row 3: Always Use Dark Mode toggle (unchanged)
        SettingsRow { rowIndex: 3
            SettingsToggle {
                id: forceDarkToggle
                label: "Always Use Dark Mode"
                configPath: "display.force_dark_mode"
            }
        }

        // Row 4: Delete Theme button (moved to bottom)
        SettingsRow {
            id: deleteThemeWrapper
            rowIndex: 4
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
                        onShortClicked: deleteThemeWrapper.triggerDeleteThemeAction()
                    }
                }
            }
        }

        Connections {
            target: ConfigService
            function onConfigChanged(path, value) {
                if (path === "display.force_dark_mode")
                    ThemeService.forceDarkMode = (value === true || value === 1 || value === "true")
                if (path === "display.wallpaper_override") {
                    var strVal = value !== undefined && value !== null ? String(value) : ""
                    wpToggle.checked = (strVal !== "")
                }
            }
        }

        Component.onCompleted: {
            // Initialize wallpaper toggle from config
            var wpOverride = ConfigService.value("display.wallpaper_override")
            var strVal = wpOverride !== undefined && wpOverride !== null ? String(wpOverride) : ""
            wpToggle.checked = (strVal !== "")
        }
    }

    SettingsScrollHints {
        flickable: root
    }
}
