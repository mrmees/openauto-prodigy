import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property string label: ""
    property string configPath: ""
    property var options: []      // display strings: ["30", "60"] or ["Left", "Right"]
    property var values: []       // typed values for config storage — if empty, stores options strings
    property bool restartRequired: false

    readonly property int currentIndex: internal.selectedIndex
    readonly property var currentValue: {
        if (root.values.length > 0 && internal.selectedIndex >= 0 && internal.selectedIndex < root.values.length)
            return root.values[internal.selectedIndex]
        if (internal.selectedIndex >= 0 && internal.selectedIndex < root.options.length)
            return root.options[internal.selectedIndex]
        return undefined
    }

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    QtObject {
        id: internal
        property int selectedIndex: 0
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginRow
        anchors.rightMargin: UiMetrics.marginRow
        spacing: UiMetrics.gap

        Text {
            text: root.label
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.normalFontColor
            Layout.fillWidth: true
        }

        MaterialIcon {
            icon: "\ue86a"
            size: UiMetrics.iconSmall
            color: ThemeService.descriptionFontColor
            visible: root.restartRequired
        }

        Row {
            id: segmentRow
            spacing: 0

            Repeater {
                model: root.options

                Rectangle {
                    id: segment
                    property bool isSelected: index === internal.selectedIndex
                    property bool isFirst: index === 0
                    property bool isLast: index === root.options.length - 1

                    width: Math.max(UiMetrics.touchMin * 1.5, segLabel.implicitWidth + UiMetrics.gap)
                    height: UiMetrics.touchMin
                    color: isSelected ? ThemeService.highlightColor : ThemeService.controlBackgroundColor
                    radius: UiMetrics.radius

                    // Mask inner corners by overlapping rectangles
                    Rectangle {
                        visible: !segment.isFirst
                        anchors.left: parent.left
                        anchors.top: parent.top
                        width: parent.radius
                        height: parent.height
                        color: parent.color
                    }
                    Rectangle {
                        visible: !segment.isLast
                        anchors.right: parent.right
                        anchors.top: parent.top
                        width: parent.radius
                        height: parent.height
                        color: parent.color
                    }

                    // Subtle divider between unselected segments
                    Rectangle {
                        visible: !segment.isFirst && !segment.isSelected && internal.selectedIndex !== (index - 1)
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: 1
                        height: parent.height * 0.6
                        color: ThemeService.descriptionFontColor
                        opacity: 0.3
                    }

                    Text {
                        id: segLabel
                        anchors.centerIn: parent
                        text: modelData
                        font.pixelSize: UiMetrics.fontSmall
                        color: segment.isSelected ? ThemeService.backgroundColor : ThemeService.normalFontColor
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (internal.selectedIndex === index) return
                            internal.selectedIndex = index
                            if (root.configPath === "") return
                            var writeVal = (root.values.length > 0 && index < root.values.length)
                                ? root.values[index] : modelData
                            ConfigService.setValue(root.configPath, writeVal)
                            ConfigService.save()
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        if (root.configPath !== "") {
            var current = ConfigService.value(root.configPath)
            var idx = -1
            if (root.values.length > 0) {
                for (var i = 0; i < root.values.length; i++) {
                    if (root.values[i] == current) { idx = i; break }
                }
            }
            if (idx < 0)
                idx = root.options.indexOf(String(current))
            if (idx >= 0) internal.selectedIndex = idx
        }
    }
}
