import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts

/// Translucent overlay shown when a 3-finger tap gesture is detected.
/// Provides quick controls: volume, brightness, home button, dismiss.
/// Auto-dismisses after 15 seconds if no interaction.
Rectangle {
    id: overlay
    anchors.fill: parent
    color: ThemeService.scrim
    visible: false
    z: 999

    property int autoDismissMs: 15000
    property bool acceptInput: false

    function show() {
        visible = true
        acceptInput = false
        inputGuardTimer.restart()
        dismissTimer.restart()
    }

    function dismiss() {
        visible = false
        acceptInput = false
        dismissTimer.stop()
        inputGuardTimer.stop()
    }

    Timer {
        id: dismissTimer
        interval: overlay.autoDismissMs
        onTriggered: overlay.dismiss()
    }

    Timer {
        id: inputGuardTimer
        interval: 500
        onTriggered: overlay.acceptInput = true
    }

    // Service -> slider sync (external changes, e.g. evdev zone callbacks)
    Connections {
        target: typeof AudioService !== "undefined" ? AudioService : null
        function onMasterVolumeChanged() {
            volumeSlider.value = AudioService.masterVolume
        }
    }
    Connections {
        target: typeof DisplayService !== "undefined" ? DisplayService : null
        function onBrightnessChanged() {
            brightnessSlider.value = DisplayService.brightness
        }
    }

    // Transparent touch sink -- absorbs taps but does NOT dismiss
    MouseArea {
        anchors.fill: parent
    }

    // Control panel (centered) with Level 3 elevation
    Item {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.7, Math.round(500 * UiMetrics.scale))
        height: controlsLayout.implicitHeight + Math.round(48 * UiMetrics.scale)

        // Surface tint: blend 12% primary into surfaceContainerHighest for visible elevation
        Rectangle {
            id: panelBg
            anchors.fill: parent
            radius: UiMetrics.radiusLarge
            color: Qt.rgba(ThemeService.surfaceTintHighest.r, ThemeService.surfaceTintHighest.g, ThemeService.surfaceTintHighest.b, 0.87)
            border.width: 1
            border.color: ThemeService.outlineVariant
            layer.enabled: true
            visible: false
        }

        MultiEffect {
            source: panelBg
            anchors.fill: panelBg
            shadowEnabled: true
            shadowColor: ThemeService.shadow
            shadowBlur: 0.85
            shadowVerticalOffset: 8
            shadowOpacity: 0.60
            shadowHorizontalOffset: 0
            shadowScale: 1.0
            autoPaddingEnabled: true
        }

        // Block clicks from passing through the panel
        MouseArea {
            anchors.fill: parent
            onClicked: {
                // consume -- don't dismiss
            }
        }

        ColumnLayout {
            id: controlsLayout
            anchors.centerIn: parent
            width: parent.width - Math.round(48 * UiMetrics.scale)
            spacing: UiMetrics.gap

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Quick Controls"
                font.pixelSize: UiMetrics.fontSmall
                font.bold: true
                color: ThemeService.onSurface
            }

            // Volume slider
            RowLayout {
                objectName: "overlayVolumeRow"
                Layout.fillWidth: true
                spacing: UiMetrics.marginRow

                MaterialIcon {
                    icon: "\ue050"  // volume_up
                    size: Math.round(22 * UiMetrics.scale)
                    color: ThemeService.onSurface
                }

                Slider {
                    id: volumeSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: typeof AudioService !== "undefined" ? AudioService.masterVolume : 80
                    enabled: overlay.acceptInput
                    onMoved: {
                        dismissTimer.restart()
                        if (typeof AudioService !== "undefined")
                            AudioService.setMasterVolume(Math.round(value))
                    }
                }

                Text {
                    text: Math.round(volumeSlider.value) + "%"
                    font.pixelSize: UiMetrics.fontSmall
                    color: ThemeService.onSurfaceVariant
                    Layout.preferredWidth: Math.round(40 * UiMetrics.scale)
                }
            }

            // Brightness / Screen Dimming slider
            RowLayout {
                objectName: "overlayBrightnessRow"
                Layout.fillWidth: true
                spacing: UiMetrics.marginRow

                MaterialIcon {
                    icon: typeof DisplayService !== "undefined" && DisplayService.hasHardwareBrightness ? "\ue1ac" : "\ue3a1"  // brightness_high / contrast
                    size: Math.round(22 * UiMetrics.scale)
                    color: ThemeService.onSurface
                }

                Slider {
                    id: brightnessSlider
                    Layout.fillWidth: true
                    from: 5
                    to: 100
                    value: typeof DisplayService !== "undefined" ? DisplayService.brightness : 80
                    enabled: overlay.acceptInput
                    onMoved: {
                        if (typeof DisplayService !== "undefined") {
                            DisplayService.setBrightness(Math.round(value))
                            ConfigService.setValue("display.brightness", Math.round(value))
                            ConfigService.save()
                        }
                        dismissTimer.restart()
                    }
                }

                Text {
                    text: Math.round(brightnessSlider.value) + "%"
                    font.pixelSize: UiMetrics.fontSmall
                    color: ThemeService.onSurfaceVariant
                    Layout.preferredWidth: Math.round(40 * UiMetrics.scale)
                }
            }

            // Action buttons
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: UiMetrics.sectionGap

                ElevatedButton {
                    objectName: "overlayHomeBtn"
                    text: "Home"
                    iconCode: "\ue9b2"
                    buttonEnabled: overlay.acceptInput
                    implicitWidth: UiMetrics.overlayBtnW
                    implicitHeight: UiMetrics.overlayBtnH
                    onClicked: {
                        ActionRegistry.dispatch("app.home")
                        overlay.dismiss()
                    }
                }

                ElevatedButton {
                    objectName: "overlayThemeBtn"
                    text: ThemeService.nightMode ? "Day" : "Night"
                    iconCode: ThemeService.nightMode ? "\ue518" : "\ue51c"
                    buttonEnabled: overlay.acceptInput
                    implicitWidth: UiMetrics.overlayBtnW
                    implicitHeight: UiMetrics.overlayBtnH
                    onClicked: {
                        ActionRegistry.dispatch("theme.toggle")
                        dismissTimer.restart()
                    }
                }

                ElevatedButton {
                    objectName: "overlayDesktopBtn"
                    text: "Desktop"
                    iconCode: "\uef6e"  // desktop_windows
                    buttonEnabled: overlay.acceptInput
                    implicitWidth: UiMetrics.overlayBtnW
                    implicitHeight: UiMetrics.overlayBtnH
                    onClicked: {
                        ActionRegistry.dispatch("app.exitToDesktop")
                        overlay.dismiss()
                    }
                }

                ElevatedButton {
                    objectName: "overlayCloseBtn"
                    text: "Close"
                    iconCode: "\ue5cd"
                    buttonEnabled: overlay.acceptInput
                    implicitWidth: UiMetrics.overlayBtnW
                    implicitHeight: UiMetrics.overlayBtnH
                    onClicked: overlay.dismiss()
                }
            }
        }
    }
}
