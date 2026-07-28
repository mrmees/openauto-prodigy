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
                readonly property real glyphWidth:
                    Math.min(height * 0.58,
                             width / Math.max(1, directions.length))

                Rectangle {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 1
                    height: parent.height * 0.28
                    visible: index > 0
                    color: ThemeService.outlineVariant
                    opacity: 0.45
                }

                Row {
                    anchors.centerIn: parent
                    width: laneItem.glyphWidth * laneItem.directions.length
                    height: parent.height

                    Repeater {
                        model: laneItem.directions

                        delegate: NavigationLaneDirectionGlyph {
                            required property var modelData
                            readonly property var direction: modelData

                            width: laneItem.glyphWidth
                            height: parent.height
                            size: Math.min(width, height * 0.58)
                            shapeToken: direction.shape
                            recommended: direction.recommended
                            color: direction.recommended
                                   ? ThemeService.primary
                                   : ThemeService.onSurfaceVariant
                            opacity: direction.recommended ? 1.0 : 0.48
                        }
                    }
                }
            }
        }
    }
}
