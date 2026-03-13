import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: homeScreen
    anchors.fill: parent

    Component.onCompleted: ApplicationController.setTitle("Home")

    // Edit mode state
    property bool editMode: false

    // Track which grid cell was long-pressed (for widget placement)
    property int _targetCol: 0
    property int _targetRow: 0
    property string _targetInstanceId: ""  // non-empty = editing existing widget

    // Track active drag/resize for cleanup on exit
    property string draggingInstanceId: ""
    property string resizingInstanceId: ""

    function exitEditMode() {
        editMode = false
        inactivityTimer.stop()
        contextMenuOverlay.visible = false
        pickerOverlay.visible = false
        // Reset drag state
        dropHighlight.visible = false
        dragPlaceholder.visible = false
        resizeGhost.visible = false
        draggingInstanceId = ""
        resizingInstanceId = ""
    }

    // Inactivity timer -- exits edit mode after 10s of no interaction
    Timer {
        id: inactivityTimer
        interval: 10000
        onTriggered: homeScreen.exitEditMode()
    }

    onEditModeChanged: {
        if (editMode) {
            inactivityTimer.restart()
        } else {
            inactivityTimer.stop()
        }
    }

    // AA fullscreen auto-exit (EDIT-08)
    Connections {
        target: PluginModel
        function onActivePluginChanged() {
            if (PluginModel.activePluginFullscreen && homeScreen.editMode)
                homeScreen.exitEditMode()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        // Grid container -- fills available space above dock
        Item {
            id: gridContainer
            Layout.fillWidth: true
            Layout.fillHeight: true

            readonly property real cellWidth: width / WidgetGridModel.gridColumns
            readonly property real cellHeight: height / WidgetGridModel.gridRows

            // Background long-press / tap detector for empty grid space
            MouseArea {
                anchors.fill: parent
                pressAndHoldInterval: 500
                onPressAndHold: function(mouse) {
                    if (!homeScreen.editMode) {
                        homeScreen.editMode = true
                    }
                }
                onClicked: {
                    if (homeScreen.editMode) {
                        homeScreen.exitEditMode()
                    }
                }
            }

            // Dotted grid lines overlay (visible in editMode)
            Item {
                anchors.fill: parent
                visible: homeScreen.editMode
                z: 0

                // Vertical lines
                Repeater {
                    model: WidgetGridModel.gridColumns + 1
                    delegate: Column {
                        x: index * gridContainer.cellWidth
                        y: 0
                        height: gridContainer.height
                        spacing: UiMetrics.spacing
                        Repeater {
                            model: Math.ceil(gridContainer.height / (UiMetrics.spacing + 2))
                            delegate: Rectangle {
                                width: 1
                                height: 2
                                color: ThemeService.outlineVariant
                                opacity: 0.3
                            }
                        }
                    }
                }

                // Horizontal lines
                Repeater {
                    model: WidgetGridModel.gridRows + 1
                    delegate: Row {
                        x: 0
                        y: index * gridContainer.cellHeight
                        width: gridContainer.width
                        spacing: UiMetrics.spacing
                        Repeater {
                            model: Math.ceil(gridContainer.width / (UiMetrics.spacing + 2))
                            delegate: Rectangle {
                                width: 2
                                height: 1
                                color: ThemeService.outlineVariant
                                opacity: 0.3
                            }
                        }
                    }
                }
            }

            // Drop highlight rectangle (shows valid/invalid drop target during drag)
            Rectangle {
                id: dropHighlight
                visible: false
                z: 5
                radius: UiMetrics.radius
                border.width: 2
                color: "transparent"
                opacity: 0.6
                property bool isValid: true
                border.color: isValid ? ThemeService.primary : ThemeService.error
            }

            // Placeholder at original position during drag
            Rectangle {
                id: dragPlaceholder
                visible: false
                z: 1
                color: "transparent"
                border.width: 1
                border.color: ThemeService.outlineVariant
                radius: UiMetrics.radius
                opacity: 0.4
            }

            // Resize ghost rectangle (shows proposed size during resize)
            Rectangle {
                id: resizeGhost
                visible: false
                z: 50
                color: "transparent"
                border.width: 2
                radius: UiMetrics.radius
                opacity: 0.7
                property bool isValid: true
                border.color: isValid ? ThemeService.primary : ThemeService.error

                // Brief flash animation for boundary hit
                SequentialAnimation {
                    id: limitFlash
                    PropertyAnimation {
                        target: resizeGhost
                        property: "border.color"
                        to: ThemeService.tertiary
                        duration: 100
                    }
                    PropertyAnimation {
                        target: resizeGhost
                        property: "border.color"
                        to: resizeGhost.isValid ? ThemeService.primary : ThemeService.error
                        duration: 100
                    }
                }
            }

            Repeater {
                id: widgetRepeater
                model: WidgetGridModel

                delegate: Item {
                    id: delegateItem
                    x: model.column * gridContainer.cellWidth
                    y: model.row * gridContainer.cellHeight
                    width: model.colSpan * gridContainer.cellWidth
                    height: model.rowSpan * gridContainer.cellHeight
                    visible: model.visible
                    z: dragging ? 100 : 1

                    // Drag state
                    property bool dragging: false
                    property real dragStartX: 0
                    property real dragStartY: 0
                    property real pressOffsetX: 0
                    property real pressOffsetY: 0
                    property real originalX: 0
                    property real originalY: 0

                    // Force-reset drag state when edit mode exits (EVIOCGRAB safety)
                    Connections {
                        target: homeScreen
                        function onEditModeChanged() {
                            if (!homeScreen.editMode && delegateItem.dragging) {
                                delegateItem.dragging = false
                                delegateItem.opacity = 1.0
                                delegateItem.scale = 1.0
                                delegateItem.x = Qt.binding(function() { return model.column * gridContainer.cellWidth })
                                delegateItem.y = Qt.binding(function() { return model.row * gridContainer.cellHeight })
                            }
                        }
                    }

                    // Snap-back animation
                    NumberAnimation on x {
                        id: snapBackX
                        running: false
                        duration: 200
                        easing.type: Easing.OutCubic
                    }
                    NumberAnimation on y {
                        id: snapBackY
                        running: false
                        duration: 200
                        easing.type: Easing.OutCubic
                    }

                    // Inner content with gutter margins
                    Item {
                        id: innerContent
                        anchors.fill: parent
                        anchors.margins: UiMetrics.spacing / 2

                        // Glass card background
                        Rectangle {
                            anchors.fill: parent
                            radius: UiMetrics.radius
                            color: ThemeService.surfaceContainer
                            opacity: model.opacity
                        }

                        // Widget content
                        Loader {
                            id: widgetLoader
                            anchors.fill: parent
                            source: model.qmlComponent
                            asynchronous: false

                            // Expose requestContextMenu for widgets with own MouseAreas
                            function requestContextMenu() {
                                if (homeScreen.editMode) {
                                    // In edit mode, long-press initiates drag instead of context menu
                                    widgetMouseArea.initiateDrag()
                                    return
                                }
                                homeScreen._targetCol = model.column
                                homeScreen._targetRow = model.row
                                homeScreen._targetInstanceId = model.instanceId
                                contextMenuOverlay.anchorX = innerContent.mapToItem(homeScreen, innerContent.width / 2, innerContent.height / 2).x
                                contextMenuOverlay.anchorY = innerContent.mapToItem(homeScreen, innerContent.width / 2, innerContent.height / 2).y
                                contextMenuOverlay.visible = true
                                inactivityTimer.restart()
                            }

                            onStatusChanged: {
                                if (status === Loader.Error)
                                    console.warn("HomeMenu: failed to load widget", model.qmlComponent)
                            }
                        }

                        // Long-press / tap detector behind widget content
                        MouseArea {
                            id: widgetMouseArea
                            anchors.fill: parent
                            z: -1
                            pressAndHoldInterval: homeScreen.editMode ? 200 : 500

                            function initiateDrag() {
                                delegateItem.dragging = true
                                homeScreen.draggingInstanceId = model.instanceId
                                delegateItem.originalX = delegateItem.x
                                delegateItem.originalY = delegateItem.y
                                delegateItem.pressOffsetX = delegateItem.width / 2
                                delegateItem.pressOffsetY = delegateItem.height / 2
                                // Visual feedback
                                delegateItem.opacity = 0.5
                                delegateItem.scale = 1.05
                                // Show placeholder at original position
                                dragPlaceholder.x = delegateItem.originalX
                                dragPlaceholder.y = delegateItem.originalY
                                dragPlaceholder.width = model.colSpan * gridContainer.cellWidth
                                dragPlaceholder.height = model.rowSpan * gridContainer.cellHeight
                                dragPlaceholder.visible = true
                                inactivityTimer.restart()
                            }

                            onPressAndHold: function(mouse) {
                                if (!homeScreen.editMode) {
                                    homeScreen.editMode = true
                                    return
                                }
                                // Edit mode: initiate drag with actual press position
                                delegateItem.pressOffsetX = mouse.x
                                delegateItem.pressOffsetY = mouse.y
                                initiateDrag()
                                // Restore actual press offset (initiateDrag sets center default)
                                delegateItem.pressOffsetX = mouse.x
                                delegateItem.pressOffsetY = mouse.y
                            }

                            onPositionChanged: function(mouse) {
                                if (!delegateItem.dragging) return
                                // Move delegate visually (breaks binding -- intentional)
                                var mapped = mapToItem(gridContainer, mouse.x - delegateItem.pressOffsetX, mouse.y - delegateItem.pressOffsetY)
                                delegateItem.x = mapped.x
                                delegateItem.y = mapped.y
                                // Compute target cell
                                var targetCol = Math.round(delegateItem.x / gridContainer.cellWidth)
                                var targetRow = Math.round(delegateItem.y / gridContainer.cellHeight)
                                targetCol = Math.max(0, Math.min(targetCol, WidgetGridModel.gridColumns - model.colSpan))
                                targetRow = Math.max(0, Math.min(targetRow, WidgetGridModel.gridRows - model.rowSpan))
                                // Check validity
                                var valid = WidgetGridModel.canPlace(targetCol, targetRow, model.colSpan, model.rowSpan, model.instanceId)
                                // Update drop highlight
                                dropHighlight.x = targetCol * gridContainer.cellWidth
                                dropHighlight.y = targetRow * gridContainer.cellHeight
                                dropHighlight.width = model.colSpan * gridContainer.cellWidth
                                dropHighlight.height = model.rowSpan * gridContainer.cellHeight
                                dropHighlight.isValid = valid
                                dropHighlight.visible = true
                                inactivityTimer.restart()
                            }

                            onReleased: function(mouse) {
                                if (!delegateItem.dragging) return
                                delegateItem.dragging = false
                                homeScreen.draggingInstanceId = ""
                                dropHighlight.visible = false
                                dragPlaceholder.visible = false
                                // Restore visual state
                                delegateItem.opacity = 1.0
                                delegateItem.scale = 1.0
                                // Compute final target cell
                                var targetCol = Math.round(delegateItem.x / gridContainer.cellWidth)
                                var targetRow = Math.round(delegateItem.y / gridContainer.cellHeight)
                                targetCol = Math.max(0, Math.min(targetCol, WidgetGridModel.gridColumns - model.colSpan))
                                targetRow = Math.max(0, Math.min(targetRow, WidgetGridModel.gridRows - model.rowSpan))
                                if (WidgetGridModel.moveWidget(model.instanceId, targetCol, targetRow)) {
                                    // Success -- rebind position to model
                                    delegateItem.x = Qt.binding(function() { return model.column * gridContainer.cellWidth })
                                    delegateItem.y = Qt.binding(function() { return model.row * gridContainer.cellHeight })
                                } else {
                                    // Failed -- animate snap back to original position
                                    snapBackX.to = delegateItem.originalX
                                    snapBackY.to = delegateItem.originalY
                                    snapBackX.running = true
                                    snapBackY.running = true
                                    // Rebind after animation completes
                                    snapBackTimer.start()
                                }
                            }

                            onClicked: {
                                if (homeScreen.editMode && !delegateItem.dragging) {
                                    widgetLoader.requestContextMenu()
                                }
                            }
                        }

                        // Timer to rebind position after snap-back animation
                        Timer {
                            id: snapBackTimer
                            interval: 250  // slightly longer than snap-back animation
                            onTriggered: {
                                delegateItem.x = Qt.binding(function() { return model.column * gridContainer.cellWidth })
                                delegateItem.y = Qt.binding(function() { return model.row * gridContainer.cellHeight })
                            }
                        }

                        // --- Edit mode overlays ---

                        // Accent border
                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            border.width: 2
                            border.color: ThemeService.primary
                            radius: UiMetrics.radius
                            visible: homeScreen.editMode
                            z: 10
                        }

                        // X remove badge (top-right)
                        Rectangle {
                            id: removeBadge
                            width: 28
                            height: 28
                            radius: 14
                            color: ThemeService.error
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.rightMargin: -6
                            anchors.topMargin: -6
                            visible: homeScreen.editMode
                            z: 20

                            MaterialIcon {
                                icon: "\ue5cd"
                                size: 16
                                color: ThemeService.onError
                                anchors.centerIn: parent
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    WidgetGridModel.removeWidget(model.instanceId)
                                    inactivityTimer.restart()
                                }
                            }
                        }

                        // Resize handle (bottom-right) with drag-to-resize
                        Rectangle {
                            id: resizeHandle
                            width: UiMetrics.touchMin * 0.5
                            height: UiMetrics.touchMin * 0.5
                            radius: UiMetrics.radiusSmall
                            color: ThemeService.primary
                            opacity: 0.7
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.rightMargin: UiMetrics.spacing * 0.25
                            anchors.bottomMargin: UiMetrics.spacing * 0.25
                            visible: homeScreen.editMode
                            z: 20

                            MouseArea {
                                anchors.fill: parent
                                // Expand touch area for easier grabbing
                                anchors.margins: -UiMetrics.spacing / 2
                                preventStealing: true

                                property point startGlobal: Qt.point(0, 0)
                                property int startColSpan: 0
                                property int startRowSpan: 0
                                property int proposedColSpan: 0
                                property int proposedRowSpan: 0
                                property bool resizing: false
                                property int lastClampedCols: 0
                                property int lastClampedRows: 0

                                onPressed: function(mouse) {
                                    resizing = true
                                    homeScreen.resizingInstanceId = model.instanceId
                                    startGlobal = mapToItem(gridContainer, mouse.x, mouse.y)
                                    startColSpan = model.colSpan
                                    startRowSpan = model.rowSpan
                                    proposedColSpan = startColSpan
                                    proposedRowSpan = startRowSpan
                                    lastClampedCols = startColSpan
                                    lastClampedRows = startRowSpan

                                    // Position ghost at current widget bounds
                                    resizeGhost.x = model.column * gridContainer.cellWidth
                                    resizeGhost.y = model.row * gridContainer.cellHeight
                                    resizeGhost.width = model.colSpan * gridContainer.cellWidth
                                    resizeGhost.height = model.rowSpan * gridContainer.cellHeight
                                    resizeGhost.visible = true
                                    resizeGhost.isValid = true

                                    inactivityTimer.restart()
                                    mouse.accepted = true
                                }

                                onPositionChanged: function(mouse) {
                                    if (!resizing) return
                                    var current = mapToItem(gridContainer, mouse.x, mouse.y)
                                    var deltaCols = Math.round((current.x - startGlobal.x) / gridContainer.cellWidth)
                                    var deltaRows = Math.round((current.y - startGlobal.y) / gridContainer.cellHeight)

                                    var newColSpan = startColSpan + deltaCols
                                    var newRowSpan = startRowSpan + deltaRows

                                    // Clamp to min/max constraints
                                    var wasAtLimit = false
                                    if (newColSpan < model.minCols) { newColSpan = model.minCols; wasAtLimit = true }
                                    if (newColSpan > model.maxCols) { newColSpan = model.maxCols; wasAtLimit = true }
                                    if (newRowSpan < model.minRows) { newRowSpan = model.minRows; wasAtLimit = true }
                                    if (newRowSpan > model.maxRows) { newRowSpan = model.maxRows; wasAtLimit = true }

                                    // Clamp to grid bounds
                                    if (model.column + newColSpan > WidgetGridModel.gridColumns) {
                                        newColSpan = WidgetGridModel.gridColumns - model.column
                                        wasAtLimit = true
                                    }
                                    if (model.row + newRowSpan > WidgetGridModel.gridRows) {
                                        newRowSpan = WidgetGridModel.gridRows - model.row
                                        wasAtLimit = true
                                    }

                                    // Ensure minimum 1
                                    newColSpan = Math.max(1, newColSpan)
                                    newRowSpan = Math.max(1, newRowSpan)

                                    proposedColSpan = newColSpan
                                    proposedRowSpan = newRowSpan

                                    // Check if proposed size overlaps other widgets
                                    var valid = WidgetGridModel.canPlace(model.column, model.row,
                                        newColSpan, newRowSpan, model.instanceId)
                                    resizeGhost.isValid = valid

                                    // Flash on boundary hit (only when value actually changed to limit)
                                    if (wasAtLimit && (newColSpan !== lastClampedCols || newRowSpan !== lastClampedRows)) {
                                        limitFlash.restart()
                                    }
                                    lastClampedCols = newColSpan
                                    lastClampedRows = newRowSpan

                                    // Update ghost size
                                    resizeGhost.width = newColSpan * gridContainer.cellWidth
                                    resizeGhost.height = newRowSpan * gridContainer.cellHeight

                                    inactivityTimer.restart()
                                }

                                onReleased: {
                                    resizing = false
                                    homeScreen.resizingInstanceId = ""
                                    resizeGhost.visible = false

                                    if (proposedColSpan !== startColSpan || proposedRowSpan !== startRowSpan) {
                                        // Atomic write on release (EDIT-09)
                                        WidgetGridModel.resizeWidget(model.instanceId, proposedColSpan, proposedRowSpan)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Launcher dock -- fixed at bottom
        LauncherDock {
            Layout.fillWidth: true
        }
    }

    // ---- FAB (floating action button) for adding widgets ----
    Rectangle {
        id: fab
        width: UiMetrics.touchMin * 1.2
        height: width
        radius: width / 2
        color: ThemeService.primary
        visible: homeScreen.editMode
        z: 50
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: UiMetrics.marginPage * 2
        anchors.bottomMargin: UiMetrics.marginPage * 2 + UiMetrics.touchMin + UiMetrics.spacing

        MaterialIcon {
            icon: "\ue145"
            size: UiMetrics.iconSize
            color: ThemeService.onPrimary
            anchors.centerIn: parent
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                inactivityTimer.restart()
                // Check if there's any space at all (1x1 minimum)
                var cell = WidgetGridModel.findFirstAvailableCell(1, 1)
                if (cell.col < 0) {
                    toast.show("No space available \u2014 remove a widget first")
                } else {
                    WidgetPickerModel.filterByAvailableSpace(
                        WidgetGridModel.gridColumns,
                        WidgetGridModel.gridRows)
                    pickerOverlay.visible = true
                }
            }
        }
    }

    // ---- Context menu overlay (for existing widgets) ----
    Item {
        id: contextMenuOverlay
        anchors.fill: parent
        visible: false
        z: 100

        property real anchorX: 0
        property real anchorY: 0

        // Scrim
        Rectangle {
            anchors.fill: parent
            color: ThemeService.scrim
            opacity: 0.4
            MouseArea {
                anchors.fill: parent
                onClicked: contextMenuOverlay.visible = false
            }
        }

        // Floating card
        Rectangle {
            id: ctxCard
            width: Math.min(280, parent.width * 0.4)
            height: ctxContent.implicitHeight + UiMetrics.spacing * 2
            x: Math.max(UiMetrics.spacing, Math.min(contextMenuOverlay.anchorX - width / 2, parent.width - width - UiMetrics.spacing))
            y: Math.max(UiMetrics.spacing, Math.min(contextMenuOverlay.anchorY - height / 2, parent.height - height - UiMetrics.spacing))
            radius: UiMetrics.radiusLarge
            color: ThemeService.surfaceContainerHighest

            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "transparent"
                border.width: 1
                border.color: ThemeService.outlineVariant
                opacity: 0.4
            }

            scale: contextMenuOverlay.visible ? 1.0 : 0.8
            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

            ColumnLayout {
                id: ctxContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: UiMetrics.spacing
                spacing: UiMetrics.gap * 0.5

                // Change Widget
                Rectangle {
                    Layout.fillWidth: true
                    height: UiMetrics.touchMin
                    radius: UiMetrics.radiusSmall
                    color: changeMA.pressed ? ThemeService.primaryContainer : ThemeService.surfaceContainer

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: UiMetrics.spacing
                        anchors.rightMargin: UiMetrics.spacing
                        spacing: UiMetrics.spacing
                        MaterialIcon { icon: "\ue41f"; size: UiMetrics.iconSmall; color: ThemeService.onSurface }
                        NormalText { text: "Change Widget"; font.pixelSize: UiMetrics.fontBody; color: ThemeService.onSurface; Layout.fillWidth: true }
                    }
                    MouseArea {
                        id: changeMA
                        anchors.fill: parent
                        onClicked: {
                            contextMenuOverlay.visible = false
                            // Remove existing, open picker at same cell
                            WidgetGridModel.removeWidget(homeScreen._targetInstanceId)
                            WidgetPickerModel.filterByAvailableSpace(
                                WidgetGridModel.gridColumns - homeScreen._targetCol,
                                WidgetGridModel.gridRows - homeScreen._targetRow)
                            pickerOverlay.visible = true
                            inactivityTimer.restart()
                        }
                    }
                }

                // Clear
                Rectangle {
                    Layout.fillWidth: true
                    height: UiMetrics.touchMin
                    radius: UiMetrics.radiusSmall
                    color: clearMA.pressed ? ThemeService.errorContainer : ThemeService.surfaceContainer

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: UiMetrics.spacing
                        anchors.rightMargin: UiMetrics.spacing
                        spacing: UiMetrics.spacing
                        MaterialIcon { icon: "\ue5cd"; size: UiMetrics.iconSmall; color: ThemeService.error }
                        NormalText { text: "Clear"; font.pixelSize: UiMetrics.fontBody; color: ThemeService.error; Layout.fillWidth: true }
                    }
                    MouseArea {
                        id: clearMA
                        anchors.fill: parent
                        onClicked: {
                            WidgetGridModel.removeWidget(homeScreen._targetInstanceId)
                            contextMenuOverlay.visible = false
                            inactivityTimer.restart()
                        }
                    }
                }
            }
        }
    }

    // ---- Widget picker overlay ----
    Item {
        id: pickerOverlay
        anchors.fill: parent
        visible: false
        z: 100

        onVisibleChanged: {
            if (visible) inactivityTimer.restart()
        }

        // Scrim
        Rectangle {
            anchors.fill: parent
            color: ThemeService.scrim
            opacity: 0.4
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    pickerOverlay.visible = false
                    inactivityTimer.restart()
                }
            }
        }

        // Picker panel (bottom sheet)
        Rectangle {
            id: pickerPanel
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: UiMetrics.marginPage
            height: Math.min(parent.height * 0.35, 200)
            radius: UiMetrics.radiusLarge
            color: ThemeService.surfaceContainerHighest

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: UiMetrics.spacing
                spacing: UiMetrics.spacing * 0.5

                NormalText {
                    text: "Choose a widget"
                    font.pixelSize: UiMetrics.fontTitle
                    font.bold: true
                    color: ThemeService.onSurface
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    orientation: ListView.Horizontal
                    spacing: UiMetrics.spacing
                    clip: true

                    model: WidgetPickerModel

                    delegate: Rectangle {
                        width: Math.max(120, pickerPanel.width * 0.18)
                        height: ListView.view.height
                        radius: UiMetrics.radiusSmall
                        color: pickMA.pressed ? ThemeService.primaryContainer : ThemeService.surfaceContainer

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: UiMetrics.spacing * 0.25

                            MaterialIcon {
                                icon: model.iconName
                                size: UiMetrics.iconSize
                                color: ThemeService.onSurface
                                Layout.alignment: Qt.AlignHCenter
                            }
                            NormalText {
                                text: model.displayName
                                font.pixelSize: UiMetrics.fontSmall
                                color: ThemeService.onSurface
                                Layout.alignment: Qt.AlignHCenter
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        MouseArea {
                            id: pickMA
                            anchors.fill: parent
                            onClicked: {
                                if (homeScreen.editMode) {
                                    // Auto-place mode: find first available cell
                                    var defCols = model.defaultCols > 0 ? model.defaultCols : 2
                                    var defRows = model.defaultRows > 0 ? model.defaultRows : 2
                                    // Clamp to grid dimensions
                                    defCols = Math.min(defCols, WidgetGridModel.gridColumns)
                                    defRows = Math.min(defRows, WidgetGridModel.gridRows)

                                    var cell = WidgetGridModel.findFirstAvailableCell(defCols, defRows)
                                    if (cell.col < 0) {
                                        // Try with 1x1 as fallback
                                        cell = WidgetGridModel.findFirstAvailableCell(1, 1)
                                    }
                                    if (cell.col >= 0) {
                                        // Clamp span to available space from found cell
                                        var placeCols = Math.min(defCols, WidgetGridModel.gridColumns - cell.col)
                                        var placeRows = Math.min(defRows, WidgetGridModel.gridRows - cell.row)
                                        WidgetGridModel.placeWidget(model.widgetId,
                                            cell.col, cell.row, placeCols, placeRows)
                                    } else {
                                        toast.show("No space available \u2014 remove a widget first")
                                    }
                                    pickerOverlay.visible = false
                                    inactivityTimer.restart()
                                } else {
                                    // Legacy targeted placement (from context menu "Change Widget")
                                    var maxCols = WidgetGridModel.gridColumns - homeScreen._targetCol
                                    var maxRows = WidgetGridModel.gridRows - homeScreen._targetRow
                                    var cols = Math.min(2, maxCols)
                                    var rows = Math.min(2, maxRows)
                                    WidgetGridModel.placeWidget(model.widgetId,
                                        homeScreen._targetCol, homeScreen._targetRow,
                                        cols, rows)
                                    pickerOverlay.visible = false
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- Toast (transient feedback) ----
    Toast {
        id: toast
        z: 200
    }
}
