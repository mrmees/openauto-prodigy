import QtQuick
import QtQuick.Layouts

Item {
    id: nowPlayingWidget

    property bool isMainPane: typeof widgetContext !== "undefined"
                              ? widgetContext.paneSize === 1 : false
    property bool isConnected: typeof BtAudioPlugin !== "undefined"
                               && BtAudioPlugin.connectionState === 1
    property bool isPlaying: typeof BtAudioPlugin !== "undefined"
                             && BtAudioPlugin.playbackState === 1
    property string title: typeof BtAudioPlugin !== "undefined"
                           ? (BtAudioPlugin.trackTitle || "") : ""
    property string artist: typeof BtAudioPlugin !== "undefined"
                            ? (BtAudioPlugin.trackArtist || "") : ""

    Rectangle {
        anchors.fill: parent
        radius: UiMetrics.radius
        color: ThemeService.surfaceContainer

        // Main pane: centered track info + controls
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: UiMetrics.spacing
            spacing: UiMetrics.spacing
            visible: isMainPane

            Item { Layout.fillHeight: true }

            MaterialIcon {
                icon: "\ue405"  // music_note
                size: UiMetrics.iconSize * 1.5
                color: isConnected ? ThemeService.primary : ThemeService.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
            }

            NormalText {
                text: title || "No music playing"
                font.pixelSize: UiMetrics.fontTitle
                font.bold: title.length > 0
                color: ThemeService.onSurface
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }

            NormalText {
                text: artist || (isConnected ? "Unknown Artist" : "")
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                visible: text.length > 0
            }

            // Playback controls
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: UiMetrics.spacing * 2
                opacity: isConnected ? 1.0 : 0.3

                MaterialIcon {
                    icon: "\ue045"  // skip_previous
                    size: UiMetrics.iconSize
                    color: ThemeService.onSurface
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -UiMetrics.spacing
                        onClicked: if (BtAudioPlugin) BtAudioPlugin.previous()
                    }
                }

                MaterialIcon {
                    icon: isPlaying ? "\ue034" : "\ue037"  // pause / play_arrow
                    size: UiMetrics.iconSize * 1.5
                    color: ThemeService.primary
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -UiMetrics.spacing
                        onClicked: {
                            if (!BtAudioPlugin) return
                            if (isPlaying) BtAudioPlugin.pause()
                            else BtAudioPlugin.play()
                        }
                    }
                }

                MaterialIcon {
                    icon: "\ue044"  // skip_next
                    size: UiMetrics.iconSize
                    color: ThemeService.onSurface
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -UiMetrics.spacing
                        onClicked: if (BtAudioPlugin) BtAudioPlugin.next()
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }

        // Sub pane: compact horizontal strip
        RowLayout {
            anchors.fill: parent
            anchors.margins: UiMetrics.spacing
            spacing: UiMetrics.spacing
            visible: !isMainPane

            MaterialIcon {
                icon: "\ue405"  // music_note
                size: UiMetrics.iconSmall
                color: isConnected ? ThemeService.primary : ThemeService.onSurfaceVariant
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                NormalText {
                    text: title || "No music"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                NormalText {
                    text: artist
                    font.pixelSize: UiMetrics.fontSmall
                    color: ThemeService.onSurfaceVariant
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    visible: text.length > 0
                }
            }

            MaterialIcon {
                icon: isPlaying ? "\ue034" : "\ue037"
                size: UiMetrics.iconSmall
                color: ThemeService.onSurface
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -UiMetrics.spacing
                    onClicked: {
                        if (!BtAudioPlugin) return
                        if (isPlaying) BtAudioPlugin.pause()
                        else BtAudioPlugin.play()
                    }
                }
            }
        }
    }
}
