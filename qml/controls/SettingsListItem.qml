import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property string icon: ""
    property string label: ""

    signal clicked()

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    scale: mouseArea.pressed ? 0.97 : 1.0
    opacity: mouseArea.pressed ? 0.85 : 1.0
    Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginPage
        anchors.rightMargin: UiMetrics.marginPage
        spacing: UiMetrics.gap

        MaterialIcon {
            icon: root.icon
            size: UiMetrics.iconSize
            color: ThemeService.textPrimary
        }

        Text {
            text: root.label
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.textPrimary
            Layout.fillWidth: true
        }

        MaterialIcon {
            icon: "\ue5cc"
            size: UiMetrics.iconSmall
            color: ThemeService.textSecondary
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: UiMetrics.marginPage
        anchors.rightMargin: UiMetrics.marginPage
        height: 1
        color: ThemeService.outlineVariant
        opacity: 1.0
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
