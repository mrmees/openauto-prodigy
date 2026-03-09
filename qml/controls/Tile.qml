import QtQuick
import QtQuick.Layouts

Rectangle {
    id: tile

    property string tileName: ""
    property string tileIcon: ""
    property bool tileEnabled: true

    signal clicked()

    radius: UiMetrics.radiusSmall
    color: ThemeService.primary

    scale: mouseArea.pressed ? 0.95 : 1.0
    opacity: mouseArea.pressed && tileEnabled ? 0.85 : (tileEnabled ? 1.0 : 0.5)
    Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: UiMetrics.spacing

        MaterialIcon {
            Layout.alignment: Qt.AlignHCenter
            icon: tile.tileIcon
            size: Math.min(tile.width, tile.height) * 0.35
            color: ThemeService.inverseOnSurface
            visible: tile.tileIcon !== ""
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: tile.tileName
            color: ThemeService.inverseOnSurface
            font.pixelSize: UiMetrics.fontHeading
            horizontalAlignment: Text.AlignHCenter
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        cursorShape: tile.tileEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            if (tile.tileEnabled)
                tile.clicked()
        }
    }
}
