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

    // Build list of ALL active touch locations (in AA coordinates) for protocol messages
    function allActiveTouches() {
        var pts = [];
        for (var i = 0; i < touchPointModel.count; i++) {
            var p = touchPointModel.get(i);
            pts.push({ x: Math.round(p.ptX / width * 1024),
                        y: Math.round(p.ptY / height * 600),
                        pointerId: p.ptId });
        }
        return pts;
    }

    // Multi-touch input — forward to phone via AA input channel
    // AA protocol requires ALL active touch locations in every message
    MultiPointTouchArea {
        anchors.fill: videoOutput
        visible: videoOutput.visible
        minimumTouchPoints: 1
        maximumTouchPoints: 10

        onPressed: function(touchPoints) {
            for (var i = 0; i < touchPoints.length; i++) {
                var tp = touchPoints[i];
                // First finger = DOWN(0), additional = POINTER_DOWN(5)
                var action = (touchPointModel.count === 0 && i === 0) ? 0 : 5;
                // Add to model FIRST so it's included in allActiveTouches
                touchPointModel.append({ ptId: tp.pointId, ptX: tp.x, ptY: tp.y });
                TouchHandler.sendBatchTouchEvent(allActiveTouches(), tp.pointId, action);
            }
        }
        onReleased: function(touchPoints) {
            for (var i = 0; i < touchPoints.length; i++) {
                var tp = touchPoints[i];
                // Update position of released point in model
                for (var j = 0; j < touchPointModel.count; j++) {
                    if (touchPointModel.get(j).ptId === tp.pointId) {
                        touchPointModel.set(j, { ptId: tp.pointId, ptX: tp.x, ptY: tp.y });
                        break;
                    }
                }
                var remaining = touchPointModel.count - 1;
                var action = (remaining === 0 && i === touchPoints.length - 1) ? 1 : 6;
                // Send with ALL touches INCLUDING the one being released
                TouchHandler.sendBatchTouchEvent(allActiveTouches(), tp.pointId, action);
                // NOW remove from model
                for (var j = touchPointModel.count - 1; j >= 0; j--) {
                    if (touchPointModel.get(j).ptId === tp.pointId)
                        touchPointModel.remove(j);
                }
            }
        }
        onUpdated: function(touchPoints) {
            // Update all moved positions in model first
            for (var i = 0; i < touchPoints.length; i++) {
                var tp = touchPoints[i];
                for (var j = 0; j < touchPointModel.count; j++) {
                    if (touchPointModel.get(j).ptId === tp.pointId) {
                        touchPointModel.set(j, { ptId: tp.pointId, ptX: tp.x, ptY: tp.y });
                        break;
                    }
                }
            }
            // Send ONE move event with ALL active touches
            if (touchPointModel.count > 0)
                TouchHandler.sendBatchTouchEvent(allActiveTouches(), touchPoints[0].pointId, 2);
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
