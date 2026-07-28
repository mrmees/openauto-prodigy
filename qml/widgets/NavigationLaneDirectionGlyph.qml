import QtQuick

Item {
    id: root

    property string shapeToken: "unknown"
    property bool recommended: false
    property real size: 32
    property color color: "white"

    readonly property string glyph: presentation.glyph
    readonly property bool drawNeutralStem: presentation.neutralStem
    readonly property bool isFallback: presentation.fallback

    readonly property string straight: "\ueb95"
    readonly property string slightRight: "\ueb9a"
    readonly property string uTurnLeft: "\ueba1"
    readonly property string uTurnRight: "\ueba2"
    readonly property string slightLeft: "\ueba4"
    readonly property string turnLeft: "\ueba6"
    readonly property string sharpLeft: "\ueba7"
    readonly property string sharpRight: "\uebaa"
    readonly property string turnRight: "\uebab"

    readonly property var presentation: presentationFor(shapeToken)

    implicitWidth: size
    implicitHeight: size

    function entry(icon, neutralStem, fallback) {
        return {
            "glyph": icon,
            "neutralStem": neutralStem || false,
            "fallback": fallback || false
        }
    }

    function presentationFor(token) {
        switch (token) {
        case "unknown": return entry("", true)
        case "straight": return entry(straight)
        case "slight_left": return entry(slightLeft)
        case "slight_right": return entry(slightRight)
        case "normal_left": return entry(turnLeft)
        case "normal_right": return entry(turnRight)
        case "sharp_left": return entry(sharpLeft)
        case "sharp_right": return entry(sharpRight)
        case "u_turn_left": return entry(uTurnLeft)
        case "u_turn_right": return entry(uTurnRight)
        default: return entry("", true, true)
        }
    }

    MaterialIcon {
        objectName: "laneDirectionGlyph"
        anchors.centerIn: parent
        visible: !root.drawNeutralStem
        icon: root.glyph
        size: root.size
        color: root.color
        weight: root.recommended ? 700 : 400
    }

    Rectangle {
        objectName: "laneNeutralStem"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: Math.max(2, root.size * 0.1)
        height: root.size * 0.72
        radius: width / 2
        visible: root.drawNeutralStem
        color: root.color
    }
}
