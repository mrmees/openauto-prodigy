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
    readonly property int touchTarget: Math.max(UiMetrics.touchMin,
                                                Math.round(64 * UiMetrics.scale))
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
        dim: false
        closePolicy: Popup.CloseOnEscape
        parent: Overlay.overlay
        x: 0
        y: 0
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        padding: 0

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: UiMetrics.animDuration }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: UiMetrics.animDurationFast }
        }

        background: Rectangle {
            color: ThemeService.surface
        }

        header: Item {
            implicitHeight: Math.max(root.touchTarget, Math.round(72 * UiMetrics.scale))

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginPage
                anchors.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.gap

                MaterialIcon {
                    icon: root.widgetIcon
                    size: UiMetrics.iconSize
                    color: ThemeService.primary
                    visible: root.widgetIcon !== ""
                }

                Text {
                    text: qsTr("Configure %1").arg(root.widgetName)
                    font.pixelSize: UiMetrics.fontTitle
                    font.bold: true
                    color: ThemeService.onSurface
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                MouseArea {
                    objectName: "widgetConfigClose"
                    Layout.preferredWidth: root.touchTarget
                    Layout.preferredHeight: root.touchTarget
                    onClicked: root.cancelDraft()

                    Rectangle {
                        anchors.fill: parent
                        radius: UiMetrics.radius
                        color: parent.pressed
                            ? ThemeService.primaryContainer
                            : ThemeService.surfaceContainer
                    }

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
                width: parent.width
                height: 1
                color: ThemeService.outlineVariant
            }
        }

        contentItem: ColumnLayout {
            spacing: 0

            Flickable {
                id: configFlickable
                Layout.fillWidth: true
                Layout.fillHeight: true
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
                            x: UiMetrics.marginPage
                            width: configColumn.width - UiMetrics.marginPage * 2
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

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: ThemeService.outlineVariant
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: root.touchTarget + UiMetrics.spacing * 2
                Layout.leftMargin: UiMetrics.marginPage
                Layout.rightMargin: UiMetrics.marginPage
                spacing: UiMetrics.gap

                Item { Layout.fillWidth: true }

                OutlinedButton {
                    objectName: "widgetConfigCancel"
                    text: qsTr("Cancel")
                    Layout.preferredWidth: Math.max(root.touchTarget * 1.8, implicitWidth)
                    Layout.preferredHeight: root.touchTarget
                    onClicked: root.cancelDraft()
                }

                FilledButton {
                    objectName: "widgetConfigSave"
                    text: qsTr("Save")
                    iconCode: "\ue161"
                    buttonEnabled: root.draftValid
                    Layout.preferredWidth: Math.max(root.touchTarget * 1.8, implicitWidth)
                    Layout.preferredHeight: root.touchTarget
                    onClicked: root.saveDraft()
                }
            }
        }
    }

    Component {
        id: enumControl

        FullScreenPicker {
            width: parent ? parent.width : 0
            label: fieldData ? fieldData.label || "" : ""
            options: fieldData ? fieldData.options || [] : []
            values: fieldData ? fieldData.values || [] : []
            placeholderText: qsTr("Select")
            currentIndex: {
                if (!fieldData || !fieldData.values)
                    return -1
                return fieldData.values.indexOf(root.draftConfig[fieldData.key])
            }
            onActivated: function(index) {
                if (fieldData && fieldData.values && index >= 0)
                    root.updateDraft(fieldData.key, fieldData.values[index])
            }
        }
    }

    Component {
        id: collectionControl

        FullScreenPicker {
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
            label: fieldData
                   ? (fieldData.label || "") + (fieldData.required ? " *" : "")
                   : ""
            options: fieldData ? fieldData.options || [] : []
            values: fieldData ? fieldData.values || [] : []
            placeholderText: selectedText()
            pickerEnabled: fieldData && fieldData.values && fieldData.values.length > 0
            currentIndex: {
                if (!fieldData || !fieldData.values)
                    return -1
                return fieldData.values.indexOf(root.draftConfig[fieldData.key])
            }
            onActivated: function(index) {
                if (fieldData && fieldData.values && index >= 0)
                    root.updateDraft(fieldData.key, fieldData.values[index])
            }
        }
    }

    Component {
        id: boolControl

        Item {
            height: Math.max(UiMetrics.rowH, UiMetrics.touchMin)
            width: parent ? parent.width : 0

            Rectangle {
                anchors.fill: parent
                radius: UiMetrics.radiusSmall
                color: ThemeService.surfaceContainerLow
                border.width: 1
                border.color: ThemeService.outlineVariant
            }

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

            Rectangle {
                anchors.fill: parent
                radius: UiMetrics.radiusSmall
                color: ThemeService.surfaceContainerLow
                border.width: 1
                border.color: ThemeService.outlineVariant
            }

            Column {
                anchors.fill: parent
                anchors.margins: UiMetrics.marginRow
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
