import QtQuick
import QtQuick.Effects
import QtQuick.Layouts

Item {
    id: tile

    property string tileName: ""
    property string tileIcon: ""
    property bool tileEnabled: true

    signal clicked()

    readonly property bool _isPressed: mouseArea.pressed && tileEnabled

    scale: _isPressed ? 0.95 : 1.0
    Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

    // Background rectangle (source for MultiEffect shadow)
    Rectangle {
        id: bg
        anchors.fill: parent
        radius: UiMetrics.radiusSmall
        color: tile._isPressed ? ThemeService.primaryContainer : ThemeService.primary
        border.width: 1
        border.color: ThemeService.outlineVariant
        opacity: tile.tileEnabled ? 1.0 : 0.5
        layer.enabled: true
        visible: false
    }

    // Shadow effect (Level 2 resting, reduced on press)
    MultiEffect {
        id: shadow
        source: bg
        anchors.fill: bg
        shadowEnabled: true
        shadowColor: ThemeService.shadow
        shadowBlur: tile._isPressed ? 0.35 : 0.65
        shadowVerticalOffset: tile._isPressed ? 2 : 5
        shadowOpacity: tile._isPressed ? 0.30 : 0.55
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
        opacity: tile._isPressed ? 0.10 : 0.0
        Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
    }

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
