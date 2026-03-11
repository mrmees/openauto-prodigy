import QtQuick

Item {
    id: root

    property Flickable flickable
    property real edgeInset: UiMetrics.marginRow
    property real visibilityThreshold: UiMetrics.spacing
    property real hintOpacity: 0.8

    // Reparent to the Flickable itself so this overlay stays fixed to the viewport
    // instead of scrolling with the Flickable's contentItem.
    parent: flickable ? flickable : null
    anchors.fill: parent
    z: 100
    enabled: false
    visible: flickable !== null

    readonly property real maxContentY: {
        if (!flickable)
            return 0
        return Math.max(0, flickable.contentHeight - flickable.height)
    }
    readonly property bool showTopHint: flickable
                                        && maxContentY > visibilityThreshold
                                        && flickable.contentY > visibilityThreshold
    readonly property bool showBottomHint: flickable
                                           && maxContentY > visibilityThreshold
                                           && flickable.contentY < (maxContentY - visibilityThreshold)

    MaterialIcon {
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.edgeInset
        icon: "\ue5ce"
        size: UiMetrics.iconSize
        color: ThemeService.onSurfaceVariant
        opacity: root.showTopHint ? root.hintOpacity : 0.0

        Behavior on opacity {
            NumberAnimation {
                duration: UiMetrics.animDurationFast
                easing.type: Easing.OutCubic
            }
        }
    }

    MaterialIcon {
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height - height - root.edgeInset
        icon: "\ue5cf"
        size: UiMetrics.iconSize
        color: ThemeService.onSurfaceVariant
        opacity: root.showBottomHint ? root.hintOpacity : 0.0

        Behavior on opacity {
            NumberAnimation {
                duration: UiMetrics.animDurationFast
                easing.type: Easing.OutCubic
            }
        }
    }
}
