import QtQuick
import QtQuick.Effects
import QtQuick.Layouts

Item {
    id: root

    property string icon: ""
    property string label: ""
    property bool flat: false

    signal clicked()

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    readonly property bool _isPressed: mouseArea.pressed

    scale: (!root.flat && _isPressed) ? 0.97 : 1.0
    Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

    // Background rectangle (source for MultiEffect shadow)
    Rectangle {
        id: bg
        anchors.fill: parent
        radius: UiMetrics.radiusSmall
        color: ThemeService.surfaceContainerLow
        border.width: 1
        border.color: ThemeService.outlineVariant
        layer.enabled: !root.flat
        visible: false
    }

    // Shadow effect (Level 2 resting, reduced on press)
    MultiEffect {
        id: shadow
        source: bg
        anchors.fill: bg
        visible: !root.flat
        shadowEnabled: !root.flat
        shadowColor: ThemeService.shadow
        shadowBlur: root._isPressed ? 0.35 : 0.65
        shadowVerticalOffset: root._isPressed ? 2 : 5
        shadowOpacity: root._isPressed ? 0.30 : 0.55
        shadowHorizontalOffset: 0
        shadowScale: 1.0
        autoPaddingEnabled: true

        Behavior on shadowBlur { NumberAnimation { duration: UiMetrics.animDurationFast } }
        Behavior on shadowVerticalOffset { NumberAnimation { duration: UiMetrics.animDurationFast } }
        Behavior on shadowOpacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
    }

    // State layer overlay
    Rectangle {
        anchors.fill: parent
        radius: UiMetrics.radiusSmall
        color: ThemeService.onSurface
        opacity: (!root.flat && root._isPressed) ? 0.10 : 0.0
        visible: !root.flat
        Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginPage
        anchors.rightMargin: UiMetrics.marginPage
        spacing: UiMetrics.gap

        MaterialIcon {
            icon: root.icon
            size: UiMetrics.iconSize
            color: ThemeService.onSurface
        }

        Text {
            text: root.label
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.onSurface
            Layout.fillWidth: true
        }

        MaterialIcon {
            icon: "\ue5cc"
            size: UiMetrics.iconSmall
            color: ThemeService.onSurfaceVariant
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
        visible: !root.flat
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
