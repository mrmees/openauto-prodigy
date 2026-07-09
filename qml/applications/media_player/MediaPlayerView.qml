import QtQuick

// Local media player — stage 1: Folders browse + persistent now-playing bar.
// Bound to the MediaPlayerPlugin context property (set in onActivated).
// Stage 2 adds Artists/Albums/Tracks tabs in the header row.
Item {
    id: mediaPlayerView

    readonly property var plugin: typeof MediaPlayerPlugin !== "undefined" ? MediaPlayerPlugin : null
    readonly property var folders: plugin ? plugin.folderModel : null

    function fmtTime(ms) {
        if (ms <= 0) return "0:00"
        var s = Math.floor(ms / 1000)
        var m = Math.floor(s / 60)
        s = s % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    // ---- Header: back + breadcrumb + refresh ----
    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 56

        Item {
            id: backButton
            width: 56; height: parent.height
            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue5c4"  // arrow_back
                size: 28
                color: ThemeService.onSurface
                opacity: folders && !folders.atTopLevel ? 1.0 : 0.3
            }
            MouseArea { anchors.fill: parent; onClicked: if (folders) folders.up() }
        }

        NormalText {
            anchors.left: backButton.right
            anchors.right: refreshButton.left
            anchors.verticalCenter: parent.verticalCenter
            text: folders ? folders.breadcrumb : ""
            font.pixelSize: 22
            font.weight: Font.Medium
            color: ThemeService.onSurface
            elide: Text.ElideLeft
        }

        Item {
            id: refreshButton
            anchors.right: parent.right
            width: 56; height: parent.height
            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue5d5"  // refresh
                size: 26
                color: ThemeService.onSurfaceVariant
            }
            MouseArea { anchors.fill: parent; onClicked: if (plugin) plugin.refreshSources() }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: ThemeService.onSurfaceVariant
            opacity: 0.15
        }
    }

    // ---- Browse list ----
    ListView {
        id: browseList
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: nowPlayingBar.top
        clip: true
        model: mediaPlayerView.folders

        delegate: Item {
            width: browseList.width
            height: 64

            MaterialIcon {
                id: rowIcon
                anchors.left: parent.left
                anchors.leftMargin: UiMetrics.spacing
                anchors.verticalCenter: parent.verticalCenter
                icon: model.isDir ? "\ue2c7" : "\ue405"  // folder / music_note
                size: 30
                color: model.isDir ? ThemeService.primary : ThemeService.onSurfaceVariant
            }

            NormalText {
                anchors.left: rowIcon.right
                anchors.leftMargin: UiMetrics.spacing
                anchors.right: parent.right
                anchors.rightMargin: UiMetrics.spacing
                anchors.verticalCenter: parent.verticalCenter
                text: model.name
                font.pixelSize: 20
                color: ThemeService.onSurface
                elide: Text.ElideRight
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: ThemeService.onSurfaceVariant
                opacity: 0.10
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (model.isDir) mediaPlayerView.folders.enter(model.path)
                    else if (mediaPlayerView.plugin) mediaPlayerView.plugin.playFileFromFolder(model.path)
                }
            }
        }

        NormalText {
            visible: browseList.count === 0
            anchors.centerIn: parent
            text: folders && folders.atTopLevel
                  ? "No music sources found.\nAdd files to ~/Music or plug in a USB drive."
                  : "No playable files here."
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 18
            color: ThemeService.onSurfaceVariant
        }
    }

    // ---- Now-playing bar ----
    Rectangle {
        id: nowPlayingBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: plugin && plugin.hasTrack ? 112 : 0
        visible: height > 0
        color: ThemeService.surface

        // Seek strip: 6px visual bar with a 24px touch strip over it.
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 6
            color: ThemeService.onSurfaceVariant
            opacity: 0.25
        }
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            height: 6
            width: plugin && plugin.trackDuration > 0
                   ? parent.width * Math.min(1, plugin.trackPosition / plugin.trackDuration)
                   : 0
            color: ThemeService.primary
        }
        MouseArea {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 24
            onClicked: function(mouse) {
                if (mediaPlayerView.plugin && mediaPlayerView.plugin.trackDuration > 0)
                    mediaPlayerView.plugin.seekTo(Math.round(mouse.x / width * mediaPlayerView.plugin.trackDuration))
            }
        }

        // Cover art
        Image {
            id: barArt
            anchors.left: parent.left
            anchors.leftMargin: UiMetrics.spacing
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 3   // visually below the seek strip
            width: 80; height: 80
            source: plugin ? plugin.artUrl : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            visible: status === Image.Ready
        }
        MaterialIcon {
            anchors.centerIn: barArt
            visible: !barArt.visible
            icon: "\ue405"  // music_note placeholder
            size: 44
            color: ThemeService.onSurfaceVariant
            opacity: 0.4
        }

        // Title / artist / time
        Column {
            anchors.left: barArt.right
            anchors.leftMargin: UiMetrics.spacing
            anchors.right: transportRow.left
            anchors.rightMargin: UiMetrics.spacing
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 3
            spacing: 2

            NormalText {
                width: parent.width
                text: plugin ? plugin.trackTitle : ""
                font.pixelSize: 22
                font.weight: Font.Bold
                color: ThemeService.onSurface
                elide: Text.ElideRight
            }
            NormalText {
                width: parent.width
                text: plugin ? plugin.trackArtist : ""
                visible: text.length > 0
                font.pixelSize: 17
                color: ThemeService.onSurfaceVariant
                elide: Text.ElideRight
            }
            NormalText {
                text: plugin ? fmtTime(plugin.trackPosition) + " / " + fmtTime(plugin.trackDuration) : ""
                font.pixelSize: 14
                color: ThemeService.onSurfaceVariant
            }
        }

        // Transport + modes
        Row {
            id: transportRow
            anchors.right: parent.right
            anchors.rightMargin: UiMetrics.spacing
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 3
            spacing: UiMetrics.spacing * 0.75

            Item {
                width: 56; height: 56
                MaterialIcon {
                    anchors.centerIn: parent
                    icon: "\ue043"  // shuffle
                    size: 26
                    color: plugin && plugin.shuffle ? ThemeService.primary : ThemeService.onSurfaceVariant
                }
                MouseArea { anchors.fill: parent; onClicked: if (plugin) plugin.toggleShuffle() }
            }

            Item {
                width: 56; height: 56
                MaterialIcon {
                    anchors.centerIn: parent
                    icon: "\ue045"  // skip_previous
                    size: 34
                    color: ThemeService.onSurface
                }
                MouseArea { anchors.fill: parent; onClicked: if (plugin) plugin.previous() }
            }

            Item {
                width: 64; height: 64
                anchors.verticalCenter: parent.verticalCenter
                MaterialIcon {
                    anchors.centerIn: parent
                    icon: plugin && plugin.isPlaying ? "\ue034" : "\ue037"  // pause / play_arrow
                    size: 44
                    color: ThemeService.primary
                }
                MouseArea { anchors.fill: parent; onClicked: if (plugin) plugin.playPause() }
            }

            Item {
                width: 56; height: 56
                MaterialIcon {
                    anchors.centerIn: parent
                    icon: "\ue044"  // skip_next
                    size: 34
                    color: ThemeService.onSurface
                }
                MouseArea { anchors.fill: parent; onClicked: if (plugin) plugin.next() }
            }

            Item {
                width: 56; height: 56
                MaterialIcon {
                    anchors.centerIn: parent
                    // repeat / repeat_one; dimmed when off
                    icon: plugin && plugin.repeatMode === 2 ? "\ue041" : "\ue040"
                    size: 26
                    color: plugin && plugin.repeatMode !== 0 ? ThemeService.primary : ThemeService.onSurfaceVariant
                }
                MouseArea { anchors.fill: parent; onClicked: if (plugin) plugin.cycleRepeat() }
            }
        }
    }
}
