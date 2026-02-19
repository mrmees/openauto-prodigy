import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: btAudioView
    anchors.fill: parent
    color: ThemeService.backgroundColor

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 20
        width: parent.width * 0.8

        // Album art placeholder
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 200
            height: 200
            radius: 12
            color: ThemeService.controlBackgroundColor
            border.color: ThemeService.borderColor
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "\u266B"  // musical note
                font.pixelSize: 72
                color: ThemeService.primaryTextColor
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
                color: ThemeService.primaryTextColor
                elide: Text.ElideRight
                Layout.maximumWidth: btAudioView.width * 0.7
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: BtAudioPlugin ? (BtAudioPlugin.trackArtist || "Unknown Artist") : "Unknown Artist"
                font.pixelSize: 16
                color: ThemeService.secondaryTextColor
                elide: Text.ElideRight
                Layout.maximumWidth: btAudioView.width * 0.7
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: BtAudioPlugin ? (BtAudioPlugin.trackAlbum || "") : ""
                font.pixelSize: 14
                color: ThemeService.secondaryTextColor
                opacity: 0.7
                elide: Text.ElideRight
                Layout.maximumWidth: btAudioView.width * 0.7
                visible: text.length > 0
            }
        }

        // Connection status
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: {
                if (!BtAudioPlugin) return "Plugin unavailable"
                if (BtAudioPlugin.connectionState === 0) return "No device connected"
                return ""
            }
            font.pixelSize: 14
            color: ThemeService.warningColor
            visible: !BtAudioPlugin || BtAudioPlugin.connectionState === 0
        }

        // Playback controls
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 32

            Button {
                text: "\u23EE"  // previous track
                font.pixelSize: 28
                flat: true
                onClicked: if (BtAudioPlugin) BtAudioPlugin.previous()
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: ThemeService.primaryTextColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.highlightColor : "transparent"
                    radius: width / 2
                }
            }

            Button {
                text: BtAudioPlugin && BtAudioPlugin.playbackState === 1 ? "\u23F8" : "\u25B6"
                font.pixelSize: 40
                flat: true
                onClicked: {
                    if (!BtAudioPlugin) return
                    if (BtAudioPlugin.playbackState === 1)
                        BtAudioPlugin.pause()
                    else
                        BtAudioPlugin.play()
                }
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: ThemeService.primaryTextColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.highlightColor : "transparent"
                    radius: width / 2
                }
            }

            Button {
                text: "\u23ED"  // next track
                font.pixelSize: 28
                flat: true
                onClicked: if (BtAudioPlugin) BtAudioPlugin.next()
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: ThemeService.primaryTextColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.pressed ? ThemeService.highlightColor : "transparent"
                    radius: width / 2
                }
            }
        }
    }
}
