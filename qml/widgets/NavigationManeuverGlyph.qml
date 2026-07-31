import QtQuick

Item {
    id: root

    property int maneuverType: 0
    property real size: 48
    property color color: "white"

    readonly property var presentation: presentationFor(maneuverType)
    readonly property bool usesLanePrimitive: presentation.lane
    readonly property string laneShapeToken: presentation.token
    readonly property string specialFamily: presentation.family
    readonly property bool hasSpecialOverlay: specialFamily.length > 0
    readonly property bool mirrored: presentation.mirrored
    readonly property bool isFallback: presentation.fallback
    readonly property real specialStrokeWidth: 3.4
    readonly property string specialLineCap: "round"
    readonly property string specialLineJoin: "round"

    implicitWidth: size
    implicitHeight: size

    function entry(lane, token, family, mirrored, fallback) {
        return {
            "lane": lane,
            "token": token || "",
            "family": family || "",
            "mirrored": mirrored || false,
            "fallback": fallback || false
        }
    }

    function presentationFor(type) {
        switch (type) {
        case 0: return entry(false, "", "unknown")
        case 1: return entry(false, "", "depart")
        case 2: return entry(true, "straight")
        case 3: return entry(false, "", "keep", true)
        case 4: return entry(false, "", "keep")
        case 5: return entry(true, "slight_left")
        case 6: return entry(true, "slight_right")
        case 7: return entry(true, "normal_left")
        case 8: return entry(true, "normal_right")
        case 9: return entry(true, "sharp_left")
        case 10: return entry(true, "sharp_right")
        case 11: return entry(true, "u_turn_left")
        case 12: return entry(true, "u_turn_right")
        case 13: return entry(true, "slight_left")
        case 14: return entry(true, "slight_right")
        case 15: return entry(true, "normal_left")
        case 16: return entry(true, "normal_right")
        case 17: return entry(true, "sharp_left")
        case 18: return entry(true, "sharp_right")
        case 19: return entry(true, "u_turn_left")
        case 20: return entry(true, "u_turn_right")
        case 21: return entry(false, "", "off_ramp_slight", true)
        case 22: return entry(false, "", "off_ramp_slight")
        case 23: return entry(false, "", "off_ramp_normal", true)
        case 24: return entry(false, "", "off_ramp_normal")
        case 25: return entry(false, "", "fork", true)
        case 26: return entry(false, "", "fork")
        case 27: return entry(false, "", "merge", true)
        case 28: return entry(false, "", "merge")
        case 29: return entry(false, "", "merge_unspecified")
        case 32: return entry(false, "", "roundabout_enter_exit")
        case 33: return entry(false, "", "roundabout_enter_exit_angle")
        case 34: return entry(false, "", "roundabout_enter_exit", true)
        case 35: return entry(false, "", "roundabout_enter_exit_angle", true)
        case 36: return entry(true, "straight")
        case 37: return entry(false, "", "ferry_boat")
        case 38: return entry(false, "", "ferry_train")
        case 39: return entry(false, "", "destination")
        case 40: return entry(false, "", "destination")
        case 41: return entry(false, "", "destination_side", true)
        case 42: return entry(false, "", "destination_side")
        case 43: return entry(false, "", "roundabout_enter")
        case 44: return entry(false, "", "roundabout_exit")
        case 45: return entry(false, "", "roundabout_enter", true)
        case 46: return entry(false, "", "roundabout_exit", true)
        case 47: return entry(true, "normal_left", "ferry_boat_marker", true)
        case 48: return entry(true, "normal_right", "ferry_boat_marker")
        case 49: return entry(true, "normal_left", "ferry_train_marker", true)
        case 50: return entry(true, "normal_right", "ferry_train_marker")
        default: return entry(false, "", "unknown", false, true)
        }
    }

    function strokePath(context, points) {
        context.beginPath()
        context.moveTo(points[0], points[1])
        for (let index = 2; index < points.length; index += 2)
            context.lineTo(points[index], points[index + 1])
        context.stroke()
    }

    function arrowHead(context, tipX, tipY, fromX, fromY, length) {
        const direction = Math.atan2(tipY - fromY, tipX - fromX)
        const back = direction + Math.PI
        const spread = 35 * Math.PI / 180
        strokePath(context, [
            tipX + length * Math.cos(back - spread),
            tipY + length * Math.sin(back - spread),
            tipX, tipY,
            tipX + length * Math.cos(back + spread),
            tipY + length * Math.sin(back + spread)
        ])
    }

    function drawRouteArrow(context, points, headLength) {
        strokePath(context, points)
        const end = points.length
        arrowHead(context, points[end - 2], points[end - 1],
                  points[end - 4], points[end - 3], headLength || 4.0)
    }

    function drawBoat(context, offsetX, offsetY, scale) {
        context.beginPath()
        context.moveTo(offsetX + 2 * scale, offsetY + 5 * scale)
        context.lineTo(offsetX + 12 * scale, offsetY + 5 * scale)
        context.lineTo(offsetX + 10 * scale, offsetY + 9 * scale)
        context.lineTo(offsetX + 4 * scale, offsetY + 9 * scale)
        context.closePath()
        context.stroke()
        strokePath(context, [offsetX + 5 * scale, offsetY + 5 * scale,
                             offsetX + 5 * scale, offsetY + 1 * scale,
                             offsetX + 9 * scale, offsetY + 5 * scale])
        context.beginPath()
        context.moveTo(offsetX + 2 * scale, offsetY + 11 * scale)
        context.quadraticCurveTo(offsetX + 4 * scale, offsetY + 9 * scale,
                                 offsetX + 6 * scale, offsetY + 11 * scale)
        context.quadraticCurveTo(offsetX + 8 * scale, offsetY + 13 * scale,
                                 offsetX + 10 * scale, offsetY + 11 * scale)
        context.stroke()
    }

    function drawTrain(context, offsetX, offsetY, scale) {
        context.beginPath()
        context.rect(offsetX + 3 * scale, offsetY + 1 * scale,
                     8 * scale, 10 * scale)
        context.stroke()
        strokePath(context, [offsetX + 5 * scale, offsetY + 4 * scale,
                             offsetX + 9 * scale, offsetY + 4 * scale])
        context.beginPath()
        context.arc(offsetX + 5 * scale, offsetY + 9 * scale,
                    0.65 * scale, 0, Math.PI * 2)
        context.arc(offsetX + 9 * scale, offsetY + 9 * scale,
                    0.65 * scale, 0, Math.PI * 2)
        context.stroke()
        strokePath(context, [offsetX + 4 * scale, offsetY + 13 * scale,
                             offsetX + 6 * scale, offsetY + 11 * scale,
                             offsetX + 8 * scale, offsetY + 11 * scale,
                             offsetX + 10 * scale, offsetY + 13 * scale])
    }

    function drawDestination(context, x, y, scale) {
        context.beginPath()
        context.moveTo(x, y + 8 * scale)
        context.lineTo(x, y)
        context.lineTo(x + 6 * scale, y + 2 * scale)
        context.lineTo(x, y + 4 * scale)
        context.stroke()
    }

    function drawRoundabout(context, family) {
        const angleExit = family === "roundabout_enter_exit_angle"
        const enterOnly = family === "roundabout_enter"
        const exitOnly = family === "roundabout_exit"
        context.beginPath()
        context.arc(12, 11, 5, Math.PI * 0.30, Math.PI * 1.90, false)
        context.stroke()
        if (!exitOnly)
            strokePath(context, [12, 22, 12, 16])
        if (!enterOnly) {
            if (angleExit)
                drawRouteArrow(context, [16, 8, 20, 4], 3.2)
            else
                drawRouteArrow(context, [17, 11, 22, 11], 3.2)
        } else {
            arrowHead(context, 9.3, 6.8, 12.5, 6.0, 3.2)
        }
        if (exitOnly)
            strokePath(context, [12, 16, 12, 22])
    }

    function paintSpecial(context) {
        switch (root.specialFamily) {
        case "unknown":
            context.beginPath()
            context.arc(12, 12, 7, 0, Math.PI * 2)
            context.stroke()
            strokePath(context, [12, 8, 12, 13])
            context.beginPath()
            context.arc(12, 17, 0.75, 0, Math.PI * 2)
            context.fill()
            break
        case "depart":
            context.beginPath()
            context.arc(12, 20, 2, 0, Math.PI * 2)
            context.stroke()
            drawRouteArrow(context, [12, 18, 12, 4], 4)
            break
        case "keep":
        case "fork":
            strokePath(context, [12, 22, 12, 13, 7, 6])
            drawRouteArrow(context, [12, 13, 17, 6], 3.8)
            break
        case "off_ramp_slight":
            strokePath(context, [10, 22, 10, 4])
            drawRouteArrow(context, [10, 14, 15, 11, 20, 6], 3.8)
            break
        case "off_ramp_normal":
            strokePath(context, [10, 22, 10, 4])
            drawRouteArrow(context, [10, 14, 14, 10, 21, 10], 3.8)
            break
        case "merge":
            drawRouteArrow(context, [11, 22, 11, 4], 3.8)
            strokePath(context, [21, 18, 16, 14, 11, 12])
            break
        case "merge_unspecified":
            drawRouteArrow(context, [12, 22, 12, 4], 3.8)
            strokePath(context, [4, 17, 9, 13, 12, 12])
            strokePath(context, [20, 17, 15, 13, 12, 12])
            break
        case "roundabout_enter_exit":
        case "roundabout_enter_exit_angle":
        case "roundabout_enter":
        case "roundabout_exit":
            drawRoundabout(context, root.specialFamily)
            break
        case "ferry_boat":
            drawBoat(context, 5, 5, 1)
            break
        case "ferry_train":
            drawTrain(context, 5, 4, 1)
            break
        case "destination":
            drawDestination(context, 9, 5, 1)
            strokePath(context, [9, 19, 15, 19])
            break
        case "destination_side":
            context.beginPath()
            context.moveTo(6, 21)
            context.bezierCurveTo(6, 15, 11, 13, 15, 11)
            context.stroke()
            drawDestination(context, 14, 3, 0.82)
            break
        case "ferry_boat_marker":
            drawBoat(context, 11, 11, 0.65)
            break
        case "ferry_train_marker":
            drawTrain(context, 11, 10, 0.65)
            break
        }
    }

    NavigationLaneDirectionGlyph {
        objectName: "maneuverLanePrimitive"
        anchors.fill: parent
        visible: root.usesLanePrimitive
        size: Math.min(width, height)
        shapeToken: root.laneShapeToken
        recommended: true
        geometryVariant: "base"
        opticalScale: 1.0
        color: root.color
    }

    Canvas {
        id: specialCanvas
        objectName: "maneuverSpecialCanvas"
        anchors.fill: parent
        visible: root.hasSpecialOverlay
        antialiasing: true

        onPaint: {
            const context = getContext("2d")
            context.reset()
            context.clearRect(0, 0, width, height)
            const scale = Math.min(width, height) / 24.0
            context.save()
            context.translate((width - 24 * scale) / 2,
                              (height - 24 * scale) / 2)
            context.scale(scale, scale)
            if (root.mirrored) {
                context.translate(24, 0)
                context.scale(-1, 1)
            }
            context.strokeStyle = root.color
            context.fillStyle = root.color
            context.lineWidth = root.specialStrokeWidth
            context.lineCap = root.specialLineCap
            context.lineJoin = root.specialLineJoin
            root.paintSpecial(context)
            context.restore()
        }
    }

    onManeuverTypeChanged: specialCanvas.requestPaint()
    onColorChanged: specialCanvas.requestPaint()
    onWidthChanged: specialCanvas.requestPaint()
    onHeightChanged: specialCanvas.requestPaint()
}
