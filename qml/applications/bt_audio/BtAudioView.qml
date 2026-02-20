import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: btAudioView
    anchors.fill: parent
    color: ThemeService.backgroundColor

    property bool isConnected: BtAudioPlugin && BtAudioPlugin.connectionState === 1
    property bool isPlaying: BtAudioPlugin && BtAudioPlugin.playbackState === 1

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 20
        width: parent.width * 0.8

        // Connected device name
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: BtAudioPlugin ? (BtAudioPlugin.deviceName || "Bluetooth Audio") : "Bluetooth Audio"
            font.pixelSize: 14
            color: ThemeService.descriptionFontColor
            opacity: 0.7
            visible: isConnected
        }

        // Album art placeholder
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 200
            height: 200
            radius: 12
            color: ThemeService.controlBackgroundColor
            border.color: ThemeService.controlForegroundColor
            border.width: 1

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue405"  // music_note
                size: 72
                color: ThemeService.normalFontColor
                opacity: 0.3
            }
        }

        // Track info
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 4

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: BtAudioPlugin ? (BtAudioPlugin.trackTitle || "No Track") : "No Track"
                font.pixelSize: 22
                font.bold: true
                color: ThemeService.normalFontColor
                elide: Text.ElideRight
                Layout.maximumWidth: btAudioView.width * 0.7
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: BtAudioPlugin ? (BtAudioPlugin.trackArtist || "Unknown Artist") : "Unknown Artist"
                font.pixelSize: 16
                color: ThemeService.descriptionFontColor
                elide: Text.ElideRight
                Layout.maximumWidth: btAudioView.width * 0.7
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: BtAudioPlugin ? (BtAudioPlugin.trackAlbum || "") : ""
                font.pixelSize: 14
                color: ThemeService.descriptionFontColor
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
            spacing: 8
            visible: BtAudioPlugin && BtAudioPlugin.trackDuration > 0

            Text {
                text: formatTime(BtAudioPlugin ? BtAudioPlugin.trackPosition : 0)
                font.pixelSize: 12
                color: ThemeService.descriptionFontColor
            }

            Rectangle {
                Layout.fillWidth: true
                height: 4
                radius: 2
                color: ThemeService.controlBackgroundColor

                Rectangle {
                    width: BtAudioPlugin && BtAudioPlugin.trackDuration > 0
                           ? parent.width * (BtAudioPlugin.trackPosition / BtAudioPlugin.trackDuration)
                           : 0
                    height: parent.height
                    radius: 2
                    color: ThemeService.highlightColor
                }
            }

            Text {
                text: formatTime(BtAudioPlugin ? BtAudioPlugin.trackDuration : 0)
                font.pixelSize: 12
                color: ThemeService.descriptionFontColor
            }
        }

        // Connection status (when disconnected)
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "No device connected"
            font.pixelSize: 14
            color: ThemeService.highlightColor
            visible: !isConnected
        }

        // Playback controls
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 32
            opacity: isConnected ? 1.0 : 0.4

            Button {
                flat: true
                enabled: isConnected
                onClicked: if (BtAudioPlugin) BtAudioPlugin.previous()
                contentItem: MaterialIcon {
                    icon: "\ue045"  // skip_previous
                    size: 32
                    color: ThemeService.normalFontColor
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.highlightColor : "transparent"
                    radius: width / 2
                    implicitWidth: 56
                    implicitHeight: 56
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
                    size: 48
                    color: ThemeService.normalFontColor
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.highlightColor : "transparent"
                    radius: width / 2
                    implicitWidth: 72
                    implicitHeight: 72
                }
            }

            Button {
                flat: true
                enabled: isConnected
                onClicked: if (BtAudioPlugin) BtAudioPlugin.next()
                contentItem: MaterialIcon {
                    icon: "\ue044"  // skip_next
                    size: 32
                    color: ThemeService.normalFontColor
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.highlightColor : "transparent"
                    radius: width / 2
                    implicitWidth: 56
                    implicitHeight: 56
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
