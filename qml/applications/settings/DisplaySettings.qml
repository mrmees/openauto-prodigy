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

        ReadOnlyField {
            label: "Screen"
            value: {
                var size = DisplayInfo ? DisplayInfo.screenSizeInches : 7.0;
                var dpi = DisplayInfo ? DisplayInfo.computedDpi : 170;
                return size.toFixed(1) + "\" / " + dpi + " PPI";
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
            label: "Theme"
            configPath: "display.theme"
            options: ThemeService.availableThemeNames
            values: ThemeService.availableThemes
            onActivated: function(index) {
                ThemeService.setTheme(ThemeService.availableThemes[index])
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
