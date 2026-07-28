import QtQuick

Item {
    id: root

    property int maneuverType: 0
    property real size: 48
    property color color: "white"

    readonly property string primaryGlyph: presentation.primary
    readonly property string badgeGlyph: presentation.badge
    readonly property bool mirrorPrimary: presentation.mirror
    readonly property bool isFallback: presentation.fallback

    readonly property string genericNavigation: "\ue55d"
    readonly property string depart: "\ue569"
    readonly property string straight: "\ueb95"
    readonly property string rampRight: "\ueb96"
    readonly property string merge: "\ueb98"
    readonly property string roundaboutLeft: "\ueb99"
    readonly property string slightRight: "\ueb9a"
    readonly property string rampLeft: "\ueb9c"
    readonly property string forkLeft: "\ueba0"
    readonly property string uTurnLeft: "\ueba1"
    readonly property string uTurnRight: "\ueba2"
    readonly property string roundaboutRight: "\ueba3"
    readonly property string slightLeft: "\ueba4"
    readonly property string turnLeft: "\ueba6"
    readonly property string sharpLeft: "\ueba7"
    readonly property string sharpRight: "\uebaa"
    readonly property string turnRight: "\uebab"
    readonly property string forkRight: "\uebac"
    readonly property string destination: "\ue153"
    readonly property string ferryBoat: "\ue532"
    readonly property string ferryTrain: "\ue570"

    readonly property var presentation: presentationFor(maneuverType)

    implicitWidth: size
    implicitHeight: size

    function entry(primary, badge, mirror, fallback) {
        return {
            "primary": primary,
            "badge": badge || "",
            "mirror": mirror || false,
            "fallback": fallback || false
        }
    }

    function presentationFor(type) {
        switch (type) {
        case 0: return entry(genericNavigation)
        case 1: return entry(depart)
        case 2: return entry(straight)
        case 3: return entry(slightLeft)
        case 4: return entry(slightRight)
        case 5: return entry(slightLeft)
        case 6: return entry(slightRight)
        case 7: return entry(turnLeft)
        case 8: return entry(turnRight)
        case 9: return entry(sharpLeft)
        case 10: return entry(sharpRight)
        case 11: return entry(uTurnLeft)
        case 12: return entry(uTurnRight)
        case 13: return entry(rampLeft)
        case 14: return entry(rampRight)
        case 15: return entry(rampLeft)
        case 16: return entry(rampRight)
        case 17: return entry(rampLeft)
        case 18: return entry(rampRight)
        case 19: return entry(uTurnLeft, rampLeft)
        case 20: return entry(uTurnRight, rampRight)
        case 21: return entry(rampLeft)
        case 22: return entry(rampRight)
        case 23: return entry(rampLeft)
        case 24: return entry(rampRight)
        case 25: return entry(forkLeft)
        case 26: return entry(forkRight)
        case 27: return entry(merge, "", true)
        case 28: return entry(merge)
        case 29: return entry(merge)
        case 32: return entry(roundaboutRight)
        case 33: return entry(roundaboutRight)
        case 34: return entry(roundaboutLeft)
        case 35: return entry(roundaboutLeft)
        case 36: return entry(straight)
        case 37: return entry(ferryBoat)
        case 38: return entry(ferryTrain)
        case 39: return entry(destination)
        case 40: return entry(destination)
        case 41: return entry(turnLeft, destination)
        case 42: return entry(turnRight, destination)
        case 43: return entry(roundaboutRight)
        case 44: return entry(roundaboutRight)
        case 45: return entry(roundaboutLeft)
        case 46: return entry(roundaboutLeft)
        case 47: return entry(turnLeft, ferryBoat)
        case 48: return entry(turnRight, ferryBoat)
        case 49: return entry(turnLeft, ferryTrain)
        case 50: return entry(turnRight, ferryTrain)
        default: return entry(genericNavigation, "", false, true)
        }
    }

    MaterialIcon {
        id: primary
        objectName: "maneuverPrimaryGlyph"
        anchors.centerIn: parent
        icon: root.primaryGlyph
        size: root.size
        color: root.color

        transform: Scale {
            origin.x: primary.width / 2
            xScale: root.mirrorPrimary ? -1 : 1
        }
    }

    MaterialIcon {
        objectName: "maneuverBadgeGlyph"
        visible: root.badgeGlyph.length > 0
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        icon: root.badgeGlyph
        size: root.size * 0.38
        color: root.color
        weight: 600
    }
}
