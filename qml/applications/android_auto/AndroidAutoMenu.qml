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

    // Touch point visualization model
    ListModel { id: touchPointModel }

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
                // First finger ever = DOWN(0), additional fingers = POINTER_DOWN(5)
                var action = (touchPointModel.count === 0 && i === 0) ? 0 : 5;
                TouchHandler.sendMultiTouchEvent(nx, ny, tp.pointId, action);
                touchPointModel.append({ ptId: tp.pointId, ptX: tp.x, ptY: tp.y });
            }
        }
        onReleased: function(touchPoints) {
            for (var i = 0; i < touchPoints.length; i++) {
                var tp = touchPoints[i];
                var nx = Math.round(tp.x / width * 1024);
                var ny = Math.round(tp.y / height * 600);
                // Last finger leaving = UP(1), others = POINTER_UP(6)
                var remaining = touchPointModel.count - 1;  // after removing this one
                var action = (remaining === 0 && i === touchPoints.length - 1) ? 1 : 6;
                TouchHandler.sendMultiTouchEvent(nx, ny, tp.pointId, action);
                for (var j = touchPointModel.count - 1; j >= 0; j--) {
                    if (touchPointModel.get(j).ptId === tp.pointId)
                        touchPointModel.remove(j);
                }
            }
        }
        onUpdated: function(touchPoints) {
            for (var i = 0; i < touchPoints.length; i++) {
                var tp = touchPoints[i];
                var nx = Math.round(tp.x / width * 1024);
                var ny = Math.round(tp.y / height * 600);
                TouchHandler.sendMultiTouchEvent(nx, ny, tp.pointId, 2);
                for (var j = 0; j < touchPointModel.count; j++) {
                    if (touchPointModel.get(j).ptId === tp.pointId) {
                        touchPointModel.set(j, { ptId: tp.pointId, ptX: tp.x, ptY: tp.y });
                        break;
                    }
                }
            }
        }
    }

    // Touch debug overlay — shows circles where touches are detected
    Repeater {
        model: touchPointModel
        delegate: Rectangle {
            x: model.ptX - 25
            y: model.ptY - 25
            width: 50
            height: 50
            radius: 25
            color: "transparent"
            border.color: "#00FF00"
            border.width: 3
            opacity: 0.8

            // Crosshair
            Rectangle { anchors.centerIn: parent; width: 1; height: 20; color: "#00FF00"; opacity: 0.6 }
            Rectangle { anchors.centerIn: parent; width: 20; height: 1; color: "#00FF00"; opacity: 0.6 }

            // Coordinate label
            Text {
                anchors.top: parent.bottom
                anchors.topMargin: 4
                anchors.horizontalCenter: parent.horizontalCenter
                text: Math.round(model.ptX / androidAutoMenu.width * 1024) + "," + Math.round(model.ptY / androidAutoMenu.height * 600)
                color: "#00FF00"
                font.pixelSize: 10
                font.family: "monospace"
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
