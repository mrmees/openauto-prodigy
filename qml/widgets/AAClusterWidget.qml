import QtQuick
import QtQuick.Layouts
import QtMultimedia

Item {
    id: root
    clip: true

    property QtObject widgetContext: null
    property bool ownsSink: false
    property string localStatusText: ""
    readonly property bool isCurrentPage: widgetContext
                                          ? widgetContext.isCurrentPage
                                          : false

    function syncSinkClaim() {
        if (!isCurrentPage) {
            if (ownsSink)
                AAClusterDisplay.detachVideoSink(videoOutput.videoSink)
            ownsSink = false
            localStatusText = ""
            return
        }

        if (ownsSink)
            return
        ownsSink = AAClusterDisplay.attachVideoSink(videoOutput.videoSink)
        localStatusText = ownsSink ? "" : "Cluster display already in use"
    }

    Item {
        id: cropViewport
        objectName: "clusterCropViewport"
        width: Math.min(root.width, root.height)
        height: AAClusterDisplay.viewportContentWidth > 0
                ? width * AAClusterDisplay.viewportContentHeight
                    / AAClusterDisplay.viewportContentWidth
                : 0
        anchors.centerIn: parent
        clip: true
        visible: root.ownsSink && AAClusterDisplay.rendering

        VideoOutput {
            id: videoOutput
            objectName: "clusterVideoOutput"
            readonly property real viewportScale:
                AAClusterDisplay.viewportContentWidth > 0
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
        visible: !cropViewport.visible
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
                  : AAClusterDisplay.statusText
        }
    }

    // SwipeView keeps adjacent dashboard pages loaded. A short retry makes a
    // page transition robust when the incoming copy becomes current just
    // before the outgoing copy releases the single cluster sink.
    Timer {
        interval: 250
        repeat: true
        running: root.isCurrentPage && !root.ownsSink
        onTriggered: root.syncSinkClaim()
    }

    onIsCurrentPageChanged: syncSinkClaim()

    Component.onCompleted: syncSinkClaim()

    Component.onDestruction: {
        if (ownsSink)
            AAClusterDisplay.detachVideoSink(videoOutput.videoSink)
    }
}
