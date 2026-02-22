import QtQuick
import QtQuick.Layouts
import QtMultimedia

Item {
    id: androidAutoMenu

    Component.onCompleted: ApplicationController.setTitle("Android Auto")

    // Read sidebar config via ConfigService
    readonly property bool sidebarEnabled: {
        var v = ConfigService.value("video.sidebar.enabled")
        return v === true || v === 1 || v === "true"
    }
    readonly property int sidebarWidth: ConfigService.value("video.sidebar.width") || 150
    readonly property string sidebarPosition: ConfigService.value("video.sidebar.position") || "right"
    readonly property bool projecting: AndroidAutoService.connectionState === 3

    RowLayout {
        anchors.fill: parent
        spacing: 0
        layoutDirection: androidAutoMenu.sidebarPosition === "left" ? Qt.RightToLeft : Qt.LeftToRight

        // AA Video area
        Item {
            id: videoArea
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Black background for letterbox bars
            Rectangle {
                anchors.fill: parent
                color: "black"
                visible: androidAutoMenu.projecting
            }

            // Video output — crops margin black bars when sidebar active
            VideoOutput {
                id: videoOutput
                anchors.fill: parent
                visible: androidAutoMenu.projecting
                fillMode: androidAutoMenu.sidebarEnabled
                         ? VideoOutput.PreserveAspectCrop
                         : VideoOutput.PreserveAspectFit
            }

            // Debug touch overlay
            Repeater {
                model: TouchHandler.debugOverlay ? TouchHandler.debugTouches : []
                delegate: Item {
                    readonly property real videoAspect: 1280 / 720
                    readonly property real displayAspect: videoArea.width / videoArea.height
                    readonly property real videoW: videoAspect > displayAspect ? videoArea.width : videoArea.height * videoAspect
                    readonly property real videoH: videoAspect > displayAspect ? videoArea.width / videoAspect : videoArea.height
                    readonly property real videoX0: (videoArea.width - videoW) / 2
                    readonly property real videoY0: (videoArea.height - videoH) / 2

                    x: videoX0 + modelData.x / 1280 * videoW - 15
                    y: videoY0 + modelData.y / 720 * videoH - 15
                    width: 30; height: 30
                    z: 100

                    Rectangle {
                        anchors.fill: parent
                        radius: 15
                        color: "transparent"
                        border.color: "#00FF00"
                        border.width: 2
                    }
                    Rectangle { anchors.centerIn: parent; width: 1; height: 20; color: "#00FF00" }
                    Rectangle { anchors.centerIn: parent; width: 20; height: 1; color: "#00FF00" }
                    Text {
                        anchors.top: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.x + "," + modelData.y
                        color: "#00FF00"
                        font.pixelSize: 10
                    }
                }
            }

            // Status overlay — hidden when projecting video
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 16
                visible: !androidAutoMenu.projecting

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: {
                        switch (AndroidAutoService.connectionState) {
                        case 0: return "Android Auto";
                        case 1: return "Waiting for Device";
                        case 2: return "Connecting...";
                        case 3: return "Android Auto Active";
                        default: return "Android Auto";
                        }
                    }
                    color: ThemeService.specialFontColor
                    font.pixelSize: 28
                    font.bold: true
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: AndroidAutoService.statusMessage
                    color: ThemeService.descriptionFontColor
                    font.pixelSize: 18
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    Layout.maximumWidth: videoArea.width * 0.8
                }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 12; height: 12; radius: 6
                    color: {
                        switch (AndroidAutoService.connectionState) {
                        case 0: return "#666666";
                        case 1: return "#FFA500";
                        case 2: return "#FFFF00";
                        case 3: return "#00FF00";
                        default: return "#666666";
                        }
                    }
                    SequentialAnimation on opacity {
                        running: AndroidAutoService.connectionState === 1 || AndroidAutoService.connectionState === 2
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 800 }
                        NumberAnimation { to: 1.0; duration: 800 }
                    }
                }
            }
        }

        // Sidebar — only when enabled and projecting
        Sidebar {
            id: sidebarPanel
            Layout.preferredWidth: androidAutoMenu.sidebarWidth
            Layout.fillHeight: true
            visible: androidAutoMenu.sidebarEnabled && androidAutoMenu.projecting
            position: androidAutoMenu.sidebarPosition
        }
    }

    // Bind the video sink to C++ decoder
    Binding {
        target: VideoDecoder
        property: "videoSink"
        value: videoOutput.videoSink
    }
}
