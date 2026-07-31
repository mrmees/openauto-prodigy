import QtQuick

Item {
    id: root

    property string shapeToken: "unknown"
    property bool recommended: false
    property real size: 32
    property color color: "white"
    property string geometryVariant: "base"

    readonly property var presentation: presentationFor(shapeToken)
    readonly property string primitiveFamily: presentation.family
    readonly property int semanticAngle: presentation.angle
    readonly property bool mirrored: presentation.mirrored
    readonly property bool drawNeutralStem: primitiveFamily === "neutral"
    readonly property bool isFallback: presentation.fallback
    readonly property real anchorFraction:
        (primitiveFamily === "straight") ? 0.5
        : ((primitiveFamily === "sharp" || primitiveFamily === "u_turn")
           && geometryVariant === "short") ? 0.375
        : drawNeutralStem ? 0.0 : 0.25
    readonly property var primitiveGeometry:
        geometryFor(primitiveFamily, geometryVariant)
    readonly property real terminalDx:
        (mirrored ? -1 : 1)
        * (primitiveGeometry.tipX - primitiveGeometry.terminalX)
    readonly property real terminalDy:
        primitiveGeometry.tipY - primitiveGeometry.terminalY
    readonly property real headADx:
        (mirrored ? -1 : 1)
        * (primitiveGeometry.headAX - primitiveGeometry.tipX)
    readonly property real headADy:
        primitiveGeometry.headAY - primitiveGeometry.tipY
    readonly property real headBDx:
        (mirrored ? -1 : 1)
        * (primitiveGeometry.headBX - primitiveGeometry.tipX)
    readonly property real headBDy:
        primitiveGeometry.headBY - primitiveGeometry.tipY

    implicitWidth: size
    implicitHeight: size

    function entry(family, angle, mirrored, fallback) {
        return {
            "family": family,
            "angle": angle,
            "mirrored": mirrored || false,
            "fallback": fallback || false
        }
    }

    function presentationFor(token) {
        switch (token) {
        case "unknown": return entry("neutral", -1)
        case "straight": return entry("straight", 0)
        case "slight_left": return entry("slight", 45, true)
        case "slight_right": return entry("slight", 45)
        case "normal_left": return entry("normal", 90, true)
        case "normal_right": return entry("normal", 90)
        case "sharp_left": return entry("sharp", 135, true)
        case "sharp_right": return entry("sharp", 135)
        case "u_turn_left": return entry("u_turn", 180, true)
        case "u_turn_right": return entry("u_turn", 180)
        default: return entry("neutral", -1, false, true)
        }
    }

    // Original Prodigy 24x24 geometry. Each family has an explicit terminal
    // segment and a 70-degree open chevron bisected by that segment.
    function geometryFor(family, variant) {
        const tall = variant === "tall"
        const shortGeometry = variant === "short"
        switch (family) {
        case "straight": {
            const tipY = tall ? 2.0 : 4.0
            return {"terminalX": 12.0, "terminalY": tipY + 4.0,
                    "tipX": 12.0, "tipY": tipY,
                    "headAX": 9.246858, "headAY": tipY + 3.931962,
                    "headBX": 14.753142, "headBY": tipY + 3.931962}
        }
        case "slight": {
            const tipX = tall ? 20.0 : 18.5
            const tipY = tall ? 3.0 : 5.5
            return {"terminalX": tipX - 2.828427,
                    "terminalY": tipY + 2.828427,
                    "tipX": tipX, "tipY": tipY,
                    "headAX": tipX - 0.833511,
                    "headAY": tipY + 4.727078,
                    "headBX": tipX - 4.727078,
                    "headBY": tipY + 0.833511}
        }
        case "normal": {
            const tipY = shortGeometry ? 14.0 : 10.0
            return {"terminalX": 17.0, "terminalY": tipY,
                    "tipX": 21.0, "tipY": tipY,
                    "headAX": 17.068038,
                    "headAY": tipY + 2.753142,
                    "headBX": 17.068038,
                    "headBY": tipY - 2.753142}
        }
        case "sharp": {
            const tipY = shortGeometry ? 19.0 : 16.5
            return {"terminalX": 17.171573,
                    "terminalY": tipY - 2.828427,
                    "tipX": 20.0, "tipY": tipY,
                    "headAX": 15.272922,
                    "headAY": tipY - 0.833511,
                    "headBX": 19.166489,
                    "headBY": tipY - 4.727078}
        }
        case "u_turn": {
            const tipY = shortGeometry ? 20.0 : 18.0
            return {"terminalX": 20.0, "terminalY": tipY - 4.0,
                    "tipX": 20.0, "tipY": tipY,
                    "headAX": 17.246858,
                    "headAY": tipY - 3.931962,
                    "headBX": 22.753142,
                    "headBY": tipY - 3.931962}
        }
        default:
            return {"terminalX": 12.0, "terminalY": 7.0,
                    "tipX": 12.0, "tipY": 7.0,
                    "headAX": 12.0, "headAY": 7.0,
                    "headBX": 12.0, "headBY": 7.0}
        }
    }

    function paintPrimitive(context) {
        const geometry = primitiveGeometry

        context.beginPath()
        switch (primitiveFamily) {
        case "straight":
            context.moveTo(12, 22)
            context.lineTo(geometry.terminalX, geometry.terminalY)
            break
        case "slight":
            context.moveTo(12, 22)
            context.lineTo(12, 13.5)
            context.bezierCurveTo(12, 9.5,
                                  geometry.terminalX - 2.0,
                                  geometry.terminalY + 2.0,
                                  geometry.terminalX, geometry.terminalY)
            break
        case "normal":
            context.moveTo(12, 22)
            context.lineTo(12, geometry.terminalY + 2.5)
            context.quadraticCurveTo(12, geometry.terminalY,
                                     geometry.terminalX - 2.0,
                                     geometry.terminalY)
            context.lineTo(geometry.terminalX, geometry.terminalY)
            break
        case "sharp":
            context.moveTo(12, 22)
            context.lineTo(12, geometry.terminalY - 4.0)
            context.bezierCurveTo(12, geometry.terminalY - 1.0,
                                  geometry.terminalX - 2.0,
                                  geometry.terminalY - 2.0,
                                  geometry.terminalX, geometry.terminalY)
            break
        case "u_turn":
            context.moveTo(12, 22)
            context.lineTo(12, geometryVariant === "short" ? 13.0 : 8.0)
            context.bezierCurveTo(12, geometryVariant === "short" ? 8.0 : 3.0,
                                  20, geometryVariant === "short" ? 8.0 : 3.0,
                                  geometry.terminalX,
                                  geometry.terminalY - 2.0)
            context.lineTo(geometry.terminalX, geometry.terminalY)
            break
        default:
            context.moveTo(12, 22)
            context.lineTo(12, 7)
            context.stroke()
            return
        }

        context.lineTo(geometry.tipX, geometry.tipY)
        context.moveTo(geometry.headAX, geometry.headAY)
        context.lineTo(geometry.tipX, geometry.tipY)
        context.lineTo(geometry.headBX, geometry.headBY)
        context.stroke()
    }

    Canvas {
        id: directionCanvas
        objectName: "laneDirectionGlyph"
        anchors.fill: parent
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
            context.lineWidth = root.recommended ? 3.4 : 3.0
            context.lineCap = "round"
            context.lineJoin = "round"
            root.paintPrimitive(context)
            context.restore()
        }
    }

    onShapeTokenChanged: directionCanvas.requestPaint()
    onRecommendedChanged: directionCanvas.requestPaint()
    onColorChanged: directionCanvas.requestPaint()
    onGeometryVariantChanged: directionCanvas.requestPaint()
    onWidthChanged: directionCanvas.requestPaint()
    onHeightChanged: directionCanvas.requestPaint()
}
