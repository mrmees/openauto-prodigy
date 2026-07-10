import QtQuick

// Tracks tab: flat, sorted list of every track in the library. Tapping a row
// starts the whole library from that row (playAllTracks preserves row order).
// Bound to the MediaPlayerPlugin context property, same as MediaPlayerView.
Item {
    id: tracksTab

    readonly property var plugin: typeof MediaPlayerPlugin !== "undefined" ? MediaPlayerPlugin : null

    // ---- Scanning indicator (both scan edges via libraryScanning) ----
    Item {
        id: scanRow
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: (plugin && plugin.libraryScanning) ? 40 : 0
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

    // ---- Track list ----
    ListView {
        id: tracksList
        anchors.top: scanRow.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        model: plugin ? plugin.tracksModel : null

        delegate: Item {
            width: tracksList.width
            height: 64

            MaterialIcon {
                id: rowIcon
                anchors.left: parent.left
                anchors.leftMargin: UiMetrics.spacing
                anchors.verticalCenter: parent.verticalCenter
                icon: "\ue405"  // music_note
                size: 28
                color: ThemeService.onSurfaceVariant
            }

            Column {
                anchors.left: rowIcon.right
                anchors.leftMargin: UiMetrics.spacing
                anchors.right: parent.right
                anchors.rightMargin: UiMetrics.spacing
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                NormalText {
                    width: parent.width
                    text: model.name  // track title
                    font.pixelSize: 20
                    color: ThemeService.onSurface
                    elide: Text.ElideRight
                }
                NormalText {
                    width: parent.width
                    text: model.subtitle  // artist
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
                // Row order == allTrackPathsSorted() order, so index maps 1:1.
                onClicked: if (plugin) plugin.playAllTracks(index)
            }
        }
    }

    // ---- Empty state ----
    NormalText {
        visible: plugin && plugin.libraryTrackCount === 0 && !plugin.libraryScanning
        anchors.centerIn: parent
        text: "No music found — add files to ~/Music or plug in a USB drive."
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        width: parent.width * 0.8
        font.pixelSize: 18
        color: ThemeService.onSurfaceVariant
    }
}
