import QtQuick
import QtQuick.Layouts
import QtMultimedia

Item {
    id: androidAutoMenu

    Component.onCompleted: ApplicationController.setTitle("Android Auto")

    // Video output — fills entire area when projecting
    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        visible: AndroidAutoService.connectionState === 3
        fillMode: VideoOutput.Stretch
    }

    // Multi-touch input — forward to phone via AA input channel
    MultiPointTouchArea {
        anchors.fill: videoOutput
        visible: videoOutput.visible
        minimumTouchPoints: 1
        maximumTouchPoints: 10

        onPressed: function(touchPoints) {
            for (var i = 0; i < touchPoints.length; i++) {
                var tp = touchPoints[i];
                var nx = Math.round(tp.x / width * 1024);
                var ny = Math.round(tp.y / height * 600);
                TouchHandler.sendMultiTouchEvent(nx, ny, tp.pointId, i === 0 ? 0 : 5);
            }
        }
        onReleased: function(touchPoints) {
            for (var i = 0; i < touchPoints.length; i++) {
                var tp = touchPoints[i];
                var nx = Math.round(tp.x / width * 1024);
                var ny = Math.round(tp.y / height * 600);
                TouchHandler.sendMultiTouchEvent(nx, ny, tp.pointId, i === 0 ? 1 : 6);
            }
        }
        onUpdated: function(touchPoints) {
            for (var i = 0; i < touchPoints.length; i++) {
                var tp = touchPoints[i];
                var nx = Math.round(tp.x / width * 1024);
                var ny = Math.round(tp.y / height * 600);
                TouchHandler.sendMultiTouchEvent(nx, ny, tp.pointId, 2);
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
