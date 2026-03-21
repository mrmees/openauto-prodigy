import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root

    property string instanceId: ""
    property string widgetId: ""
    property string widgetName: ""
    property string widgetIcon: ""
    property var currentConfig: ({})
    property var defaultConfig: ({})
    property var overrideKeys: ({})
    property var schemaFields: []
    readonly property bool isOpen: configDialog.visible

    function closeConfig() {
        configDialog.close()
    }

    function openConfig(instanceId, widgetId, displayName, iconName) {
        root.instanceId = instanceId
        root.widgetId = widgetId
        root.widgetName = displayName
        root.widgetIcon = iconName

        // Load schema fields as plain QVariantList
        root.schemaFields = WidgetGridModel.configSchemaForWidget(widgetId)

        // Load defaults for override delta computation
        root.defaultConfig = WidgetGridModel.defaultConfigForWidget(widgetId)

        // Load effective config (defaults merged with overrides) for display
        root.currentConfig = WidgetGridModel.effectiveWidgetConfig(instanceId)

        // Seed overrideKeys from existing overrides so they're retained
        var existing = WidgetGridModel.widgetConfig(instanceId)
        var keys = {}
        for (var k in existing) keys[k] = true
        root.overrideKeys = keys

        configDialog.open()
    }

    function applyConfig(key, value) {
        var cfg = root.currentConfig
        cfg[key] = value
        root.currentConfig = cfg

        var ok = root.overrideKeys
        ok[key] = true
        root.overrideKeys = ok

        // Build override-only map: only keys that differ from defaults
        var overrides = {}
        for (var k in root.overrideKeys) {
            if (root.overrideKeys[k] && root.currentConfig[k] !== root.defaultConfig[k]) {
                overrides[k] = root.currentConfig[k]
            }
        }
        WidgetGridModel.setWidgetConfig(root.instanceId, overrides)
    }

    Dialog {
        id: configDialog
        modal: true
        dim: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        parent: Overlay.overlay
        x: 0
        y: parent ? parent.height : 0
        width: parent ? parent.width : 0
        height: parent ? parent.height * 0.6 : 300

        padding: 0
        topPadding: 0
        bottomPadding: 0

        enter: Transition {
            NumberAnimation {
                property: "y"
                from: configDialog.parent ? configDialog.parent.height : 0
                to: configDialog.parent ? configDialog.parent.height * 0.4 : 0
                duration: 250
                easing.type: Easing.OutCubic
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "y"
                from: configDialog.parent ? configDialog.parent.height * 0.4 : 0
                to: configDialog.parent ? configDialog.parent.height : 0
                duration: 200
                easing.type: Easing.InCubic
            }
        }

        onOpened: {
            y = parent ? parent.height * 0.4 : 0
        }

        background: Rectangle {
            color: ThemeService.surface
            radius: UiMetrics.radius
            // Only round top corners
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: UiMetrics.radius
                color: ThemeService.surface
            }
        }

        header: Item {
            implicitHeight: UiMetrics.headerH

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginPage
                anchors.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.gap

                MaterialIcon {
                    icon: root.widgetIcon
                    size: UiMetrics.iconSize
                    color: ThemeService.onSurface
                    visible: root.widgetIcon !== ""
                }

                Text {
                    text: root.widgetName
                    font.pixelSize: UiMetrics.fontTitle
                    font.bold: true
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                }

                MouseArea {
                    Layout.preferredWidth: UiMetrics.backBtnSize
                    Layout.preferredHeight: UiMetrics.backBtnSize
                    onClicked: configDialog.close()

                    MaterialIcon {
                        anchors.centerIn: parent
                        icon: "\ue5cd"
                        size: UiMetrics.iconSize
                        color: ThemeService.onSurface
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: ThemeService.outlineVariant
            }
        }

        contentItem: Flickable {
            id: configFlickable
            clip: true
            contentHeight: configColumn.height
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: configColumn
                width: parent.width
                spacing: UiMetrics.spacing

                topPadding: UiMetrics.spacing
                bottomPadding: UiMetrics.spacing

                Repeater {
                    model: root.schemaFields

                    Loader {
                        width: configColumn.width
                        property var fieldData: modelData

                        sourceComponent: {
                            if (!fieldData) return null
                            if (fieldData.type === "enum") return enumControl
                            if (fieldData.type === "bool") return boolControl
                            if (fieldData.type === "intrange") return intRangeControl
                            return null
                        }
                    }
                }
            }
        }
    }

    // --- Control components ---

    Component {
        id: enumControl

        Item {
            height: UiMetrics.rowH
            width: parent ? parent.width : 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginPage
                anchors.rightMargin: UiMetrics.marginPage
                spacing: UiMetrics.gap

                Text {
                    text: fieldData.label || ""
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                }

                // Current value display + dropdown
                Rectangle {
                    Layout.preferredWidth: enumRow.implicitWidth + UiMetrics.marginRow * 2
                    Layout.preferredHeight: UiMetrics.touchMin
                    radius: UiMetrics.radiusSmall
                    color: ThemeService.surfaceContainerLow
                    border.width: 1
                    border.color: ThemeService.outlineVariant

                    RowLayout {
                        id: enumRow
                        anchors.centerIn: parent
                        spacing: UiMetrics.spacing * 0.5

                        Text {
                            id: enumValueText
                            font.pixelSize: UiMetrics.fontBody
                            color: ThemeService.onSurface

                            // Find the display string for current value
                            text: {
                                if (!fieldData || !fieldData.values || !fieldData.options) return ""
                                var val = root.currentConfig[fieldData.key]
                                for (var i = 0; i < fieldData.values.length; i++) {
                                    if (fieldData.values[i] === val)
                                        return fieldData.options[i] || ""
                                }
                                return String(val || "")
                            }
                        }

                        MaterialIcon {
                            icon: "\ue5cf"
                            size: UiMetrics.iconSmall
                            color: ThemeService.onSurfaceVariant
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: enumPopup.open()
                    }

                    // Dropdown popup for enum options
                    Popup {
                        id: enumPopup
                        parent: Overlay.overlay
                        modal: true
                        dim: true
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                        x: (parent ? (Overlay.overlay ? (Overlay.overlay.width - width) / 2 : 0) : 0)
                        y: (parent ? (Overlay.overlay ? (Overlay.overlay.height - height) / 2 : 0) : 0)
                        width: Math.min(Overlay.overlay ? Overlay.overlay.width * 0.7 : 300, 400)
                        padding: 0

                        background: Rectangle {
                            color: ThemeService.surfaceContainerHigh
                            radius: UiMetrics.radius
                        }

                        contentItem: Column {
                            Repeater {
                                model: fieldData ? fieldData.options : []

                                Item {
                                    width: enumPopup.width
                                    height: UiMetrics.rowH

                                    property bool isSelected: {
                                        if (!fieldData || !fieldData.values) return false
                                        return fieldData.values[index] === root.currentConfig[fieldData.key]
                                    }

                                    Rectangle {
                                        anchors.fill: parent
                                        anchors.leftMargin: UiMetrics.spacing * 0.5
                                        anchors.rightMargin: UiMetrics.spacing * 0.5
                                        radius: UiMetrics.radiusSmall
                                        color: parent.isSelected ? ThemeService.secondaryContainer : "transparent"
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: UiMetrics.marginPage
                                        anchors.rightMargin: UiMetrics.marginPage
                                        spacing: UiMetrics.gap

                                        Text {
                                            text: modelData
                                            font.pixelSize: UiMetrics.fontBody
                                            color: parent.parent.isSelected
                                                ? ThemeService.onSecondaryContainer
                                                : ThemeService.onSurface
                                            Layout.fillWidth: true
                                        }

                                        MaterialIcon {
                                            icon: "\ue876"
                                            size: UiMetrics.iconSize
                                            color: ThemeService.onSecondaryContainer
                                            visible: parent.parent.isSelected
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            root.applyConfig(fieldData.key, fieldData.values[index])
                                            enumPopup.close()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: boolControl

        Item {
            height: UiMetrics.rowH
            width: parent ? parent.width : 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginPage
                anchors.rightMargin: UiMetrics.marginPage
                spacing: UiMetrics.gap

                Text {
                    text: fieldData.label || ""
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                }

                Switch {
                    checked: root.currentConfig[fieldData.key] === true
                    onToggled: root.applyConfig(fieldData.key, checked)
                }
            }
        }
    }

    Component {
        id: intRangeControl

        Item {
            height: UiMetrics.rowH * 1.5
            width: parent ? parent.width : 0

            Column {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginPage
                anchors.rightMargin: UiMetrics.marginPage
                spacing: UiMetrics.spacing * 0.25

                RowLayout {
                    width: parent.width
                    spacing: UiMetrics.gap

                    Text {
                        text: fieldData.label || ""
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.onSurface
                        Layout.fillWidth: true
                    }

                    Text {
                        text: String(root.currentConfig[fieldData.key] !== undefined
                                     ? root.currentConfig[fieldData.key] : fieldData.rangeMin)
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.onSurfaceVariant
                    }
                }

                Slider {
                    width: parent.width
                    from: fieldData.rangeMin
                    to: fieldData.rangeMax
                    stepSize: fieldData.rangeStep
                    value: root.currentConfig[fieldData.key] !== undefined
                           ? root.currentConfig[fieldData.key] : fieldData.rangeMin
                    onMoved: root.applyConfig(fieldData.key, value)
                }
            }
        }
    }
}
