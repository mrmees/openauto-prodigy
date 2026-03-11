import QtQuick
import QtQuick.Layouts

Item {
    id: root
    readonly property bool blocksBackHold: interactive

    property int rowIndex: 0
    property bool interactive: false
    signal clicked()
    default property alias content: contentContainer.data

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    // Alternating background: even = surfaceContainer, odd = surfaceContainerHigh
    Rectangle {
        id: bg
        anchors.fill: parent
        color: rowIndex % 2 === 0 ? ThemeService.surfaceContainer : ThemeService.surfaceContainerHigh
    }

    // Press feedback state layer (interactive rows only)
    Rectangle {
        anchors.fill: parent
        color: ThemeService.onSurface
        opacity: interactive && mouseArea.pressed ? 0.08 : 0.0
        Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
    }

    // Content container — no margins (controls provide their own)
    Item {
        id: contentContainer
        anchors.fill: parent
        // Auto-size the single content child to fill this container
        onChildrenChanged: {
            for (var i = 0; i < children.length; i++) {
                var child = children[i];
                child.anchors.left = contentContainer.left;
                child.anchors.right = contentContainer.right;
                child.anchors.top = contentContainer.top;
                child.anchors.bottom = contentContainer.bottom;
            }
        }
    }

    // MouseArea for interactive rows
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.interactive
        visible: root.interactive
        onClicked: root.clicked()
    }
}
