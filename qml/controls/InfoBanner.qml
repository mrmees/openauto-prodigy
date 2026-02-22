import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string text: "Restart required to apply changes"
    property bool shown: false

    Layout.fillWidth: true
    implicitHeight: shown ? bannerContent.implicitHeight + UiMetrics.gap * 2 : 0
    clip: true

    Behavior on implicitHeight {
        NumberAnimation { duration: 200 }
    }

    Rectangle {
        id: bannerContent
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: UiMetrics.gap
        implicitHeight: bannerRow.implicitHeight + UiMetrics.gap * 2
        radius: UiMetrics.gap
        color: ThemeService.controlBoxBackgroundColor

        RowLayout {
            id: bannerRow
            anchors.fill: parent
            anchors.margins: UiMetrics.gap
            spacing: UiMetrics.gap

            MaterialIcon {
                icon: "\ue88e"
                size: UiMetrics.iconSmall
                color: ThemeService.highlightColor
            }

            Text {
                text: root.text
                font.pixelSize: UiMetrics.fontSmall
                font.italic: true
                color: ThemeService.normalFontColor
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
}
