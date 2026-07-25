import QtQuick
import QtQuick.Layouts
import QtMultimedia

Item {
    id: root
    clip: true

    property QtObject widgetContext: null
    property bool ownsSink: false
    property string localStatusText: ""

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        visible: root.ownsSink && AAClusterDisplay.rendering
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.max(0, parent.width - UiMetrics.spacing * 2)
        spacing: UiMetrics.spacing
        visible: !videoOutput.visible
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

    Component.onCompleted: {
        ownsSink = AAClusterDisplay.attachVideoSink(videoOutput.videoSink)
        if (!ownsSink)
            localStatusText = "Cluster display already in use"
    }

    Component.onDestruction: {
        if (ownsSink)
            AAClusterDisplay.detachVideoSink(videoOutput.videoSink)
    }
}
