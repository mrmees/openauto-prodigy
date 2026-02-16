import QtQuick
import QtQuick.Layouts

Rectangle {
    id: topBar

    color: ThemeController.barBackgroundColor

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        // Back button
        Rectangle {
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            Layout.alignment: Qt.AlignVCenter
            color: backMouse.containsMouse ? ThemeController.highlightColor : "transparent"
            radius: 8
            visible: ApplicationController.currentApplication !== 0  // Not on Launcher
            opacity: visible ? 1.0 : 0.0

            Text {
                anchors.centerIn: parent
                text: "\u25C0"  // Left-pointing triangle
                color: ThemeController.normalFontColor
                font.pixelSize: 22
            }

            MouseArea {
                id: backMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: ApplicationController.navigateBack()
            }
        }

        // Spacer when back button hidden
        Item {
            Layout.preferredWidth: 48
            visible: ApplicationController.currentApplication === 0
        }

        // Title
        Text {
            Layout.fillWidth: true
            text: ApplicationController.currentTitle
            color: ThemeController.specialFontColor
            font.pixelSize: 22
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        // Clock
        Clock {
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
