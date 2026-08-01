import QtQuick

Item {
    id: root
    objectName: "laneComposite"

    property int laneIndex: -1
    property var directions: []
    readonly property int directionCount: directions ? directions.length : 0
    readonly property bool compound: directionCount > 1
    readonly property real opticalScale: compound ? 0.90 : 1.0

    function geometryVariantFor(direction) {
        if (!compound)
            return "base"
        switch (direction.shape) {
        case "straight":
        case "slight_left":
        case "slight_right":
            return "tall"
        case "normal_left":
        case "normal_right":
        case "sharp_left":
        case "sharp_right":
        case "u_turn_left":
        case "u_turn_right":
            return "short"
        default:
            return "base"
        }
    }

    Repeater {
        model: root.directions

        delegate: NavigationLaneDirectionGlyph {
            required property int index
            required property var modelData
            readonly property var direction: modelData

            objectName: "laneDirectionPrimitive"
            anchors.fill: parent
            size: Math.min(width, height)
            shapeToken: direction.shape
            recommended: direction.recommended
            geometryVariant: root.geometryVariantFor(direction)
            opticalScale: root.opticalScale
            color: direction.recommended
                   ? ThemeService.primary
                   : ThemeService.onSurfaceVariant
            opacity: direction.recommended ? 1.0 : 0.48
            z: direction.recommended ? 100 + index : index
        }
    }
}
