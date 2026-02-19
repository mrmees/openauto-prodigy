import QtQuick
import QtQuick.Layouts
import QtMultimedia

Item {
    id: androidAutoMenu

    Component.onCompleted: ApplicationController.setTitle("Android Auto")

    // Black background for letterbox bars
    Rectangle {
        anchors.fill: parent
        color: "black"
        visible: AndroidAutoService.connectionState === 3
    }

    // Video output — fills entire area when projecting
    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        visible: AndroidAutoService.connectionState === 3
        fillMode: VideoOutput.PreserveAspectFit
    }

    // Touch input is handled by EvdevTouchReader in C++ — reads directly from
    // /dev/input/event4 for reliable multi-touch. No QML touch handling needed.

    // Debug touch overlay — shows green crosshair circles at AA touch coordinates
    // Accounts for letterboxing (video is PreserveAspectFit within display)
    Repeater {
        model: TouchHandler.debugOverlay ? TouchHandler.debugTouches : []
        delegate: Item {
            // Compute video area within display (same logic as C++ letterbox)
            readonly property real videoAspect: 1280 / 720
            readonly property real displayAspect: androidAutoMenu.width / androidAutoMenu.height
            readonly property real videoW: videoAspect > displayAspect ? androidAutoMenu.width : androidAutoMenu.height * videoAspect
            readonly property real videoH: videoAspect > displayAspect ? androidAutoMenu.width / videoAspect : androidAutoMenu.height
            readonly property real videoX0: (androidAutoMenu.width - videoW) / 2
            readonly property real videoY0: (androidAutoMenu.height - videoH) / 2

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
            // Crosshair
            Rectangle { anchors.centerIn: parent; width: 1; height: 20; color: "#00FF00" }
            Rectangle { anchors.centerIn: parent; width: 20; height: 1; color: "#00FF00" }
            // Coordinate label
            Text {
                anchors.top: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                text: modelData.x + "," + modelData.y
                color: "#00FF00"
                font.pixelSize: 10
            }
        }
    }

    // Bind the video sink to C++ decoder
    Binding {
        target: VideoDecoder
        property: "videoSink"
        value: videoOutput.videoSink
    }

    // Status overlay — hidden when projecting video
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16
        visible: AndroidAutoService.connectionState !== 3

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
            color: ThemeController.specialFontColor
            font.pixelSize: 28
            font.bold: true
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: AndroidAutoService.statusMessage
            color: ThemeController.descriptionFontColor
            font.pixelSize: 18
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.maximumWidth: parent.parent.width * 0.8
        }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 12
            height: 12
            radius: 6
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
