import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: btAudioView
    anchors.fill: parent
    color: ThemeService.background

    property bool isConnected: BtAudioPlugin && BtAudioPlugin.connectionState === 1
    property bool isPlaying: BtAudioPlugin && BtAudioPlugin.playbackState === 1

    ColumnLayout {
        anchors.centerIn: parent
        spacing: UiMetrics.sectionGap
        width: parent.width * 0.8

        // Connected device name
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: BtAudioPlugin ? (BtAudioPlugin.deviceName || "Bluetooth Audio") : "Bluetooth Audio"
            font.pixelSize: UiMetrics.fontSmall
            color: ThemeService.onSurfaceVariant
            opacity: 0.7
            visible: isConnected
        }

        // Album art placeholder
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: UiMetrics.albumArt
            height: UiMetrics.albumArt
            radius: UiMetrics.radius
            color: ThemeService.surfaceVariant
            border.color: ThemeService.onSurface
            border.width: 1

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue405"  // music_note
                size: Math.round(72 * UiMetrics.scale)
                color: ThemeService.onSurface
                opacity: 0.3
            }
        }

        // Track info
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Math.round(4 * UiMetrics.scale)

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: BtAudioPlugin ? (BtAudioPlugin.trackTitle || "No Track") : "No Track"
                font.pixelSize: UiMetrics.fontTitle
                font.bold: true
                color: ThemeService.onSurface
                elide: Text.ElideRight
                Layout.maximumWidth: btAudioView.width * 0.7
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: BtAudioPlugin ? (BtAudioPlugin.trackArtist || "Unknown Artist") : "Unknown Artist"
                font.pixelSize: UiMetrics.fontSmall
                color: ThemeService.onSurfaceVariant
                elide: Text.ElideRight
                Layout.maximumWidth: btAudioView.width * 0.7
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: BtAudioPlugin ? (BtAudioPlugin.trackAlbum || "") : ""
                font.pixelSize: UiMetrics.fontSmall
                color: ThemeService.onSurfaceVariant
                opacity: 0.7
                elide: Text.ElideRight
                Layout.maximumWidth: btAudioView.width * 0.7
                visible: text.length > 0
            }
        }

        // Progress bar (when duration available)
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: btAudioView.width * 0.6
            spacing: UiMetrics.spacing
            visible: BtAudioPlugin && BtAudioPlugin.trackDuration > 0

            Text {
                text: formatTime(BtAudioPlugin ? BtAudioPlugin.trackPosition : 0)
                font.pixelSize: UiMetrics.fontTiny
                color: ThemeService.onSurfaceVariant
            }

            Rectangle {
                Layout.fillWidth: true
                height: UiMetrics.progressH
                radius: UiMetrics.progressH / 2
                color: ThemeService.surfaceVariant

                Rectangle {
                    width: BtAudioPlugin && BtAudioPlugin.trackDuration > 0
                           ? parent.width * (BtAudioPlugin.trackPosition / BtAudioPlugin.trackDuration)
                           : 0
                    height: parent.height
                    radius: UiMetrics.progressH / 2
                    color: ThemeService.primary
                }
            }

            Text {
                text: formatTime(BtAudioPlugin ? BtAudioPlugin.trackDuration : 0)
                font.pixelSize: UiMetrics.fontTiny
                color: ThemeService.onSurfaceVariant
            }
        }

        // Connection status (when disconnected)
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "No device connected"
            font.pixelSize: UiMetrics.fontSmall
            color: ThemeService.primary
            visible: !isConnected
        }

        // Playback controls
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Math.round(32 * UiMetrics.scale)
            opacity: isConnected ? 1.0 : 0.4

            Button {
                flat: true
                enabled: isConnected
                onClicked: if (BtAudioPlugin) BtAudioPlugin.previous()
                contentItem: MaterialIcon {
                    icon: "\ue045"  // skip_previous
                    size: UiMetrics.iconSize
                    color: ThemeService.onSurface
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.primary : "transparent"
                    radius: width / 2
                    implicitWidth: UiMetrics.touchMin
                    implicitHeight: UiMetrics.touchMin
                }
            }

            Button {
                flat: true
                enabled: isConnected
                onClicked: {
                    if (!BtAudioPlugin) return
                    if (isPlaying)
                        BtAudioPlugin.pause()
                    else
                        BtAudioPlugin.play()
                }
                contentItem: MaterialIcon {
                    icon: isPlaying ? "\ue034" : "\ue037"  // pause / play_arrow
                    size: Math.round(48 * UiMetrics.scale)
                    color: ThemeService.onSurface
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.primary : "transparent"
                    radius: width / 2
                    implicitWidth: UiMetrics.callBtnSize
                    implicitHeight: UiMetrics.callBtnSize
                }
            }

            Button {
                flat: true
                enabled: isConnected
                onClicked: if (BtAudioPlugin) BtAudioPlugin.next()
                contentItem: MaterialIcon {
                    icon: "\ue044"  // skip_next
                    size: UiMetrics.iconSize
                    color: ThemeService.onSurface
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.primary : "transparent"
                    radius: width / 2
                    implicitWidth: UiMetrics.touchMin
                    implicitHeight: UiMetrics.touchMin
                }
            }
        }
    }

    function formatTime(ms) {
        var totalSec = Math.floor(ms / 1000)
        var min = Math.floor(totalSec / 60)
        var sec = totalSec % 60
        return min + ":" + (sec < 10 ? "0" : "") + sec
    }
}
