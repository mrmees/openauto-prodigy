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
    readonly property bool isTall: rowSpan >= 2
    readonly property bool isWide: colSpan >= 3

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
    readonly property string btIcon: "\uf032"       // media_bluetooth_on
    readonly property string aaIcon: "\ue859"       // android

    // Scaling helpers — buttons should be large and easy to hit
    // In tall layout, buttons get 40% of widget height
    readonly property real controlSize: height * 0.4
    readonly property real playSize: controlSize * 1.2

    // ---- No media: icon only ----
    MaterialIcon {
        anchors.centerIn: parent
        visible: !nowPlayingWidget.hasMedia
        icon: "\ue405"  // music_note
        size: Math.min(width, height) * 0.5
        color: ThemeService.onSurfaceVariant
        opacity: 0.4
    }

    // ---- Tall layout (2+ rows): text top, controls bottom ----
    Item {
        anchors.fill: parent
        anchors.margins: UiMetrics.spacing
        visible: nowPlayingWidget.hasMedia && nowPlayingWidget.isTall

        // Show skip buttons only at 3+ cols
        readonly property bool showSkip: nowPlayingWidget.colSpan >= 3
        // Button size: 45% of height, capped to fit width
        // If showing skip: 3 buttons + 2 gaps = btn * 3.6
        // If play only: just one button
        readonly property real btnFromHeight: height * 0.45
        readonly property real btnFromWidth: showSkip ? width / 3.6 : width / 1.2
        readonly property real btnSize: Math.min(btnFromHeight, btnFromWidth)

        // Top region: title + artist — vertically centered (hidden at 1x1)
        Column {
            visible: nowPlayingWidget.colSpan >= 2 || nowPlayingWidget.rowSpan >= 2
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: controlRegion.top
            anchors.bottomMargin: UiMetrics.spacing

            // Vertical centering via top padding
            topPadding: (height - titleClip.height - artistText.height - UiMetrics.spacing * 0.5) / 2

            spacing: UiMetrics.spacing * 0.5

            // Scrolling title
            Item {
                id: titleClip
                width: parent.width
                height: titleText.implicitHeight
                clip: true

                NormalText {
                    id: titleText
                    text: title
                    font.pixelSize: nowPlayingWidget.height * 0.12
                    font.weight: Font.Bold
                    color: ThemeService.onSurface
                    x: 0
                    width: implicitWidth > parent.width ? implicitWidth : parent.width
                    horizontalAlignment: implicitWidth > parent.width ? Text.AlignLeft : Text.AlignHCenter

                    SequentialAnimation on x {
                        running: titleText.implicitWidth > titleClip.width
                        loops: Animation.Infinite
                        NumberAnimation { to: 0; duration: 0 }
                        PauseAnimation { duration: 2000 }
                        NumberAnimation {
                            to: -(titleText.implicitWidth - titleClip.width)
                            duration: titleText.implicitWidth * 15
                        }
                        PauseAnimation { duration: 2000 }
                        NumberAnimation { to: 0; duration: 500 }
                    }
                }
            }

            NormalText {
                id: artistText
                text: artist
                visible: artist.length > 0
                font.pixelSize: nowPlayingWidget.height * 0.08
                color: ThemeService.onSurfaceVariant
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        // Bottom region (or centered at 1x1): controls capped to fit width
        Row {
            id: controlRegion
            anchors.horizontalCenter: parent.horizontalCenter
            // At 1x1 (no text visible), center vertically; otherwise anchor to bottom
            y: (nowPlayingWidget.colSpan <= 1 && nowPlayingWidget.rowSpan <= 1)
               ? (parent.height - height) / 2
               : parent.height - height
            height: parent.btnSize
            spacing: parent.showSkip ? parent.btnSize * 0.3 : 0

            Item {
                visible: parent.parent.showSkip
                width: visible ? parent.parent.btnSize : 0
                height: width
                MaterialIcon {
                    anchors.centerIn: parent
                    icon: "\ue045"  // skip_previous
                    size: parent.width * 0.6
                    color: ThemeService.onSurface
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: mediaSource !== ""
                    onClicked: ActionRegistry.dispatch("media.previous")
                }
            }

            Item {
                width: parent.parent.btnSize
                height: width
                MaterialIcon {
                    anchors.centerIn: parent
                    icon: isPlaying ? "\ue034" : "\ue037"  // pause / play_arrow
                    size: parent.width * 0.65
                    color: ThemeService.primary
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: mediaSource !== ""
                    onClicked: ActionRegistry.dispatch("media.playPause")
                }
            }

            Item {
                visible: parent.parent.showSkip
                width: visible ? parent.parent.btnSize : 0
                height: width
                MaterialIcon {
                    anchors.centerIn: parent
                    icon: "\ue044"  // skip_next
                    size: parent.width * 0.6
                    color: ThemeService.onSurface
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: mediaSource !== ""
                    onClicked: ActionRegistry.dispatch("media.next")
                }
            }
        }
    }

    // ---- Single row layout: horizontal strip ----
    // Skip buttons only at 4+ cols — narrow widths get play/pause only
    readonly property bool showSkipButtons: colSpan >= 4
    readonly property real rowBtnSize: height * 0.8

    RowLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.spacing
        spacing: UiMetrics.spacing
        visible: nowPlayingWidget.hasMedia && !nowPlayingWidget.isTall

        // Metadata — fills available width
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            // Scrolling title — fixed size, scrolls if too wide
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: rowTitleText.implicitHeight
                clip: true

                NormalText {
                    id: rowTitleText
                    text: title
                    font.pixelSize: nowPlayingWidget.height * 0.35
                    font.weight: Font.Bold
                    color: ThemeService.onSurface
                    x: 0
                    width: implicitWidth > parent.width ? implicitWidth : parent.width
                    horizontalAlignment: implicitWidth > parent.width ? Text.AlignLeft : Text.AlignLeft

                    SequentialAnimation on x {
                        running: rowTitleText.implicitWidth > rowTitleText.parent.width
                        loops: Animation.Infinite
                        NumberAnimation { to: 0; duration: 0 }
                        PauseAnimation { duration: 2000 }
                        NumberAnimation {
                            to: -(rowTitleText.implicitWidth - rowTitleText.parent.width)
                            duration: rowTitleText.implicitWidth * 15
                        }
                        PauseAnimation { duration: 2000 }
                        NumberAnimation { to: 0; duration: 500 }
                    }
                }
            }

            NormalText {
                text: artist
                visible: nowPlayingWidget.isWide && artist.length > 0
                font.pixelSize: nowPlayingWidget.height * 0.22
                color: ThemeService.onSurfaceVariant
                elide: Text.ElideRight
                Layout.fillWidth: true
                maximumLineCount: 1
            }
        }

        // Skip previous (3+ cols only)
        Item {
            visible: nowPlayingWidget.showSkipButtons
            Layout.preferredWidth: nowPlayingWidget.rowBtnSize
            Layout.preferredHeight: Layout.preferredWidth
            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue045"  // skip_previous
                size: parent.width * 0.7
                color: ThemeService.onSurface
            }
            MouseArea {
                anchors.fill: parent
                enabled: mediaSource !== ""
                onClicked: ActionRegistry.dispatch("media.previous")
            }
        }

        // Play/pause (always visible)
        Item {
            Layout.preferredWidth: nowPlayingWidget.rowBtnSize
            Layout.preferredHeight: Layout.preferredWidth
            MaterialIcon {
                anchors.centerIn: parent
                icon: isPlaying ? "\ue034" : "\ue037"
                size: parent.width * 0.7
                color: ThemeService.primary
            }
            MouseArea {
                anchors.fill: parent
                enabled: mediaSource !== ""
                onClicked: ActionRegistry.dispatch("media.playPause")
            }
        }

        // Skip next (3+ cols only)
        Item {
            visible: nowPlayingWidget.showSkipButtons
            Layout.preferredWidth: nowPlayingWidget.rowBtnSize
            Layout.preferredHeight: Layout.preferredWidth
            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue044"  // skip_next
                size: parent.width * 0.7
                color: ThemeService.onSurface
            }
            MouseArea {
                anchors.fill: parent
                enabled: mediaSource !== ""
                onClicked: ActionRegistry.dispatch("media.next")
            }
        }
    }

    // Long-press for context menu (edit mode)
    MouseArea {
        anchors.fill: parent
        z: -1
        pressAndHoldInterval: 500
        onPressAndHold: {
            if (nowPlayingWidget.parent && nowPlayingWidget.parent.requestContextMenu)
                nowPlayingWidget.parent.requestContextMenu()
        }
    }
}
