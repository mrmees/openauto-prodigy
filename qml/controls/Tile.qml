import QtQuick
import QtQuick.Layouts

Rectangle {
    id: tile

    property string tileName: ""
    property string tileIcon: ""
    property bool tileEnabled: true

    signal clicked()

    radius: 8
    color: mouseArea.containsMouse && tileEnabled
           ? ThemeService.highlightColor
           : ThemeService.controlBackgroundColor
    opacity: tileEnabled ? 1.0 : 0.5

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 8

        MaterialIcon {
            Layout.alignment: Qt.AlignHCenter
            icon: tile.tileIcon
            size: Math.min(tile.width, tile.height) * 0.35
            color: ThemeService.normalFontColor
            visible: tile.tileIcon !== ""
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: tile.tileName
            color: ThemeService.normalFontColor
            font.pixelSize: UiMetrics.fontSmall
            horizontalAlignment: Text.AlignHCenter
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: tile.tileEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            if (tile.tileEnabled)
                tile.clicked()
        }
    }
}
