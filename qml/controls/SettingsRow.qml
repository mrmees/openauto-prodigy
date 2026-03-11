import QtQuick
import QtQuick.Layouts

Item {
    id: root

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

    // Content container with row margins
    Item {
        id: contentContainer
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginRow
        anchors.rightMargin: UiMetrics.marginRow
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
