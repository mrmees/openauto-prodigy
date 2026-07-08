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
                             ? widgetContext.mediaStatus.isPlaying === true : false
    property string title: widgetContext && widgetContext.mediaStatus
                           ? (widgetContext.mediaStatus.title || "") : ""
    property string artist: widgetContext && widgetContext.mediaStatus
                            ? (widgetContext.mediaStatus.artist || "") : ""
    property string artUrl: widgetContext && widgetContext.mediaStatus
                            ? (widgetContext.mediaStatus.artUrl || "") : ""
    property bool hasPosition: widgetContext && widgetContext.mediaStatus
                               ? widgetContext.mediaStatus.hasPosition === true : false
    property real trackPosition: widgetContext && widgetContext.mediaStatus
                                 ? widgetContext.mediaStatus.position : -1
    property real trackDuration: widgetContext && widgetContext.mediaStatus
                                 ? widgetContext.mediaStatus.duration : 0

    // Source icon codepoints
    readonly property string btIcon: "\uf032"       // media_bluetooth_on
    readonly property string aaIcon: "\ue859"       // android
    readonly property string localIcon: "\ue030"    // library_music (local media player)

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

        // Cover art (tall layout, 3+ cols, when the source provides it)
        Image {
            id: tallArt
            visible: nowPlayingWidget.colSpan >= 3 && nowPlayingWidget.artUrl !== ""
                     && status === Image.Ready
            source: nowPlayingWidget.colSpan >= 3 ? nowPlayingWidget.artUrl : ""
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: controlRegion.top
            anchors.bottomMargin: UiMetrics.spacing
            width: visible ? height : 0
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }

        // Top region: title + artist — vertically centered (hidden at 1x1)
        Column {
            visible: nowPlayingWidget.colSpan >= 2 || nowPlayingWidget.rowSpan >= 2
            anchors.left: tallArt.visible ? tallArt.right : parent.left
            anchors.leftMargin: tallArt.visible ? UiMetrics.spacing : 0
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

        // Cover art thumbnail (wide row layouts, when available)
        Image {
            source: nowPlayingWidget.isWide ? nowPlayingWidget.artUrl : ""
            visible: nowPlayingWidget.isWide && nowPlayingWidget.artUrl !== ""
                     && status === Image.Ready
            Layout.preferredWidth: visible ? nowPlayingWidget.height * 0.8 : 0
            Layout.preferredHeight: Layout.preferredWidth
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }

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

    // Source badge (top-right): which source owns the display right now
    MaterialIcon {
        visible: nowPlayingWidget.hasMedia && nowPlayingWidget.mediaSource !== ""
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: UiMetrics.spacing * 0.5
        icon: nowPlayingWidget.mediaSource === "Bluetooth" ? nowPlayingWidget.btIcon
            : nowPlayingWidget.mediaSource === "AndroidAuto" ? nowPlayingWidget.aaIcon
            : nowPlayingWidget.localIcon
        size: Math.max(14, nowPlayingWidget.height * 0.10)
        color: ThemeService.onSurfaceVariant
        opacity: 0.7
    }

    // Track progress along the bottom edge (only when the source reports it).
    // Container is an Item, NOT a Rectangle: child opacity multiplies under a
    // translucent parent, so track and fill must be siblings.
    Item {
        visible: nowPlayingWidget.hasMedia && nowPlayingWidget.hasPosition
                 && nowPlayingWidget.trackDuration > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 3

        Rectangle {  // track
            anchors.fill: parent
            color: ThemeService.onSurfaceVariant
            opacity: 0.25
        }
        Rectangle {  // fill
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * Math.min(1, nowPlayingWidget.trackPosition / nowPlayingWidget.trackDuration)
            color: ThemeService.primary
            opacity: 0.85
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
