import QtQuick

// Artists tab: artist list → that artist's albums → album track list. Each
// drill level has a back header (same pattern as the Folders breadcrumb).
// Tapping a track row starts its album from that row. The drill-down list is a
// stale snapshot, so taps play by PATH (playAlbumFromPath) — a rescan/yank
// could reorder the library and make a bare row index hit the wrong track.
Item {
    id: artistsTab

    readonly property var plugin: typeof MediaPlayerPlugin !== "undefined" ? MediaPlayerPlugin : null

    // 0 = artists, 1 = albums-of-artist, 2 = tracks-of-album.
    property int depth: 0
    property string artistName: ""
    property var artistAlbums: []
    property string selectedAlbumKey: ""
    property string selectedAlbumName: ""
    property var albumTracks: []

    function openArtist(key, name) {
        artistName = name
        artistAlbums = plugin ? plugin.albumsForArtist(key) : []
        depth = 1
    }
    function openAlbum(key, name) {
        selectedAlbumKey = key
        selectedAlbumName = name
        albumTracks = plugin ? plugin.tracksForAlbum(key) : []
        depth = 2
    }
    function back() {
        if (depth > 0) depth = depth - 1
    }

    // ---- Scanning indicator (both scan edges; artist level only) ----
    Item {
        id: scanRow
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: (plugin && plugin.libraryScanning && artistsTab.depth === 0) ? 40 : 0
        visible: height > 0
        clip: true

        NormalText {
            anchors.centerIn: parent
            text: "Scanning…"
            font.pixelSize: 16
            color: ThemeService.onSurfaceVariant
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

    // ---- Level 0: artists ----
    ListView {
        id: artistList
        anchors.top: scanRow.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        visible: artistsTab.depth === 0
        model: plugin ? plugin.artistsModel : null

        delegate: Item {
            width: artistList.width
            height: 64

            MaterialIcon {
                id: artistIcon
                anchors.left: parent.left
                anchors.leftMargin: UiMetrics.spacing
                anchors.verticalCenter: parent.verticalCenter
                icon: "\ue030"  // library_music
                size: 28
                color: ThemeService.onSurfaceVariant
            }

            Column {
                anchors.left: artistIcon.right
                anchors.leftMargin: UiMetrics.spacing
                anchors.right: parent.right
                anchors.rightMargin: UiMetrics.spacing
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                NormalText {
                    width: parent.width
                    text: model.name
                    font.pixelSize: 20
                    color: ThemeService.onSurface
                    elide: Text.ElideRight
                }
                NormalText {
                    width: parent.width
                    text: model.subtitle  // "N album(s)"
                    visible: text.length > 0
                    font.pixelSize: 15
                    color: ThemeService.onSurfaceVariant
                    elide: Text.ElideRight
                }
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
                onClicked: artistsTab.openArtist(model.key, model.name)
            }
        }
    }

    // ---- Empty state (artist level only) ----
    NormalText {
        visible: plugin && plugin.libraryTrackCount === 0 && !plugin.libraryScanning
                 && artistsTab.depth === 0
        anchors.centerIn: parent
        text: "No music found — add files to ~/Music or plug in a USB drive."
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        width: parent.width * 0.8
        font.pixelSize: 18
        color: ThemeService.onSurfaceVariant
    }

    // ---- Shared back header (levels 1 and 2) ----
    Item {
        id: drillHeader
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 56
        visible: artistsTab.depth > 0

        Item {
            id: drillBack
            width: 56; height: parent.height
            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue5c4"  // arrow_back
                size: 28
                color: ThemeService.onSurface
            }
            MouseArea { anchors.fill: parent; onClicked: artistsTab.back() }
        }

        NormalText {
            anchors.left: drillBack.right
            anchors.right: parent.right
            anchors.rightMargin: UiMetrics.spacing
            anchors.verticalCenter: parent.verticalCenter
            text: artistsTab.depth === 1 ? artistsTab.artistName : artistsTab.selectedAlbumName
            font.pixelSize: 22
            font.weight: Font.Medium
            color: ThemeService.onSurface
            elide: Text.ElideRight
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

    // ---- Level 1: albums of the selected artist ----
    ListView {
        id: albumList
        anchors.top: drillHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        visible: artistsTab.depth === 1
        model: artistsTab.artistAlbums

        delegate: Item {
            width: albumList.width
            height: 68

            Item {
                id: albumThumb
                width: 48; height: 48
                anchors.left: parent.left
                anchors.leftMargin: UiMetrics.spacing
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    anchors.fill: parent
                    radius: UiMetrics.radiusSmall
                    color: ThemeService.surface
                }
                MaterialIcon {
                    anchors.centerIn: parent
                    visible: albumThumbImg.status !== Image.Ready
                    icon: "\ue030"  // library_music
                    size: 26
                    color: ThemeService.onSurfaceVariant
                    opacity: 0.4
                }
                Image {
                    id: albumThumbImg
                    anchors.fill: parent
                    source: modelData.artUrl
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    visible: status === Image.Ready
                }
            }

            Column {
                anchors.left: albumThumb.right
                anchors.leftMargin: UiMetrics.spacing
                anchors.right: parent.right
                anchors.rightMargin: UiMetrics.spacing
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                NormalText {
                    width: parent.width
                    text: modelData.name
                    font.pixelSize: 20
                    color: ThemeService.onSurface
                    elide: Text.ElideRight
                }
                NormalText {
                    width: parent.width
                    text: modelData.trackCount + " track(s)"
                    font.pixelSize: 15
                    color: ThemeService.onSurfaceVariant
                    elide: Text.ElideRight
                }
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
                onClicked: artistsTab.openAlbum(modelData.key, modelData.name)
            }
        }
    }

    // ---- Level 2: tracks of the selected album ----
    ListView {
        id: artistTrackList
        anchors.top: drillHeader.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        visible: artistsTab.depth === 2
        model: artistsTab.albumTracks

        delegate: Item {
            width: artistTrackList.width
            height: 60

            NormalText {
                id: trackNo
                anchors.left: parent.left
                anchors.leftMargin: UiMetrics.spacing
                anchors.verticalCenter: parent.verticalCenter
                width: 36
                horizontalAlignment: Text.AlignHCenter
                text: modelData.trackNo > 0 ? modelData.trackNo : ""
                font.pixelSize: 16
                color: ThemeService.onSurfaceVariant
            }

            Column {
                anchors.left: trackNo.right
                anchors.leftMargin: UiMetrics.spacing
                anchors.right: parent.right
                anchors.rightMargin: UiMetrics.spacing
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                NormalText {
                    width: parent.width
                    text: modelData.title
                    font.pixelSize: 19
                    color: ThemeService.onSurface
                    elide: Text.ElideRight
                }
                NormalText {
                    width: parent.width
                    text: modelData.artist
                    visible: text.length > 0
                    font.pixelSize: 14
                    color: ThemeService.onSurfaceVariant
                    elide: Text.ElideRight
                }
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
                onClicked: if (plugin) plugin.playAlbumFromPath(artistsTab.selectedAlbumKey, modelData.path)
            }
        }
    }
}
