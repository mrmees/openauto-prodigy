import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/// Translucent overlay shown when a 3-finger tap gesture is detected.
/// Provides quick controls: volume, brightness, home button, dismiss.
/// Auto-dismisses after 5 seconds or on tap outside controls.
Rectangle {
    id: overlay
    anchors.fill: parent
    color: "#AA000000"
    visible: false
    z: 999

    property int autoDismissMs: 5000

    function show() {
        visible = true
        dismissTimer.restart()
    }

    function dismiss() {
        visible = false
        dismissTimer.stop()
    }

    Timer {
        id: dismissTimer
        interval: overlay.autoDismissMs
        onTriggered: overlay.dismiss()
    }

    // Tap outside controls to dismiss
    MouseArea {
        anchors.fill: parent
        onClicked: overlay.dismiss()
    }

    // Control panel (centered)
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.7, 500)
        height: controlsLayout.implicitHeight + 48
        radius: 16
        color: "#DD1a1a2e"
        border.color: "#0f3460"
        border.width: 2

        // Block clicks from dismissing when clicking inside the panel
        MouseArea {
            anchors.fill: parent
            onClicked: {
                // consume — don't dismiss
            }
        }

        ColumnLayout {
            id: controlsLayout
            anchors.centerIn: parent
            width: parent.width - 48
            spacing: 16

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Quick Controls"
                font.pixelSize: 16
                font.bold: true
                color: "#e0e0e0"
            }

            // Volume slider
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                MaterialIcon {
                    icon: "\ue050"  // volume_up
                    size: 22
                    color: "#e0e0e0"
                }

                Slider {
                    id: volumeSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: 80
                    Component.onCompleted: {
                        if (typeof AudioService !== "undefined")
                            value = AudioService.masterVolume()
                    }
                    onValueChanged: {
                        dismissTimer.restart()
                        if (typeof AudioService !== "undefined")
                            AudioService.setMasterVolume(Math.round(value))
                    }
                }

                Text {
                    text: Math.round(volumeSlider.value) + "%"
                    font.pixelSize: 14
                    color: "#a0a0c0"
                    Layout.preferredWidth: 40
                }
            }

            // Brightness slider
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                MaterialIcon {
                    icon: "\ue1ac"  // brightness_high
                    size: 22
                    color: "#e0e0e0"
                }

                Slider {
                    id: brightnessSlider
                    Layout.fillWidth: true
                    from: 10
                    to: 100
                    value: 80
                    onValueChanged: {
                        // TODO: Wire to IDisplayService.setBrightness()
                        dismissTimer.restart()
                    }
                }

                Text {
                    text: Math.round(brightnessSlider.value) + "%"
                    font.pixelSize: 14
                    color: "#a0a0c0"
                    Layout.preferredWidth: 40
                }
            }

            // Action buttons
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 24

                Button {
                    font.pixelSize: 14
                    onClicked: {
                        ActionRegistry.dispatch("app.home")
                        overlay.dismiss()
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        MaterialIcon {
                            icon: "\ue9b2"  // home
                            size: 18
                            color: "#e0e0e0"
                        }
                        Text {
                            text: "Home"
                            font.pixelSize: 14
                            color: "#e0e0e0"
                        }
                    }
                    background: Rectangle {
                        color: parent.pressed ? "#e94560" : "#0f3460"
                        radius: 8
                        implicitWidth: 100
                        implicitHeight: 36
                    }
                }

                Button {
                    font.pixelSize: 14
                    onClicked: {
                        ActionRegistry.dispatch("theme.toggle")
                        dismissTimer.restart()
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        MaterialIcon {
                            icon: ThemeService.nightMode ? "\ue518" : "\ue51c"  // light_mode / dark_mode
                            size: 18
                            color: "#e0e0e0"
                        }
                        Text {
                            text: ThemeService.nightMode ? "Day" : "Night"
                            font.pixelSize: 14
                            color: "#e0e0e0"
                        }
                    }
                    background: Rectangle {
                        color: parent.pressed ? "#e94560" : "#0f3460"
                        radius: 8
                        implicitWidth: 100
                        implicitHeight: 36
                    }
                }

                Button {
                    font.pixelSize: 14
                    onClicked: overlay.dismiss()
                    contentItem: RowLayout {
                        spacing: 6
                        MaterialIcon {
                            icon: "\ue5cd"  // close
                            size: 18
                            color: "#e0e0e0"
                        }
                        Text {
                            text: "Close"
                            font.pixelSize: 14
                            color: "#e0e0e0"
                        }
                    }
                    background: Rectangle {
                        color: parent.pressed ? "#e94560" : "#0f3460"
                        radius: 8
                        implicitWidth: 100
                        implicitHeight: 36
                    }
                }
            }
        }
    }
}
