import QtQuick

// Albums tab: a grid of album art. Tapping an album drills into an inline
// track list (same back-header pattern as the Folders breadcrumb); tapping a
// track row starts that album from that row. tracksForAlbum() row order is
// identical to trackPathsForAlbum(), so row N maps to playAlbum(key, N).
Item {
    id: albumsTab

    readonly property var plugin: typeof MediaPlayerPlugin !== "undefined" ? MediaPlayerPlugin : null

    // Drill-down state ("" == showing the grid).
    property string selectedAlbumKey: ""
    property string selectedAlbumName: ""
    property var albumTracks: []

    function openAlbum(key, name) {
        selectedAlbumKey = key
        selectedAlbumName = name
        albumTracks = plugin ? plugin.tracksForAlbum(key) : []
    }
    function closeAlbum() {
        selectedAlbumKey = ""
        albumTracks = []
    }

    // ---- Scanning indicator (both scan edges) ----
    Item {
        id: scanRow
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: (plugin && plugin.libraryScanning) ? 40 : 0
        visible: height > 0 && albumsTab.selectedAlbumKey === ""
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

    // ---- Album grid ----
    GridView {
        id: albumGrid
        anchors.top: scanRow.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: UiMetrics.spacing
        anchors.rightMargin: UiMetrics.spacing
        clip: true
        visible: albumsTab.selectedAlbumKey === ""
        cellWidth: 180
        cellHeight: 220
        model: plugin ? plugin.albumsModel : null

        delegate: Item {
            width: albumGrid.cellWidth
            height: albumGrid.cellHeight

            Column {
                anchors.fill: parent
                anchors.margins: UiMetrics.spacing
                spacing: UiMetrics.spacing * 0.5

                // Art (square) with a library_music glyph fallback.
                Item {
                    width: parent.width
                    height: width

                    Rectangle {
                        anchors.fill: parent
                        radius: UiMetrics.radiusSmall
                        color: ThemeService.surface
                    }
                    MaterialIcon {
                        anchors.centerIn: parent
                        visible: albumArt.status !== Image.Ready
                        icon: "\ue030"  // library_music
                        size: parent.width * 0.4
                        color: ThemeService.onSurfaceVariant
                        opacity: 0.4
                    }
                    Image {
                        id: albumArt
                        anchors.fill: parent
                        source: model.artUrl
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        visible: status === Image.Ready
                    }
                }

                NormalText {
                    width: parent.width
                    text: model.name
                    font.pixelSize: 17
                    font.weight: Font.Medium
                    color: ThemeService.onSurface
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
                NormalText {
                    width: parent.width
                    text: model.subtitle  // artist / Various Artists
                    visible: text.length > 0
                    font.pixelSize: 14
                    color: ThemeService.onSurfaceVariant
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: albumsTab.openAlbum(model.key, model.name)
            }
        }
    }

    // ---- Empty state (grid level only) ----
    NormalText {
        visible: plugin && plugin.libraryTrackCount === 0 && !plugin.libraryScanning
                 && albumsTab.selectedAlbumKey === ""
        anchors.centerIn: parent
        text: "No music found — add files to ~/Music or plug in a USB drive."
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        width: parent.width * 0.8
        font.pixelSize: 18
        color: ThemeService.onSurfaceVariant
    }

    // ---- Track-list drill-down (inline overlay panel with back header) ----
    Item {
        id: trackPanel
        anchors.fill: parent
        visible: albumsTab.selectedAlbumKey !== ""

        Item {
            id: panelHeader
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 56

            Item {
                id: panelBack
                width: 56; height: parent.height
                MaterialIcon {
                    anchors.centerIn: parent
                    icon: "\ue5c4"  // arrow_back
                    size: 28
                    color: ThemeService.onSurface
                }
                MouseArea { anchors.fill: parent; onClicked: albumsTab.closeAlbum() }
            }

            NormalText {
                anchors.left: panelBack.right
                anchors.right: parent.right
                anchors.rightMargin: UiMetrics.spacing
                anchors.verticalCenter: parent.verticalCenter
                text: albumsTab.selectedAlbumName
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

        ListView {
            id: trackList
            anchors.top: panelHeader.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true
            model: albumsTab.albumTracks

            delegate: Item {
                width: trackList.width
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
                    onClicked: if (plugin) plugin.playAlbum(albumsTab.selectedAlbumKey, index)
                }
            }
        }
    }
}
