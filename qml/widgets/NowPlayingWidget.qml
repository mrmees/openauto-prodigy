import QtQuick
import QtQuick.Layouts

Item {
    id: nowPlayingWidget
    clip: true

    // Widget contract: context injection from host
    property QtObject widgetContext: null

    // Span-based breakpoints for responsive layout
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1
    readonly property bool showArtist: colSpan >= 3
    readonly property bool isTall: rowSpan >= 2

    // Provider access via widgetContext
    property bool hasMedia: widgetContext && widgetContext.mediaStatus
                            ? widgetContext.mediaStatus.hasMedia : false
    property string mediaSource: widgetContext && widgetContext.mediaStatus
                                 ? (widgetContext.mediaStatus.source || "") : ""
    property bool isPlaying: widgetContext && widgetContext.mediaStatus
                             ? widgetContext.mediaStatus.playbackState === 1 : false
    property string title: widgetContext && widgetContext.mediaStatus
                           ? (widgetContext.mediaStatus.title || "") : ""
    property string artist: widgetContext && widgetContext.mediaStatus
                            ? (widgetContext.mediaStatus.artist || "") : ""

    // Source icon codepoints
    readonly property string btIcon: "\ue1a7"      // bluetooth_audio
    readonly property string aaIcon: "\ue649"      // android

    // ---- Full layout (3x2+): title, artist, controls, source icon ----
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.spacing
        spacing: UiMetrics.spacing
        visible: isTall

        Item { Layout.fillHeight: true }

        // Source indicator (top-right corner)
        MaterialIcon {
            icon: mediaSource === "AndroidAuto" ? aaIcon : (mediaSource === "Bluetooth" ? btIcon : "")
            visible: mediaSource !== ""
            size: UiMetrics.iconSmall
            color: ThemeService.onSurfaceVariant
            Layout.alignment: Qt.AlignRight
        }

        NormalText {
            text: title || "No media"
            font.pixelSize: UiMetrics.fontTitle
            font.bold: hasMedia
            color: hasMedia ? ThemeService.onSurface : ThemeService.onSurfaceVariant
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        NormalText {
            text: artist
            visible: artist.length > 0
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.onSurfaceVariant
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        // Playback controls
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: UiMetrics.spacing * 2
            opacity: mediaSource !== "" ? 1.0 : 0.4

            MaterialIcon {
                icon: "\ue045"  // skip_previous
                size: UiMetrics.iconSize
                color: ThemeService.onSurface
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -UiMetrics.spacing
                    enabled: mediaSource !== ""
                    pressAndHoldInterval: 500
                    onClicked: if (widgetContext && widgetContext.mediaStatus) widgetContext.mediaStatus.previous()
                    onPressAndHold: {
                        if (nowPlayingWidget.parent && nowPlayingWidget.parent.requestContextMenu)
                            nowPlayingWidget.parent.requestContextMenu()
                    }
                }
            }

            MaterialIcon {
                icon: isPlaying ? "\ue034" : "\ue037"  // pause : play_arrow
                size: UiMetrics.iconSize * 1.5
                color: ThemeService.primary
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -UiMetrics.spacing
                    enabled: mediaSource !== ""
                    pressAndHoldInterval: 500
                    onClicked: if (widgetContext && widgetContext.mediaStatus) widgetContext.mediaStatus.playPause()
                    onPressAndHold: {
                        if (nowPlayingWidget.parent && nowPlayingWidget.parent.requestContextMenu)
                            nowPlayingWidget.parent.requestContextMenu()
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
                    enabled: mediaSource !== ""
                    pressAndHoldInterval: 500
                    onClicked: if (widgetContext && widgetContext.mediaStatus) widgetContext.mediaStatus.next()
                    onPressAndHold: {
                        if (nowPlayingWidget.parent && nowPlayingWidget.parent.requestContextMenu)
                            nowPlayingWidget.parent.requestContextMenu()
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }

    // ---- Compact layout (2x1, 3x1): horizontal strip ----
    RowLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.spacing
        spacing: UiMetrics.spacing
        visible: !isTall

        // Metadata column
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            NormalText {
                text: title || "No media"
                font.pixelSize: UiMetrics.fontBody
                font.bold: hasMedia
                color: hasMedia ? ThemeService.onSurface : ThemeService.onSurfaceVariant
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            NormalText {
                text: artist
                visible: showArtist && artist.length > 0
                font.pixelSize: UiMetrics.fontSmall
                color: ThemeService.onSurfaceVariant
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        // Compact controls
        RowLayout {
            spacing: UiMetrics.spacing
            opacity: mediaSource !== "" ? 1.0 : 0.4

            MaterialIcon {
                icon: "\ue045"  // skip_previous
                size: UiMetrics.iconSmall
                color: ThemeService.onSurface
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -UiMetrics.spacing
                    enabled: mediaSource !== ""
                    pressAndHoldInterval: 500
                    onClicked: if (widgetContext && widgetContext.mediaStatus) widgetContext.mediaStatus.previous()
                    onPressAndHold: {
                        if (nowPlayingWidget.parent && nowPlayingWidget.parent.requestContextMenu)
                            nowPlayingWidget.parent.requestContextMenu()
                    }
                }
            }

            MaterialIcon {
                icon: isPlaying ? "\ue034" : "\ue037"
                size: UiMetrics.iconSize
                color: ThemeService.primary
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -UiMetrics.spacing
                    enabled: mediaSource !== ""
                    pressAndHoldInterval: 500
                    onClicked: if (widgetContext && widgetContext.mediaStatus) widgetContext.mediaStatus.playPause()
                    onPressAndHold: {
                        if (nowPlayingWidget.parent && nowPlayingWidget.parent.requestContextMenu)
                            nowPlayingWidget.parent.requestContextMenu()
                    }
                }
            }

            MaterialIcon {
                icon: "\ue044"  // skip_next
                size: UiMetrics.iconSmall
                color: ThemeService.onSurface
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -UiMetrics.spacing
                    enabled: mediaSource !== ""
                    pressAndHoldInterval: 500
                    onClicked: if (widgetContext && widgetContext.mediaStatus) widgetContext.mediaStatus.next()
                    onPressAndHold: {
                        if (nowPlayingWidget.parent && nowPlayingWidget.parent.requestContextMenu)
                            nowPlayingWidget.parent.requestContextMenu()
                    }
                }
            }
        }

        // Source indicator
        MaterialIcon {
            icon: mediaSource === "AndroidAuto" ? aaIcon : (mediaSource === "Bluetooth" ? btIcon : "")
            visible: mediaSource !== ""
            size: UiMetrics.iconSmall
            color: ThemeService.onSurfaceVariant
        }
    }

    // ---- Inactive overlay (when no source) ----
    Item {
        anchors.centerIn: parent
        visible: mediaSource === "" && !hasMedia
        opacity: 0.4

        ColumnLayout {
            anchors.centerIn: parent
            spacing: UiMetrics.spacing

            MaterialIcon {
                icon: "\ue405"  // music_note
                size: rowSpan >= 2 ? UiMetrics.iconSize * 2 : UiMetrics.iconSize * 1.5
                color: ThemeService.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
            }

            NormalText {
                text: "No media"
                visible: colSpan >= 2 || rowSpan >= 2
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }

    // Long-press for context menu (edit mode)
    MouseArea {
        anchors.fill: parent
        z: -1  // behind control MouseAreas
        pressAndHoldInterval: 500
        onPressAndHold: {
            if (nowPlayingWidget.parent && nowPlayingWidget.parent.requestContextMenu)
                nowPlayingWidget.parent.requestContextMenu()
        }
        // No onClick -- interaction only through control buttons (per user decision)
    }
}
