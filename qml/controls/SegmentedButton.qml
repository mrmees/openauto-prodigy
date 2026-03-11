import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    readonly property bool blocksBackHold: true
    property string label: ""
    property string configPath: ""
    property var options: []      // display strings: ["30", "60"] or ["Left", "Right"]
    property var values: []       // typed values for config storage -- if empty, stores options strings
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
            color: ThemeService.onSurface
            Layout.fillWidth: true
        }

        MaterialIcon {
            icon: "\ue86a"
            size: UiMetrics.iconSmall
            color: ThemeService.onSurfaceVariant
            visible: root.restartRequired
        }

        // Shadow container for the whole segmented control
        Item {
            id: segContainer
            Layout.preferredWidth: segmentRow.width
            Layout.preferredHeight: segmentRow.height

            // Background source for shadow (covers entire segmented control)
            Rectangle {
                id: segBg
                anchors.fill: segmentRow
                radius: UiMetrics.radius
                color: ThemeService.surfaceContainerLow
                border.width: 1
                border.color: ThemeService.outlineVariant
                layer.enabled: true
                visible: false
            }

            // Shadow effect on the whole segmented control
            MultiEffect {
                source: segBg
                anchors.fill: segBg
                shadowEnabled: true
                shadowColor: ThemeService.shadow
                shadowBlur: 0.65
                shadowVerticalOffset: 5
                shadowOpacity: 0.55
                shadowHorizontalOffset: 0
                shadowScale: 1.0
                autoPaddingEnabled: true
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
                        readonly property bool _isPressed: segMouseArea.pressed

                        width: Math.max(UiMetrics.touchMin * 1.5, segLabel.implicitWidth + UiMetrics.gap)
                        height: UiMetrics.touchMin
                        color: isSelected ? ThemeService.secondaryContainer : ThemeService.surfaceContainerLow
                        radius: UiMetrics.radius

                        scale: _isPressed ? 0.97 : 1.0
                        Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

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
                            color: ThemeService.outlineVariant
                        }

                        // State layer overlay
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            color: ThemeService.onSurface
                            opacity: segment._isPressed ? 0.10 : 0.0
                            Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
                        }

                        Text {
                            id: segLabel
                            anchors.centerIn: parent
                            text: modelData
                            font.pixelSize: UiMetrics.fontSmall
                            color: segment.isSelected ? ThemeService.onSecondaryContainer : ThemeService.onSurface
                        }

                        SettingsHoldArea {
                            id: segMouseArea
                            anchors.fill: parent
                            onShortClicked: {
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
