import QtQuick

Item {
    id: root

    // Widget contract: context injection from host
    property QtObject widgetContext: null

    MaterialIcon {
        anchors.centerIn: parent
        icon: "\ue8b8"  // settings gear
        size: Math.min(root.width, root.height) * 0.6
        opticalSize: Math.min(size, 48)
        color: ThemeService.onSurface
    }
    MouseArea {
        anchors.fill: parent
        pressAndHoldInterval: 500
        onClicked: {
            ActionRegistry.dispatch("app.openSettings")
        }
        onPressAndHold: {
            if (root.parent && root.parent.requestContextMenu)
                root.parent.requestContextMenu()
        }
    }
}
