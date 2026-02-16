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
           ? ThemeController.highlightColor
           : ThemeController.controlBackgroundColor
    opacity: tileEnabled ? 1.0 : 0.5

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 8

        Image {
            Layout.alignment: Qt.AlignHCenter
            source: tile.tileIcon
            sourceSize.width: tile.width * 0.4
            sourceSize.height: tile.height * 0.4
            fillMode: Image.PreserveAspectFit
            smooth: true
            visible: tile.tileIcon !== ""
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: tile.tileName
            color: ThemeController.normalFontColor
            font.pixelSize: 14
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
