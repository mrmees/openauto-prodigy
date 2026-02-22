import QtQuick
import QtQuick.Layouts

Rectangle {
    id: sidebar
    color: Qt.rgba(0, 0, 0, 0.85)

    property string position: "right"
    readonly property bool isHorizontal: position === "top" || position === "bottom"

    // Subtle border on the AA-facing edge
    Rectangle {
        color: ThemeService.barShadowColor
        // Vertical sidebar: 1px wide border on inner edge
        width: sidebar.isHorizontal ? parent.width : 1
        height: sidebar.isHorizontal ? 1 : parent.height
        anchors.left: position === "right" ? parent.left : undefined
        anchors.right: position === "left" ? parent.right : undefined
        anchors.top: position === "bottom" ? parent.top : undefined
        anchors.bottom: position === "top" ? parent.bottom : undefined
    }

    // Vertical layout (left/right sidebar)
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8
        visible: !sidebar.isHorizontal

        // Volume icon
        MaterialIcon {
            Layout.alignment: Qt.AlignHCenter
            icon: AudioService.masterVolume > 50 ? "\ue050"
                : AudioService.masterVolume > 0  ? "\ue04d"
                : "\ue04e"
            size: 24
            color: ThemeService.normalFontColor
        }

        // Volume level bar — vertical (bottom-up)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 6; height: parent.height; radius: 3
                color: ThemeService.barShadowColor
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                width: 6
                height: parent.height * AudioService.masterVolume / 100
                radius: 3
                color: ThemeService.specialFontColor
                Behavior on height { NumberAnimation { duration: 80 } }
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height * (1 - AudioService.masterVolume / 100) - height / 2
                width: 18; height: 18; radius: 9
                color: ThemeService.normalFontColor
                Behavior on y { NumberAnimation { duration: 80 } }
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: AudioService.masterVolume + "%"
            font.pixelSize: 12
            color: ThemeService.descriptionFontColor
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: ThemeService.barShadowColor }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue9b2"; size: 32
                color: ThemeService.specialFontColor
            }
        }
    }

    // Horizontal layout (top/bottom sidebar)
    RowLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 8
        visible: sidebar.isHorizontal

        // Volume icon
        MaterialIcon {
            Layout.alignment: Qt.AlignVCenter
            icon: AudioService.masterVolume > 50 ? "\ue050"
                : AudioService.masterVolume > 0  ? "\ue04d"
                : "\ue04e"
            size: 20
            color: ThemeService.normalFontColor
        }

        // Volume level bar — horizontal (left-to-right)
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width; height: 6; radius: 3
                color: ThemeService.barShadowColor
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                height: 6
                width: parent.width * AudioService.masterVolume / 100
                radius: 3
                color: ThemeService.specialFontColor
                Behavior on width { NumberAnimation { duration: 80 } }
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                x: parent.width * AudioService.masterVolume / 100 - width / 2
                width: 18; height: 18; radius: 9
                color: ThemeService.normalFontColor
                Behavior on x { NumberAnimation { duration: 80 } }
            }
        }

        Text {
            Layout.alignment: Qt.AlignVCenter
            text: AudioService.masterVolume + "%"
            font.pixelSize: 12
            color: ThemeService.descriptionFontColor
        }

        Rectangle { width: 1; Layout.fillHeight: true; color: ThemeService.barShadowColor }

        Item {
            Layout.fillHeight: true
            Layout.preferredWidth: 56
            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue9b2"; size: 28
                color: ThemeService.specialFontColor
            }
        }
    }
}
