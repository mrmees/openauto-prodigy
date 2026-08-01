import QtQuick
import QtQuick.Layouts
import QtMultimedia

Item {
    id: root
    clip: true

    property QtObject widgetContext: null
    property bool ownsSink: false
    property int sinkClaimAttempts: 0
    property string localStatusText: ""
    property string dashboardNavigationMode: "map"
    readonly property int maxSinkClaimAttempts: 8
    readonly property bool isMapMode: dashboardNavigationMode === "map"
    readonly property bool isCurrentPage: widgetContext
                                          ? widgetContext.isCurrentPage
                                          : false

    function syncSinkClaim() {
        if (!isCurrentPage || !isMapMode) {
            if (ownsSink)
                AAClusterDisplay.detachVideoSink(videoOutput.videoSink)
            ownsSink = false
            sinkClaimAttempts = 0
            localStatusText = ""
            return
        }

        if (ownsSink || sinkClaimAttempts >= maxSinkClaimAttempts)
            return
        ownsSink = AAClusterDisplay.attachVideoSink(videoOutput.videoSink)
        if (!ownsSink)
            ++sinkClaimAttempts
        localStatusText = ownsSink ? "" : "Cluster display already in use"
    }

    function normalizeDashboardNavigationMode(value) {
        return value === "turn_card" ? "turn_card" : "map"
    }

    Item {
        id: cropViewport
        objectName: "clusterCropViewport"
        readonly property real contentAspect:
            AAClusterDisplay.viewportContentWidth > 0
            && AAClusterDisplay.viewportContentHeight > 0
            ? AAClusterDisplay.viewportContentWidth
              / AAClusterDisplay.viewportContentHeight
            : 1
        readonly property real availableAspect:
            root.height > 0 ? root.width / root.height : contentAspect
        width: contentAspect >= availableAspect
               ? root.width : root.height * contentAspect
        height: contentAspect >= availableAspect
                ? root.width / contentAspect : root.height
        anchors.centerIn: parent
        clip: true
        visible: root.isMapMode && root.ownsSink && AAClusterDisplay.rendering

        VideoOutput {
            id: videoOutput
            objectName: "clusterVideoOutput"
            readonly property real viewportScale:
                AAClusterDisplay.viewportContentWidth > 0
                && AAClusterDisplay.viewportContentHeight > 0
                ? cropViewport.width / AAClusterDisplay.viewportContentWidth
                : 0
            width: AAClusterDisplay.viewportEncodedWidth * viewportScale
            height: AAClusterDisplay.viewportEncodedHeight * viewportScale
            x: -AAClusterDisplay.viewportContentX * viewportScale
            y: -AAClusterDisplay.viewportContentY * viewportScale
            fillMode: VideoOutput.PreserveAspectFit
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.max(0, parent.width - UiMetrics.spacing * 2)
        spacing: UiMetrics.spacing
        visible: root.isMapMode && !cropViewport.visible
        opacity: 0.6

        MaterialIcon {
            icon: "\ue55c"
            size: UiMetrics.iconSize * 2
            color: ThemeService.onSurfaceVariant
            Layout.alignment: Qt.AlignHCenter
        }

        NormalText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: ThemeService.onSurfaceVariant
            font.pixelSize: UiMetrics.fontBody
            text: root.localStatusText.length > 0
                  ? root.localStatusText
                  : AAClusterDisplay.awaitingContent
                    ? "Waiting for map"
                    : AAClusterDisplay.statusText
        }
    }

    NativeNavigationCard {
        anchors.fill: parent
        visible: !root.isMapMode
        navigationProvider: NavigationProvider
        aaConnected: ProjectionStatus
                     && (ProjectionStatus.projectionState === 3
                         || ProjectionStatus.projectionState === 4)
    }

    // SwipeView keeps adjacent dashboard pages loaded. A bounded retry window
    // covers the incoming-current/outgoing-release race without polling for
    // the entire time the dashboard page remains visible.
    Timer {
        interval: 250
        repeat: true
        running: root.isCurrentPage && root.isMapMode && !root.ownsSink
                 && root.sinkClaimAttempts < root.maxSinkClaimAttempts
        onTriggered: root.syncSinkClaim()
    }

    Connections {
        target: AAClusterDisplay
        function onVideoSinkAvailabilityChanged() {
            if (AAClusterDisplay.videoSinkAvailable
                    && root.isCurrentPage && root.isMapMode
                    && !root.ownsSink) {
                root.sinkClaimAttempts = 0
                root.syncSinkClaim()
            }
        }
    }

    Connections {
        target: ConfigService
        function onConfigChanged(path, value) {
            if (path !== "video.secondary_display_content")
                return
            root.dashboardNavigationMode =
                root.normalizeDashboardNavigationMode(value)
            root.sinkClaimAttempts = 0
            root.syncSinkClaim()
        }
    }

    onIsCurrentPageChanged: {
        sinkClaimAttempts = 0
        syncSinkClaim()
    }

    Component.onCompleted: {
        dashboardNavigationMode = normalizeDashboardNavigationMode(
            ConfigService.value("video.secondary_display_content"))
        syncSinkClaim()
    }

    Component.onDestruction: {
        if (ownsSink)
            AAClusterDisplay.detachVideoSink(videoOutput.videoSink)
    }
}
