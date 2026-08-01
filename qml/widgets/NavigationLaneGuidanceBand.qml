import QtQuick

Rectangle {
    id: root

    property var laneModel: null

    implicitHeight: 112
    color: ThemeService.surfaceContainerLow

    Row {
        id: laneRow
        anchors.fill: parent

        Repeater {
            id: laneRepeater
            model: root.laneModel

            delegate: Item {
                id: laneItem

                required property int index
                required property var directions

                width: laneRow.width / Math.max(1, laneRepeater.count)
                height: laneRow.height

                NavigationLaneCompositeGlyph {
                    anchors.fill: parent
                    laneIndex: laneItem.index
                    directions: laneItem.directions
                }
            }
        }
    }

    Repeater {
        model: Math.max(0, laneRepeater.count - 1)

        delegate: Rectangle {
            required property int index

            objectName: "laneDivider"
            x: (root.width * (index + 1) / Math.max(1, laneRepeater.count))
               - width / 2
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: parent.height * 0.28
            color: ThemeService.outlineVariant
            opacity: 0.45
        }
    }
}
