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

                Text {
                    text: "\uD83D\uDD0A"
                    font.pixelSize: 20
                    color: "#e0e0e0"
                }

                Slider {
                    id: volumeSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: 80
                    onValueChanged: {
                        // TODO: Wire to AudioService.setMasterVolume()
                        dismissTimer.restart()
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

                Text {
                    text: "\u2600"
                    font.pixelSize: 20
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
                    text: "\u2302 Home"
                    font.pixelSize: 14
                    onClicked: {
                        if (ApplicationController)
                            ApplicationController.navigateTo(0)  // Launcher
                        overlay.dismiss()
                    }
                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "#e0e0e0"
                        horizontalAlignment: Text.AlignHCenter
                    }
                    background: Rectangle {
                        color: parent.pressed ? "#e94560" : "#0f3460"
                        radius: 8
                        implicitWidth: 100
                        implicitHeight: 36
                    }
                }

                Button {
                    text: ThemeService.nightMode ? "\u263C Day" : "\u263E Night"
                    font.pixelSize: 14
                    onClicked: {
                        if (ThemeService) ThemeService.toggleMode()
                        dismissTimer.restart()
                    }
                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "#e0e0e0"
                        horizontalAlignment: Text.AlignHCenter
                    }
                    background: Rectangle {
                        color: parent.pressed ? "#e94560" : "#0f3460"
                        radius: 8
                        implicitWidth: 100
                        implicitHeight: 36
                    }
                }

                Button {
                    text: "\u2715 Close"
                    font.pixelSize: 14
                    onClicked: overlay.dismiss()
                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "#e0e0e0"
                        horizontalAlignment: Text.AlignHCenter
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
