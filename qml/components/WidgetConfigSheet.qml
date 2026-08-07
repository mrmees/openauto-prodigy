import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root

    property string instanceId: ""
    property string widgetId: ""
    property string widgetName: ""
    property string widgetIcon: ""
    property var draftConfig: ({})
    property var defaultConfig: ({})
    property var overrideKeys: ({})
    property var schemaFields: []
    readonly property bool isOpen: configDialog.visible
    readonly property bool draftValid: root.widgetId !== ""
                                          && WidgetGridModel.isWidgetConfigValid(
                                              root.widgetId, root.draftConfig)

    function copyMap(source) {
        var copy = {}
        if (!source)
            return copy
        for (var key in source)
            copy[key] = source[key]
        return copy
    }

    function closeConfig() {
        cancelDraft()
    }

    function openConfig(instanceId, widgetId, displayName, iconName) {
        root.instanceId = instanceId
        root.widgetId = widgetId
        root.widgetName = displayName
        root.widgetIcon = iconName

        // Rescan on every open so newly installed collection items appear
        // without restarting Prodigy.
        root.schemaFields = WidgetGridModel.configSchemaForWidget(widgetId)
        root.defaultConfig = copyMap(WidgetGridModel.defaultConfigForWidget(widgetId))
        root.draftConfig = copyMap(WidgetGridModel.effectiveWidgetConfig(instanceId))

        var existing = WidgetGridModel.widgetConfig(instanceId)
        var keys = {}
        for (var key in existing)
            keys[key] = true
        root.overrideKeys = keys

        configDialog.open()
    }

    function updateDraft(key, value) {
        var draft = copyMap(root.draftConfig)
        draft[key] = value
        root.draftConfig = draft

        var keys = copyMap(root.overrideKeys)
        keys[key] = true
        root.overrideKeys = keys
    }

    function cancelDraft() {
        configDialog.close()
    }

    function saveDraft() {
        if (!root.draftValid)
            return

        var overrides = {}
        for (var key in root.overrideKeys) {
            if (root.overrideKeys[key]
                    && root.draftConfig[key] !== root.defaultConfig[key]) {
                overrides[key] = root.draftConfig[key]
            }
        }
        WidgetGridModel.setWidgetConfig(root.instanceId, overrides)
        configDialog.close()
    }

    Dialog {
        id: configDialog
        modal: true
        dim: true
        closePolicy: Popup.NoAutoClose
        parent: Overlay.overlay
        x: 0
        y: 0
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        padding: 0

        background: Rectangle {
            color: ThemeService.surface
        }

        header: Item {
            implicitHeight: Math.max(UiMetrics.headerH, UiMetrics.touchMin)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginPage
                anchors.rightMargin: UiMetrics.marginPage
                spacing: UiMetrics.gap

                Button {
                    objectName: "widgetConfigCancel"
                    text: qsTr("Cancel")
                    Layout.preferredWidth: Math.max(UiMetrics.touchMin * 1.6, implicitWidth)
                    Layout.preferredHeight: UiMetrics.touchMin
                    onClicked: root.cancelDraft()
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: UiMetrics.spacing

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
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                Button {
                    objectName: "widgetConfigSave"
                    text: qsTr("Save")
                    Layout.preferredWidth: Math.max(UiMetrics.touchMin * 1.6, implicitWidth)
                    Layout.preferredHeight: UiMetrics.touchMin
                    enabled: root.draftValid
                    onClicked: root.saveDraft()
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: ThemeService.outlineVariant
            }
        }

        contentItem: Flickable {
            id: configFlickable
            clip: true
            contentHeight: configColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: configColumn
                width: configFlickable.width
                spacing: UiMetrics.spacing
                topPadding: UiMetrics.spacing
                bottomPadding: UiMetrics.marginPage

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
                            if (fieldData.type === "collection") return collectionControl
                            return null
                        }
                    }
                }
            }
        }
    }

    Component {
        id: enumControl

        Item {
            height: Math.max(UiMetrics.rowH, UiMetrics.touchMin)
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

                Button {
                    Layout.minimumHeight: UiMetrics.touchMin
                    text: {
                        if (!fieldData || !fieldData.values || !fieldData.options)
                            return ""
                        var value = root.draftConfig[fieldData.key]
                        for (var i = 0; i < fieldData.values.length; ++i) {
                            if (fieldData.values[i] === value)
                                return fieldData.options[i] || ""
                        }
                        return value === undefined ? "" : String(value)
                    }
                    onClicked: enumPopup.open()
                }
            }

            Popup {
                id: enumPopup
                parent: Overlay.overlay
                modal: true
                dim: true
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                anchors.centerIn: Overlay.overlay
                width: Math.min(Overlay.overlay.width * 0.7, 420)
                padding: 0

                background: Rectangle {
                    color: ThemeService.surfaceContainerHigh
                    radius: UiMetrics.radius
                }

                contentItem: Column {
                    Repeater {
                        model: fieldData.options || []

                        Button {
                            width: enumPopup.width
                            height: Math.max(UiMetrics.rowH, UiMetrics.touchMin)
                            text: modelData
                            checked: fieldData && fieldData.values
                                     && fieldData.values[index] === root.draftConfig[fieldData.key]
                            onClicked: {
                                root.updateDraft(fieldData.key, fieldData.values[index])
                                enumPopup.close()
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: collectionControl

        Item {
            height: Math.max(UiMetrics.rowH, UiMetrics.touchMin)
            width: parent ? parent.width : 0

            function selectedText() {
                if (!fieldData)
                    return ""
                var values = fieldData.values || []
                var options = fieldData.options || []
                var selected = root.draftConfig[fieldData.key]
                for (var i = 0; i < values.length; ++i) {
                    if (values[i] === selected)
                        return options[i] || values[i]
                }
                if (selected !== undefined && selected !== "")
                    return qsTr("Missing: ") + selected
                if (values.length === 0)
                    return qsTr("No items installed")
                return qsTr("Select")
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginPage
                anchors.rightMargin: UiMetrics.marginPage
                spacing: UiMetrics.gap

                Text {
                    text: (fieldData.label || "") + (fieldData.required ? " *" : "")
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                }

                Button {
                    Layout.minimumHeight: UiMetrics.touchMin
                    text: parent.parent.selectedText()
                    enabled: fieldData.values && fieldData.values.length > 0
                    onClicked: collectionPopup.open()
                }
            }

            Popup {
                id: collectionPopup
                parent: Overlay.overlay
                modal: true
                dim: true
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                anchors.centerIn: Overlay.overlay
                width: Math.min(Overlay.overlay.width * 0.8, 480)
                padding: 0

                background: Rectangle {
                    color: ThemeService.surfaceContainerHigh
                    radius: UiMetrics.radius
                }

                contentItem: Column {
                    Repeater {
                        model: fieldData.options || []

                        Button {
                            width: collectionPopup.width
                            height: Math.max(UiMetrics.rowH, UiMetrics.touchMin)
                            text: modelData
                            checked: fieldData && fieldData.values
                                     && fieldData.values[index] === root.draftConfig[fieldData.key]
                            onClicked: {
                                root.updateDraft(fieldData.key, fieldData.values[index])
                                collectionPopup.close()
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
            height: Math.max(UiMetrics.rowH, UiMetrics.touchMin)
            width: parent ? parent.width : 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginPage
                anchors.rightMargin: UiMetrics.marginPage

                Text {
                    text: fieldData.label || ""
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                }

                Switch {
                    Layout.minimumWidth: UiMetrics.touchMin
                    Layout.minimumHeight: UiMetrics.touchMin
                    checked: root.draftConfig[fieldData.key] === true
                    onToggled: root.updateDraft(fieldData.key, checked)
                }
            }
        }
    }

    Component {
        id: intRangeControl

        Item {
            height: Math.max(UiMetrics.rowH * 1.5, UiMetrics.touchMin)
            width: parent ? parent.width : 0

            Column {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginPage
                anchors.rightMargin: UiMetrics.marginPage
                spacing: UiMetrics.spacing * 0.25

                RowLayout {
                    width: parent.width

                    Text {
                        text: fieldData.label || ""
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.onSurface
                        Layout.fillWidth: true
                    }

                    Text {
                        text: String(root.draftConfig[fieldData.key] !== undefined
                                     ? root.draftConfig[fieldData.key] : fieldData.rangeMin)
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.onSurfaceVariant
                    }
                }

                Slider {
                    width: parent.width
                    height: UiMetrics.touchMin
                    from: fieldData.rangeMin
                    to: fieldData.rangeMax
                    stepSize: fieldData.rangeStep
                    value: root.draftConfig[fieldData.key] !== undefined
                           ? root.draftConfig[fieldData.key] : fieldData.rangeMin
                    onMoved: root.updateDraft(fieldData.key, value)
                }
            }
        }
    }
}
