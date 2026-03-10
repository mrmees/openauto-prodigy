import QtQuick
import QtQuick.Effects
import QtQuick.Layouts

Item {
    id: root

    property string text: ""
    property string iconCode: ""
    property color buttonColor: ThemeService.primary
    property color pressedColor: ThemeService.primaryContainer
    property color textColor: ThemeService.onPrimary
    property color pressedTextColor: ThemeService.onPrimaryContainer
    property real buttonScale: 0.97
    property bool buttonEnabled: true
    property real buttonRadius: UiMetrics.radiusSmall
    property int elevation: 2

    signal clicked()
    signal pressAndHold()

    implicitWidth: contentRow.implicitWidth + UiMetrics.spacing * 4
    implicitHeight: contentRow.implicitHeight + UiMetrics.spacing * 2

    // Shadow elevation lookup
    readonly property var _shadowParams: {
        switch (elevation) {
        case 0: return { blur: 0.0, offset: 0, opacity: 0.0 };
        case 1: return { blur: 0.45, offset: 3, opacity: 0.50 };
        case 2: return { blur: 0.65, offset: 5, opacity: 0.55 };
        case 3: return { blur: 0.85, offset: 8, opacity: 0.60 };
        default: return { blur: 0.50, offset: 4, opacity: 0.30 };
        }
    }
    readonly property var _pressedShadow: {
        switch (elevation) {
        case 0: return { blur: 0.0, offset: 0, opacity: 0.0 };
        case 1: return { blur: 0.0, offset: 0, opacity: 0.0 };
        default: return { blur: 0.35, offset: 2, opacity: 0.30 };
        }
    }

    readonly property bool _isPressed: mouseArea.pressed && buttonEnabled

    scale: _isPressed ? buttonScale : 1.0
    Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

    // Background rectangle (source for MultiEffect)
    Rectangle {
        id: bg
        anchors.fill: parent
        radius: root.buttonRadius
        color: root._isPressed ? root.pressedColor : root.buttonColor
        border.width: 1
        border.color: ThemeService.outlineVariant
        opacity: root.buttonEnabled ? 1.0 : 0.5
        layer.enabled: true
        visible: false
    }

    // Shadow effect
    MultiEffect {
        id: shadow
        source: bg
        anchors.fill: bg
        shadowEnabled: root.elevation > 0
        shadowColor: ThemeService.shadow
        shadowBlur: root._isPressed ? root._pressedShadow.blur : root._shadowParams.blur
        shadowVerticalOffset: root._isPressed ? root._pressedShadow.offset : root._shadowParams.offset
        shadowOpacity: root._isPressed ? root._pressedShadow.opacity : root._shadowParams.opacity
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
        radius: root.buttonRadius
        color: ThemeService.onSurface
        opacity: root._isPressed ? 0.10 : 0.0
        Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
    }

    // Content
    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: UiMetrics.spacing / 2

        MaterialIcon {
            icon: root.iconCode
            size: contentText.font.pixelSize
            color: root._isPressed ? root.pressedTextColor : root.textColor
            visible: root.iconCode !== ""
            anchors.verticalCenter: parent.verticalCenter
        }

        NormalText {
            id: contentText
            text: root.text
            color: root._isPressed ? root.pressedTextColor : root.textColor
            visible: root.text !== ""
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        cursorShape: root.buttonEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            if (root.buttonEnabled)
                root.clicked()
        }
        onPressAndHold: {
            if (root.buttonEnabled)
                root.pressAndHold()
        }
    }
}
