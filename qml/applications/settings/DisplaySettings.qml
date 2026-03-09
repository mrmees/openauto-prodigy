import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    property real _currentScale: {
        var v = ConfigService.value("ui.scale")
        if (v !== undefined && v !== null && Number(v) > 0) return Number(v)
        return 1.0
    }
    Connections {
        target: ConfigService
        function onConfigChanged(path, value) {
            if (path === "ui.scale") {
                root._currentScale = (value !== undefined && value !== null && Number(value) > 0) ? Number(value) : 1.0
            }
        }
    }

    function _applyScale(newVal) {
        ConfigService.setValue("ui.scale", newVal)
        ConfigService.save()
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        ReadOnlyField {
            label: "Screen"
            value: {
                var size = DisplayInfo ? DisplayInfo.screenSizeInches : 7.0;
                var dpi = DisplayInfo ? DisplayInfo.computedDpi : 170;
                return size.toFixed(1) + "\" / " + dpi + " PPI";
            }
        }

        // Scale stepper: [-] value [+] (reset)
        Item {
            Layout.fillWidth: true
            implicitHeight: UiMetrics.rowH

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginRow
                anchors.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.gap

                Text {
                    text: "UI Scale"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                }

                // Reset button (left of controls so [-] [+] don't shift)
                Rectangle {
                    width: UiMetrics.touchMin
                    height: UiMetrics.touchMin
                    radius: UiMetrics.touchMin / 2
                    color: "transparent"
                    border.color: Math.abs(root._currentScale - 1.0) > 0.05 ? ThemeService.onSurfaceVariant : "transparent"
                    border.width: 1
                    opacity: Math.abs(root._currentScale - 1.0) > 0.05 ? 1.0 : 0.0

                    MaterialIcon {
                        anchors.centerIn: parent
                        icon: "\ue042"
                        size: UiMetrics.iconSmall
                        color: ThemeService.onSurfaceVariant
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: Math.abs(root._currentScale - 1.0) > 0.05
                        onClicked: root._applyScale(1.0)
                    }
                }

                // [-] button
                Rectangle {
                    width: UiMetrics.touchMin
                    height: UiMetrics.touchMin
                    radius: UiMetrics.radius
                    color: root._currentScale <= 0.5 ? ThemeService.surfaceContainerLow : ThemeService.primary
                    opacity: root._currentScale <= 0.5 ? 0.3 : 1.0

                    Text {
                        anchors.centerIn: parent
                        text: "\u2212"
                        font.pixelSize: UiMetrics.fontHeading
                        font.weight: Font.Bold
                        color: "white"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (root._currentScale <= 0.5) return
                            var newVal = Math.round((root._currentScale - 0.1) * 10) / 10
                            root._applyScale(newVal)
                        }
                    }
                }

                // Value display
                Text {
                    text: root._currentScale.toFixed(1)
                    font.pixelSize: UiMetrics.fontTitle
                    font.weight: Font.DemiBold
                    color: ThemeService.onSurface
                    horizontalAlignment: Text.AlignHCenter
                    Layout.preferredWidth: UiMetrics.touchMin
                }

                // [+] button
                Rectangle {
                    width: UiMetrics.touchMin
                    height: UiMetrics.touchMin
                    radius: UiMetrics.radius
                    color: root._currentScale >= 2.0 ? ThemeService.surfaceContainerLow : ThemeService.primary
                    opacity: root._currentScale >= 2.0 ? 0.3 : 1.0

                    Text {
                        anchors.centerIn: parent
                        text: "+"
                        font.pixelSize: UiMetrics.fontHeading
                        font.weight: Font.Bold
                        color: "white"
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (root._currentScale >= 2.0) return
                            var newVal = Math.round((root._currentScale + 0.1) * 10) / 10
                            root._applyScale(newVal)
                        }
                    }
                }
            }
        }

        SectionHeader { text: "General" }

        SettingsSlider {
            id: brightnessSlider
            label: typeof DisplayService !== "undefined" && DisplayService.hasHardwareBrightness ? "Brightness" : "Screen Dimming"
            configPath: "display.brightness"
            from: 5; to: 100; stepSize: 1
            onMoved: {
                if (typeof DisplayService !== "undefined")
                    DisplayService.setBrightness(Math.round(value))
            }
        }

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

        SectionHeader { text: "Clock" }

        SettingsToggle {
            label: "24-Hour Format"
            configPath: "display.clock_24h"
        }

        SectionHeader { text: "Navbar" }

        FullScreenPicker {
            label: "Navbar Position"
            configPath: "navbar.edge"
            options: ["Bottom", "Top", "Left", "Right"]
            values: ["bottom", "top", "left", "right"]
            onActivated: function(index) {
                var edges = ["bottom", "top", "left", "right"]
                NavbarController.setEdge(edges[index])
            }
        }

        SettingsToggle {
            label: "Show Navbar during Android Auto"
            configPath: "navbar.show_during_aa"
            restartRequired: true
        }

        SectionHeader { text: "Day / Night Mode" }

        FullScreenPicker {
            id: nightSource
            label: "Source"
            configPath: "sensors.night_mode.source"
            options: ["time", "gpio", "none"]
        }

        ReadOnlyField {
            label: "Day starts at"
            configPath: "sensors.night_mode.day_start"
            placeholder: "HH:MM"
            visible: nightSource.currentValue === "time"
        }

        ReadOnlyField {
            label: "Night starts at"
            configPath: "sensors.night_mode.night_start"
            placeholder: "HH:MM"
            visible: nightSource.currentValue === "time"
        }

        SettingsSlider {
            label: "GPIO Pin"
            configPath: "sensors.night_mode.gpio_pin"
            from: 0; to: 40; stepSize: 1
            visible: nightSource.currentValue === "gpio"
        }

        SettingsToggle {
            label: "GPIO Active High"
            configPath: "sensors.night_mode.gpio_active_high"
            visible: nightSource.currentValue === "gpio"
        }
    }

}
