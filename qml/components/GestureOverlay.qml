import QtQuick
import QtQuick.Controls
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

    // Transparent touch sink — absorbs taps but does NOT dismiss
    MouseArea {
        anchors.fill: parent
    }

    // Control panel (centered)
    Rectangle {
        objectName: "overlayPanel"
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.7, Math.round(500 * UiMetrics.scale))
        height: controlsLayout.implicitHeight + Math.round(48 * UiMetrics.scale)
        radius: UiMetrics.radiusLarge
        color: Qt.rgba(ThemeService.surfaceContainerHighest.r, ThemeService.surfaceContainerHighest.g, ThemeService.surfaceContainerHighest.b, 0.87)
        border.color: ThemeService.outline
        border.width: 2

        // Block clicks from passing through the panel
        MouseArea {
            anchors.fill: parent
            onClicked: {
                // consume — don't dismiss
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

                Button {
                    objectName: "overlayHomeBtn"
                    font.pixelSize: UiMetrics.fontSmall
                    enabled: overlay.acceptInput
                    onClicked: {
                        ActionRegistry.dispatch("app.home")
                        overlay.dismiss()
                    }
                    contentItem: RowLayout {
                        spacing: UiMetrics.spacing
                        MaterialIcon {
                            icon: "\ue9b2"  // home
                            size: UiMetrics.iconSmall
                            color: ThemeService.onSurface
                        }
                        Text {
                            text: "Home"
                            font.pixelSize: UiMetrics.fontSmall
                            color: ThemeService.onSurface
                        }
                    }
                    background: Rectangle {
                        color: parent.pressed ? ThemeService.primary : ThemeService.surfaceContainerLow
                        radius: UiMetrics.radiusSmall
                        implicitWidth: UiMetrics.overlayBtnW
                        implicitHeight: UiMetrics.overlayBtnH
                    }
                }

                Button {
                    objectName: "overlayThemeBtn"
                    font.pixelSize: UiMetrics.fontSmall
                    enabled: overlay.acceptInput
                    onClicked: {
                        ActionRegistry.dispatch("theme.toggle")
                        dismissTimer.restart()
                    }
                    contentItem: RowLayout {
                        spacing: UiMetrics.spacing
                        MaterialIcon {
                            icon: ThemeService.nightMode ? "\ue518" : "\ue51c"  // light_mode / dark_mode
                            size: UiMetrics.iconSmall
                            color: ThemeService.onSurface
                        }
                        Text {
                            text: ThemeService.nightMode ? "Day" : "Night"
                            font.pixelSize: UiMetrics.fontSmall
                            color: ThemeService.onSurface
                        }
                    }
                    background: Rectangle {
                        color: parent.pressed ? ThemeService.primary : ThemeService.surfaceContainerLow
                        radius: UiMetrics.radiusSmall
                        implicitWidth: UiMetrics.overlayBtnW
                        implicitHeight: UiMetrics.overlayBtnH
                    }
                }

                Button {
                    objectName: "overlayCloseBtn"
                    font.pixelSize: UiMetrics.fontSmall
                    enabled: overlay.acceptInput
                    onClicked: overlay.dismiss()
                    contentItem: RowLayout {
                        spacing: UiMetrics.spacing
                        MaterialIcon {
                            icon: "\ue5cd"  // close
                            size: UiMetrics.iconSmall
                            color: ThemeService.onSurface
                        }
                        Text {
                            text: "Close"
                            font.pixelSize: UiMetrics.fontSmall
                            color: ThemeService.onSurface
                        }
                    }
                    background: Rectangle {
                        color: parent.pressed ? ThemeService.primary : ThemeService.surfaceContainerLow
                        radius: UiMetrics.radiusSmall
                        implicitWidth: UiMetrics.overlayBtnW
                        implicitHeight: UiMetrics.overlayBtnH
                    }
                }
            }
        }
    }
}
