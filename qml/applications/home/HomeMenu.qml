import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: homeScreen
    anchors.fill: parent

    Component.onCompleted: ApplicationController.setTitle("Home")

    // Keep WidgetContextFactory cell size in sync with snapped grid cell size
    Binding { target: WidgetContextFactory; property: "cellSide"; value: Math.round(homeScreen.cellSide) }

    // Per-widget selection state (replaces global editMode)
    property string selectedInstanceId: ""
    property bool _addingPage: false
    property bool _skipPageCleanup: false

    // Track active drag/resize for cleanup on deselect
    property string draggingInstanceId: ""
    property string resizingInstanceId: ""
    property var draggingDelegate: null
    property int _dragColSpan: 1
    property int _dragRowSpan: 1

    // Edge resize proposed grid position (ghost tracks these)
    property int proposedCol: 0
    property int proposedRow: 0
    property int proposedColSpan: 0
    property int proposedRowSpan: 0

    // Snap-aware grid frame computation (two-stage: base -> snap -> effective)
    readonly property real baseCellSide: DisplayInfo ? DisplayInfo.cellSide : 120
    readonly property real kSnapThreshold: 0.5  // Add row/col when waste > 50% of cell

    // Stage 1: initial grid from base cell size
    readonly property int _baseCols: pageView.width > 0 ? Math.max(3, Math.floor(pageView.width / baseCellSide)) : 3
    readonly property int _baseRows: pageView.height > 0 ? Math.max(2, Math.floor(pageView.height / baseCellSide)) : 2

    // Stage 2: first snap pass -- recover wasted gutter space
    readonly property real _xWaste1: pageView.width - _baseCols * baseCellSide
    readonly property real _yWaste1: pageView.height - _baseRows * baseCellSide
    readonly property int _snap1Cols: _xWaste1 > kSnapThreshold * baseCellSide ? _baseCols + 1 : _baseCols
    readonly property int _snap1Rows: _yWaste1 > kSnapThreshold * baseCellSide ? _baseRows + 1 : _baseRows

    // Stage 3: second snap pass -- only for axes that did NOT snap in pass 1.
    // Pass 1 may shrink cellSide (e.g., Y snaps 3->4, cell shrinks 132->125),
    // creating new waste on the other axis. Pass 2 catches that cascaded waste.
    // An axis that already snapped in pass 1 is NOT eligible for pass 2
    // to prevent iterative packing (e.g., 7x4 -> 8x5 -> 9x5).
    readonly property bool _xSnapped: _snap1Cols > _baseCols
    readonly property bool _ySnapped: _snap1Rows > _baseRows
    readonly property real _snap1CellSide: (_snap1Cols > 0 && _snap1Rows > 0)
        ? Math.min(pageView.width / _snap1Cols, pageView.height / _snap1Rows) : baseCellSide
    readonly property real _xWaste2: pageView.width - _snap1Cols * _snap1CellSide
    readonly property real _yWaste2: pageView.height - _snap1Rows * _snap1CellSide
    readonly property int gridCols: (!_xSnapped && _xWaste2 > kSnapThreshold * _snap1CellSide) ? _snap1Cols + 1 : _snap1Cols
    readonly property int gridRows: (!_ySnapped && _yWaste2 > kSnapThreshold * _snap1CellSide) ? _snap1Rows + 1 : _snap1Rows

    // Effective cell size (square cells, fit both axes after both snap passes)
    readonly property real cellSide: gridCols > 0 && gridRows > 0
        ? Math.min(pageView.width / gridCols, pageView.height / gridRows) : baseCellSide
    readonly property real gridW: gridCols * cellSide
    readonly property real gridH: gridRows * cellSide
    readonly property real offsetX: (pageView.width - gridW) / 2
    readonly property real offsetY: (pageView.height - gridH) / 2

    // Legacy aliases for backward compat in overlays
    readonly property real cellWidth: cellSide
    readonly property real cellHeight: cellSide

    // Grid is invisible until first valid layout
    property bool gridReady: false

    // Push grid dimensions to C++ model when they change (batched via callLater
    // to avoid intermediate states when cols and rows update independently)
    function _pushGridDims() {
        if (gridCols > 0 && gridRows > 0 && pageView.width > 0 && pageView.height > 0) {
            WidgetGridModel.setGridDimensions(gridCols, gridRows)
            gridReady = true
        }
    }
    onGridColsChanged: Qt.callLater(_pushGridDims)
    onGridRowsChanged: Qt.callLater(_pushGridDims)

    // Select a widget by instanceId
    function selectWidget(instanceId) {
        if (selectedInstanceId === instanceId) return
        selectedInstanceId = instanceId
        WidgetGridModel.setWidgetSelected(true)
        // Set navbar metadata for widget interaction mode
        var meta = WidgetGridModel.widgetMeta(instanceId)
        NavbarController.widgetDisplayName = meta.displayName || ""
        NavbarController.selectedWidgetHasConfig = meta.hasConfigSchema || false
        NavbarController.selectedWidgetIsSingleton = meta.isSingleton || false
        selectionTimer.restart()
    }

    // Deselect the currently selected widget and clean up all interaction state
    function deselectWidget() {
        selectedInstanceId = ""
        WidgetGridModel.setWidgetSelected(false)
        // Clear navbar metadata
        NavbarController.widgetDisplayName = ""
        NavbarController.selectedWidgetHasConfig = false
        NavbarController.selectedWidgetIsSingleton = false
        selectionTimer.stop()
        // Reset any active drag/resize state
        dropHighlight.visible = false
        dragPlaceholder.visible = false
        resizeGhost.visible = false
        if (draggingInstanceId !== "") {
            draggingInstanceId = ""
            draggingDelegate = null
        }
        resizingInstanceId = ""
        widgetPickerSheet.closePicker()
        // Auto-clean empty pages (except page 0 and current page) after settling
        // Skip when trash handler already did page cleanup (avoids shifted-index race)
        if (!_skipPageCleanup) {
            var currentPage = pageView.currentIndex
            Qt.callLater(function() {
                for (var i = WidgetGridModel.pageCount - 1; i > 0; i--) {
                    if (i !== currentPage && WidgetGridModel.widgetCountOnPage(i) === 0) {
                        WidgetGridModel.removePage(i)
                    }
                }
                if (pageView.currentIndex >= WidgetGridModel.pageCount)
                    pageView.setCurrentIndex(WidgetGridModel.pageCount - 1)
            })
        }
    }

    // --- Edge resize helper functions ---

    // Start edge resize -- initialize ghost at current widget position
    // pageContainer: the pageGridContent Item (passed from delegate context)
    function startEdgeResize(instanceId, col, row, colSpan, rowSpan, pageContainer) {
        resizingInstanceId = instanceId
        proposedCol = col
        proposedRow = row
        proposedColSpan = colSpan
        proposedRowSpan = rowSpan

        var ghostPos = pageContainer.mapToItem(homeScreen,
            offsetX + col * cellSide, offsetY + row * cellSide)
        resizeGhost.x = ghostPos.x
        resizeGhost.y = ghostPos.y
        resizeGhost.width = colSpan * cellSide
        resizeGhost.height = rowSpan * cellSide
        resizeGhost.visible = true
        resizeGhost.isValid = true
    }

    // Update edge resize ghost with new proposed grid position
    // pageContainer: the pageGridContent Item (passed from delegate context)
    function updateEdgeResize(newCol, newRow, newColSpan, newRowSpan, instanceId, pageContainer, descriptorLimitHit) {
        var wasAtLimit = descriptorLimitHit || false
        // Grid bounds clamping
        if (newCol < 0) { newCol = 0; wasAtLimit = true }
        if (newRow < 0) { newRow = 0; wasAtLimit = true }
        if (newColSpan < 1) newColSpan = 1
        if (newRowSpan < 1) newRowSpan = 1
        if (newCol + newColSpan > gridCols) {
            newColSpan = gridCols - newCol
            wasAtLimit = true
        }
        if (newRow + newRowSpan > gridRows) {
            newRowSpan = gridRows - newRow
            wasAtLimit = true
        }

        // Detect limit change (fire flash only on newly-hit limits)
        if (wasAtLimit && (newCol !== proposedCol || newRow !== proposedRow ||
                           newColSpan !== proposedColSpan || newRowSpan !== proposedRowSpan)) {
            limitFlash.restart()
        }

        proposedCol = newCol
        proposedRow = newRow
        proposedColSpan = newColSpan
        proposedRowSpan = newRowSpan

        var valid = WidgetGridModel.canPlace(newCol, newRow, newColSpan, newRowSpan, instanceId)
        resizeGhost.isValid = valid

        var ghostPos = pageContainer.mapToItem(homeScreen,
            offsetX + newCol * cellSide, offsetY + newRow * cellSide)
        resizeGhost.x = ghostPos.x
        resizeGhost.y = ghostPos.y
        resizeGhost.width = newColSpan * cellSide
        resizeGhost.height = newRowSpan * cellSide
    }

    // Commit the edge resize if ghost shows valid
    function commitEdgeResize(instanceId) {
        resizingInstanceId = ""
        resizeGhost.visible = false

        if (resizeGhost.isValid) {
            WidgetGridModel.resizeWidgetFromEdge(instanceId,
                proposedCol, proposedRow, proposedColSpan, proposedRowSpan)
        }
    }

    function cancelEdgeResize() {
        resizingInstanceId = ""
        resizeGhost.visible = false
    }

    // Auto-deselect timer -- clears selection after 10s of inactivity
    Timer {
        id: selectionTimer
        interval: 10000
        running: homeScreen.selectedInstanceId !== "" && !configSheet.isOpen && !widgetPickerSheet.isOpen
        onTriggered: homeScreen.deselectWidget()
    }

    // Navbar widget interaction mode: active when widget is selected and NOT dragging
    Binding {
        target: NavbarController
        property: "widgetInteractionMode"
        value: homeScreen.selectedInstanceId !== "" && homeScreen.draggingInstanceId === ""
    }

    // Navbar widget action signals
    Connections {
        target: NavbarController
        function onWidgetConfigRequested() {
            if (homeScreen.selectedInstanceId === "") return
            var meta = WidgetGridModel.widgetMeta(homeScreen.selectedInstanceId)
            if (!meta.hasConfigSchema) return
            configSheet.openConfig(homeScreen.selectedInstanceId, meta.widgetId,
                                   meta.displayName, meta.iconName)
            selectionTimer.restart()
        }
        function onWidgetDeleteRequested() {
            if (homeScreen.selectedInstanceId === "") return
            var meta = WidgetGridModel.widgetMeta(homeScreen.selectedInstanceId)
            if (meta.isSingleton) return
            var deletedPage = WidgetGridModel.activePage
            var widgetCountBefore = WidgetGridModel.widgetCountOnPage(deletedPage)
            WidgetGridModel.removeWidget(homeScreen.selectedInstanceId)
            // PGM-04: if that was the last widget on this page, auto-delete the page
            if (widgetCountBefore === 1 && deletedPage > 0) {
                homeScreen._addingPage = true  // prevent deselect side-effect from page change
                pageView.setCurrentIndex(deletedPage - 1)
                homeScreen._addingPage = false
                Qt.callLater(function() {
                    WidgetGridModel.removePage(deletedPage)
                })
            }
            // Skip deselectWidget's deferred empty-page cleanup since we already handled
            // page deletion above. removePage() shifts page indices, so the deferred
            // cleanup in deselectWidget() would iterate over wrong page indices.
            homeScreen._skipPageCleanup = true
            homeScreen.deselectWidget()
            homeScreen._skipPageCleanup = false
        }
    }

    // C++ navigation deselect bridge — when main.cpp calls setWidgetSelected(false),
    // sync QML selection state so visuals, SwipeView lock, and timer all clear
    Connections {
        target: WidgetGridModel
        function onWidgetDeselectedFromCpp() {
            emptySpaceMenu.close()
            widgetPickerSheet.closePicker()
            if (homeScreen.selectedInstanceId !== "")
                homeScreen.deselectWidget()
        }
    }

    // Plugin activation overlay cleanup (AA fullscreen, plugin launch, etc.)
    Connections {
        target: PluginModel
        function onActivePluginChanged() {
            // Always dismiss no-selection overlays (menu/picker) on ANY plugin change
            emptySpaceMenu.close()
            widgetPickerSheet.closePicker()
            // Also deselect widget if fullscreen plugin activates while selected
            if (PluginModel.activePluginFullscreen && homeScreen.selectedInstanceId !== "") {
                configSheet.closeConfig()
                homeScreen.deselectWidget()
            }
        }
    }

    // Dismiss no-selection overlays when navigating to settings or other built-in views
    Connections {
        target: ApplicationController
        function onCurrentApplicationChanged() {
            emptySpaceMenu.close()
            widgetPickerSheet.closePicker()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        // Multi-page SwipeView -- fills available space above page dots
        SwipeView {
            id: pageView
            Layout.fillWidth: true
            Layout.fillHeight: true
            interactive: homeScreen.selectedInstanceId === ""
            clip: true

            Component.onCompleted: {
                contentItem.boundsBehavior = Flickable.DragAndOvershootBounds
                contentItem.highlightMoveDuration = 350
            }

            onCurrentIndexChanged: {
                WidgetGridModel.activePage = currentIndex
                emptySpaceMenu.close()
                if (homeScreen.selectedInstanceId !== "" && !homeScreen._addingPage)
                    homeScreen.deselectWidget()
            }

            Repeater {
                model: WidgetGridModel.pageCount

                // Each page is a Loader for lazy instantiation (PAGE-08)
                Loader {
                    active: SwipeView.isCurrentItem
                            || SwipeView.isNextItem
                            || SwipeView.isPreviousItem

                    property int pageIndex: index

                    sourceComponent: Component {
                        Item {
                            id: pageGridContent

                            // Background tap detector for empty grid space
                            MouseArea {
                                anchors.fill: parent
                                pressAndHoldInterval: 500
                                onPressAndHold: function(mouse) {
                                    // Gesture arbitration: only when no widget is selected
                                    if (homeScreen.selectedInstanceId !== "") return
                                    // Bounds check: only within the actual grid area, not gutter margins
                                    if (mouse.x < homeScreen.offsetX || mouse.x > homeScreen.offsetX + homeScreen.gridW) return
                                    if (mouse.y < homeScreen.offsetY || mouse.y > homeScreen.offsetY + homeScreen.gridH) return
                                    // Position popup near the touch point
                                    var globalPos = mapToItem(Overlay.overlay, mouse.x, mouse.y)
                                    emptySpaceMenu.x = Math.min(globalPos.x, Overlay.overlay.width - emptySpaceMenu.width)
                                    emptySpaceMenu.y = Math.min(globalPos.y, Overlay.overlay.height - emptySpaceMenu.height)
                                    emptySpaceMenu.open()
                                }
                                onClicked: {
                                    if (homeScreen.selectedInstanceId !== "")
                                        homeScreen.deselectWidget()
                                }
                            }

                            // Dotted grid lines overlay (visible when widget selected)
                            Item {
                                anchors.fill: parent
                                visible: homeScreen.selectedInstanceId !== ""
                                z: 0

                                // Vertical lines (interior only -- skip outer edges to avoid clip)
                                Repeater {
                                    model: Math.max(0, homeScreen.gridCols - 1)
                                    delegate: Column {
                                        x: homeScreen.offsetX + (index + 1) * homeScreen.cellSide
                                        y: homeScreen.offsetY
                                        height: homeScreen.gridH
                                        spacing: UiMetrics.spacing
                                        Repeater {
                                            model: Math.ceil(homeScreen.gridH / (UiMetrics.spacing + 2))
                                            delegate: Rectangle {
                                                width: 1
                                                height: 2
                                                color: ThemeService.outlineVariant
                                                opacity: 0.3
                                            }
                                        }
                                    }
                                }

                                // Horizontal lines (interior only -- skip outer edges to avoid clip)
                                Repeater {
                                    model: Math.max(0, homeScreen.gridRows - 1)
                                    delegate: Row {
                                        x: homeScreen.offsetX
                                        y: homeScreen.offsetY + (index + 1) * homeScreen.cellSide
                                        width: homeScreen.gridW
                                        spacing: UiMetrics.spacing
                                        Repeater {
                                            model: Math.ceil(pageGridContent.width / (UiMetrics.spacing + 2))
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

                            // Widget Repeater -- shows only widgets on this page
                            Repeater {
                                id: widgetRepeater
                                model: WidgetGridModel

                                delegate: Item {
                                    id: delegateItem
                                    x: homeScreen.offsetX + model.column * homeScreen.cellSide
                                    y: homeScreen.offsetY + model.row * homeScreen.cellSide
                                    width: model.colSpan * homeScreen.cellSide
                                    height: model.rowSpan * homeScreen.cellSide
                                    visible: model.page === pageIndex && model.visible
                                    z: dragging ? 100 : (isSelected ? 50 : 1)

                                    readonly property bool isSelected: model.instanceId === homeScreen.selectedInstanceId

                                    // Widget context for provider access and layout info
                                    property QtObject widgetCtx: WidgetContextFactory.createContext(model.instanceId, delegateItem)

                                    // Keep spans synced when model changes (resize)
                                    Binding { target: widgetCtx; property: "colSpan"; value: model.colSpan; when: widgetCtx !== null }
                                    Binding { target: widgetCtx; property: "rowSpan"; value: model.rowSpan; when: widgetCtx !== null }

                                    // Keep cell dimensions reactive (cellSide can change on resize/remap)
                                    Binding { target: widgetCtx; property: "cellWidth"; value: Math.round(homeScreen.cellSide); when: widgetCtx !== null }
                                    Binding { target: widgetCtx; property: "cellHeight"; value: Math.round(homeScreen.cellSide); when: widgetCtx !== null }

                                    // isCurrentPage: true when this widget's page is the active SwipeView page
                                    Binding { target: widgetCtx; property: "isCurrentPage"; value: model.page === pageView.currentIndex; when: widgetCtx !== null }

                                    // Drag state
                                    property bool dragging: false
                                    property real dragStartX: 0
                                    property real dragStartY: 0
                                    property real pressOffsetX: 0
                                    property real pressOffsetY: 0
                                    property real originalX: 0
                                    property real originalY: 0

                                    function finalizeDrop() {
                                        if (!dragging) return
                                        dragging = false
                                        homeScreen.draggingInstanceId = ""
                                        homeScreen.draggingDelegate = null
                                        dropHighlight.visible = false
                                        dragPlaceholder.visible = false
                                        opacity = 1.0
                                        scale = 1.0
                                        innerContent.scale = 1.0
                                        liftShadow.visible = false
                                        var targetCol = Math.round((x - homeScreen.offsetX) / homeScreen.cellSide)
                                        var targetRow = Math.round((y - homeScreen.offsetY) / homeScreen.cellSide)
                                        targetCol = Math.max(0, Math.min(targetCol, homeScreen.gridCols - model.colSpan))
                                        targetRow = Math.max(0, Math.min(targetRow, homeScreen.gridRows - model.rowSpan))
                                        if (WidgetGridModel.moveWidget(model.instanceId, targetCol, targetRow)) {
                                            x = Qt.binding(function() { return homeScreen.offsetX + model.column * homeScreen.cellSide })
                                            y = Qt.binding(function() { return homeScreen.offsetY + model.row * homeScreen.cellSide })
                                        } else {
                                            snapBackX.to = originalX
                                            snapBackY.to = originalY
                                            snapBackX.running = true
                                            snapBackY.running = true
                                            snapBackTimer.start()
                                        }
                                    }

                                    // Update drag position from any source MouseArea (interceptor or dragOverlay)
                                    function updateDragFrom(sourceItem, localX, localY) {
                                        var pageLocal = sourceItem.mapToItem(delegateItem.parent, localX, localY)
                                        x = pageLocal.x - pressOffsetX
                                        y = pageLocal.y - pressOffsetY

                                        var cs = homeScreen._dragColSpan
                                        var rs = homeScreen._dragRowSpan
                                        var targetCol = Math.round((x - homeScreen.offsetX) / homeScreen.cellSide)
                                        var targetRow = Math.round((y - homeScreen.offsetY) / homeScreen.cellSide)
                                        targetCol = Math.max(0, Math.min(targetCol, homeScreen.gridCols - cs))
                                        targetRow = Math.max(0, Math.min(targetRow, homeScreen.gridRows - rs))
                                        var valid = WidgetGridModel.canPlace(targetCol, targetRow, cs, rs, homeScreen.draggingInstanceId)

                                        var hlPos = delegateItem.parent.mapToItem(homeScreen,
                                            homeScreen.offsetX + targetCol * homeScreen.cellSide,
                                            homeScreen.offsetY + targetRow * homeScreen.cellSide)
                                        dropHighlight.x = hlPos.x
                                        dropHighlight.y = hlPos.y
                                        dropHighlight.width = cs * homeScreen.cellSide
                                        dropHighlight.height = rs * homeScreen.cellSide
                                        dropHighlight.isValid = valid
                                        dropHighlight.visible = true
                                        selectionTimer.restart()
                                    }

                                    // Force-reset drag state when selection changes (EVIOCGRAB safety)
                                    Connections {
                                        target: homeScreen
                                        function onSelectedInstanceIdChanged() {
                                            if (homeScreen.selectedInstanceId !== model.instanceId) {
                                                // Reset lift visuals when this delegate loses selection
                                                innerContent.scale = 1.0
                                                liftShadow.visible = false
                                                if (delegateItem.dragging) {
                                                    delegateItem.dragging = false
                                                    delegateItem.opacity = 1.0
                                                    delegateItem.scale = 1.0
                                                    delegateItem.x = Qt.binding(function() { return homeScreen.offsetX + model.column * homeScreen.cellSide })
                                                    delegateItem.y = Qt.binding(function() { return homeScreen.offsetY + model.row * homeScreen.cellSide })
                                                }
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

                                    // Timer for lift reset on interactive widgets (forwarded long-press)
                                    Timer {
                                        id: liftResetTimer
                                        interval: 300
                                        onTriggered: {
                                            if (!delegateItem.dragging) {
                                                innerContent.scale = 1.0
                                                liftShadow.visible = false
                                            }
                                        }
                                    }

                                    // Inner content with gutter margins
                                    Item {
                                        id: innerContent
                                        anchors.fill: parent
                                        anchors.margins: Math.max(UiMetrics.spacing / 4, 2)

                                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                                        // Glass card background
                                        Rectangle {
                                            anchors.fill: parent
                                            radius: UiMetrics.radius
                                            color: ThemeService.surfaceContainer
                                            opacity: model.opacity
                                        }

                                        // Lift shadow (visible during press-hold only)
                                        Rectangle {
                                            id: liftShadow
                                            anchors.fill: parent
                                            anchors.margins: -3
                                            z: -1
                                            radius: UiMetrics.radius + 3
                                            color: "#40000000"
                                            visible: false
                                        }

                                        // Widget content
                                        Loader {
                                            id: widgetLoader
                                            anchors.fill: parent
                                            source: model.qmlComponent
                                            asynchronous: false

                                            function requestContextMenu() {
                                                if (homeScreen.selectedInstanceId === model.instanceId) {
                                                    // Already selected -- initiate drag from interactive widget
                                                    innerContent.scale = 1.05
                                                    liftShadow.visible = true
                                                    widgetMouseArea.initiateDrag()
                                                } else {
                                                    // Select this widget with brief lift flash
                                                    innerContent.scale = 1.05
                                                    liftShadow.visible = true
                                                    homeScreen.selectWidget(model.instanceId)
                                                    // Reset scale after a short delay (finger may still be down)
                                                    liftResetTimer.start()
                                                }
                                            }

                                            onLoaded: {
                                                if (item && "widgetContext" in item)
                                                    item.widgetContext = delegateItem.widgetCtx
                                            }

                                            onStatusChanged: {
                                                if (status === Loader.Error)
                                                    console.warn("HomeMenu: failed to load widget", model.qmlComponent)
                                            }
                                        }

                                        // Long-press detector -- always behind widget content (z:-1)
                                        MouseArea {
                                            id: widgetMouseArea
                                            anchors.fill: parent
                                            z: -1
                                            pressAndHoldInterval: 500

                                            function initiateDrag(preservePressOffset) {
                                                delegateItem.dragging = true
                                                homeScreen.draggingInstanceId = model.instanceId
                                                homeScreen.draggingDelegate = delegateItem
                                                homeScreen._dragColSpan = model.colSpan
                                                homeScreen._dragRowSpan = model.rowSpan
                                                delegateItem.originalX = delegateItem.x
                                                delegateItem.originalY = delegateItem.y
                                                if (!preservePressOffset) {
                                                    delegateItem.pressOffsetX = delegateItem.width / 2
                                                    delegateItem.pressOffsetY = delegateItem.height / 2
                                                }
                                                delegateItem.opacity = 0.5
                                                delegateItem.scale = 1.05
                                                // Map placeholder position from page-local to homeScreen coords
                                                var phPos = delegateItem.parent.mapToItem(homeScreen, delegateItem.originalX, delegateItem.originalY)
                                                dragPlaceholder.x = phPos.x
                                                dragPlaceholder.y = phPos.y
                                                dragPlaceholder.width = model.colSpan * homeScreen.cellSide
                                                dragPlaceholder.height = model.rowSpan * homeScreen.cellSide
                                                dragPlaceholder.visible = true
                                                selectionTimer.restart()
                                            }

                                            onPressAndHold: function(mouse) {
                                                innerContent.scale = 1.05
                                                liftShadow.visible = true
                                                if (homeScreen.selectedInstanceId === model.instanceId) {
                                                    // Already selected -- initiate drag
                                                    delegateItem.pressOffsetX = mouse.x
                                                    delegateItem.pressOffsetY = mouse.y
                                                    initiateDrag()
                                                } else {
                                                    // Select this widget
                                                    homeScreen.selectWidget(model.instanceId)
                                                }
                                            }

                                            onReleased: {
                                                if (!delegateItem.dragging) {
                                                    innerContent.scale = 1.0
                                                    liftShadow.visible = false
                                                }
                                            }

                                            onCanceled: {
                                                innerContent.scale = 1.0
                                                liftShadow.visible = false
                                            }

                                            onClicked: {
                                                // No-op: when selection is active, selectionTapInterceptor handles taps.
                                                // When no selection, click falls through to widget content (z:-1 means
                                                // widget MouseAreas get it first for interactive widgets).
                                            }
                                        }

                                        // Tap interceptor -- catches taps on ALL delegates during selection
                                        // When selected, tap deselects. When another widget is selected, tap also deselects.
                                        // Prevents widget actions (especially launcher widgets) from firing during selection.
                                        // For the SELECTED widget: press immediately lifts, any movement starts drag.
                                        MouseArea {
                                            id: selectionTapInterceptor
                                            anchors.fill: parent
                                            z: 15  // above widget content (z:0), below edge handles (z:20)
                                            enabled: homeScreen.selectedInstanceId !== ""
                                            visible: enabled
                                            property bool _movedDuringPress: false
                                            property bool _dragOwnedHere: false

                                            onPressed: function(mouse) {
                                                _movedDuringPress = false
                                                _dragOwnedHere = false
                                                if (delegateItem.isSelected) {
                                                    // Immediately lift the selected widget (ready to drag)
                                                    innerContent.scale = 1.05
                                                    liftShadow.visible = true
                                                    delegateItem.pressOffsetX = mouse.x
                                                    delegateItem.pressOffsetY = mouse.y
                                                }
                                            }
                                            onPositionChanged: function(mouse) {
                                                if (!delegateItem.isSelected) return
                                                if (!delegateItem.dragging) {
                                                    // Movement detected — start drag immediately (no long-press wait)
                                                    _movedDuringPress = true
                                                    _dragOwnedHere = true
                                                    widgetMouseArea.initiateDrag(true)
                                                }
                                                // Keep drag updates in this MouseArea (touch grab stays here)
                                                if (_dragOwnedHere)
                                                    delegateItem.updateDragFrom(selectionTapInterceptor, mouse.x, mouse.y)
                                            }
                                            // Tap (press+release without movement) deselects
                                            onClicked: {
                                                if (!_movedDuringPress)
                                                    homeScreen.deselectWidget()
                                            }
                                            // Also handle long-press on NON-selected widgets: deselect
                                            onPressAndHold: function(mouse) {
                                                if (!delegateItem.isSelected)
                                                    homeScreen.deselectWidget()
                                            }
                                            onReleased: {
                                                // If we owned the drag, finalize it
                                                if (_dragOwnedHere && delegateItem.dragging) {
                                                    _dragOwnedHere = false
                                                    delegateItem.finalizeDrop()
                                                    return
                                                }
                                                // Reset lift if press didn't start a drag
                                                if (!delegateItem.dragging) {
                                                    innerContent.scale = 1.0
                                                    liftShadow.visible = false
                                                }
                                            }
                                        }

                                        // Timer to rebind position after snap-back animation
                                        Timer {
                                            id: snapBackTimer
                                            interval: 250
                                            onTriggered: {
                                                delegateItem.x = Qt.binding(function() { return homeScreen.offsetX + model.column * homeScreen.cellSide })
                                                delegateItem.y = Qt.binding(function() { return homeScreen.offsetY + model.row * homeScreen.cellSide })
                                            }
                                        }

                                        // --- Selection overlays ---

                                        // Accent border (selected widget only)
                                        Rectangle {
                                            anchors.fill: parent
                                            color: "transparent"
                                            border.width: 2
                                            border.color: ThemeService.primary
                                            radius: UiMetrics.radius
                                            visible: delegateItem.isSelected
                                            z: 10
                                        }

                                        // Badge buttons removed (CLN-03): X delete, gear config, corner resize
                                        // handle are now replaced by navbar gear/trash controls (Phase 26).

                                        // --- 4-edge resize handles ---

                                        // Top edge handle
                                        Rectangle {
                                            id: topEdgeHandle
                                            width: parent.width * 0.4
                                            height: 4
                                            radius: 2
                                            color: ThemeService.primary
                                            opacity: 0.7
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            anchors.top: parent.top
                                            anchors.topMargin: -2
                                            visible: delegateItem.isSelected
                                            z: 20

                                            MouseArea {
                                                anchors.fill: parent
                                                anchors.margins: -UiMetrics.spacing
                                                preventStealing: true
                                                cursorShape: Qt.SizeVerCursor

                                                property real startY: 0
                                                property int startRow: 0
                                                property int startRowSpan: 0
                                                property bool resizing: false

                                                onPressed: function(mouse) {
                                                    resizing = true
                                                    var mapped = mapToItem(delegateItem.parent, mouse.x, mouse.y)
                                                    startY = mapped.y
                                                    startRow = model.row
                                                    startRowSpan = model.rowSpan
                                                    homeScreen.startEdgeResize(model.instanceId, model.column, model.row,
                                                        model.colSpan, model.rowSpan, delegateItem.parent)
                                                    selectionTimer.restart()
                                                    mouse.accepted = true
                                                }

                                                onPositionChanged: function(mouse) {
                                                    if (!resizing) return
                                                    var current = mapToItem(delegateItem.parent, mouse.x, mouse.y)
                                                    var deltaRows = Math.round((current.y - startY) / homeScreen.cellSide)

                                                    // Top edge: move row (decrease = grow up, increase = shrink down)
                                                    var newRow = startRow + deltaRows
                                                    var newRowSpan = startRowSpan - deltaRows
                                                    var hitDescriptorLimit = false

                                                    // Clamp to min/max constraints, keeping BOTTOM edge anchored
                                                    var bottomEdge = startRow + startRowSpan
                                                    if (newRowSpan < model.minRows) {
                                                        newRowSpan = model.minRows
                                                        newRow = bottomEdge - newRowSpan
                                                        hitDescriptorLimit = true
                                                    }
                                                    if (newRowSpan > model.maxRows) {
                                                        newRowSpan = model.maxRows
                                                        newRow = bottomEdge - newRowSpan
                                                        hitDescriptorLimit = true
                                                    }
                                                    if (newRow < 0) {
                                                        newRow = 0
                                                        newRowSpan = bottomEdge - newRow
                                                    }

                                                    homeScreen.updateEdgeResize(model.column, newRow, model.colSpan, newRowSpan,
                                                        model.instanceId, delegateItem.parent, hitDescriptorLimit)
                                                    selectionTimer.restart()
                                                }

                                                onReleased: {
                                                    if (!resizing) return
                                                    resizing = false
                                                    homeScreen.commitEdgeResize(model.instanceId)
                                                }

                                                onCanceled: {
                                                    resizing = false
                                                    homeScreen.cancelEdgeResize()
                                                }
                                            }
                                        }

                                        // Bottom edge handle
                                        Rectangle {
                                            id: bottomEdgeHandle
                                            width: parent.width * 0.4
                                            height: 4
                                            radius: 2
                                            color: ThemeService.primary
                                            opacity: 0.7
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            anchors.bottom: parent.bottom
                                            anchors.bottomMargin: -2
                                            visible: delegateItem.isSelected
                                            z: 20

                                            MouseArea {
                                                anchors.fill: parent
                                                anchors.margins: -UiMetrics.spacing
                                                preventStealing: true
                                                cursorShape: Qt.SizeVerCursor

                                                property real startY: 0
                                                property int startRowSpan: 0
                                                property bool resizing: false

                                                onPressed: function(mouse) {
                                                    resizing = true
                                                    var mapped = mapToItem(delegateItem.parent, mouse.x, mouse.y)
                                                    startY = mapped.y
                                                    startRowSpan = model.rowSpan
                                                    homeScreen.startEdgeResize(model.instanceId, model.column, model.row,
                                                        model.colSpan, model.rowSpan, delegateItem.parent)
                                                    selectionTimer.restart()
                                                    mouse.accepted = true
                                                }

                                                onPositionChanged: function(mouse) {
                                                    if (!resizing) return
                                                    var current = mapToItem(delegateItem.parent, mouse.x, mouse.y)
                                                    var deltaRows = Math.round((current.y - startY) / homeScreen.cellSide)

                                                    // Bottom edge: position stays, only span changes
                                                    var newRowSpan = startRowSpan + deltaRows
                                                    var hitDescriptorLimit = false
                                                    if (newRowSpan < model.minRows) { newRowSpan = model.minRows; hitDescriptorLimit = true }
                                                    if (newRowSpan > model.maxRows) { newRowSpan = model.maxRows; hitDescriptorLimit = true }

                                                    homeScreen.updateEdgeResize(model.column, model.row, model.colSpan, newRowSpan,
                                                        model.instanceId, delegateItem.parent, hitDescriptorLimit)
                                                    selectionTimer.restart()
                                                }

                                                onReleased: {
                                                    if (!resizing) return
                                                    resizing = false
                                                    homeScreen.commitEdgeResize(model.instanceId)
                                                }

                                                onCanceled: {
                                                    resizing = false
                                                    homeScreen.cancelEdgeResize()
                                                }
                                            }
                                        }

                                        // Left edge handle
                                        Rectangle {
                                            id: leftEdgeHandle
                                            width: 4
                                            height: parent.height * 0.4
                                            radius: 2
                                            color: ThemeService.primary
                                            opacity: 0.7
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.left: parent.left
                                            anchors.leftMargin: -2
                                            visible: delegateItem.isSelected
                                            z: 20

                                            MouseArea {
                                                anchors.fill: parent
                                                anchors.margins: -UiMetrics.spacing
                                                preventStealing: true
                                                cursorShape: Qt.SizeHorCursor

                                                property real startX: 0
                                                property int startCol: 0
                                                property int startColSpan: 0
                                                property bool resizing: false

                                                onPressed: function(mouse) {
                                                    resizing = true
                                                    var mapped = mapToItem(delegateItem.parent, mouse.x, mouse.y)
                                                    startX = mapped.x
                                                    startCol = model.column
                                                    startColSpan = model.colSpan
                                                    homeScreen.startEdgeResize(model.instanceId, model.column, model.row,
                                                        model.colSpan, model.rowSpan, delegateItem.parent)
                                                    selectionTimer.restart()
                                                    mouse.accepted = true
                                                }

                                                onPositionChanged: function(mouse) {
                                                    if (!resizing) return
                                                    var current = mapToItem(delegateItem.parent, mouse.x, mouse.y)
                                                    var deltaCols = Math.round((current.x - startX) / homeScreen.cellSide)

                                                    // Left edge: move col (decrease = grow left, increase = shrink right)
                                                    var newCol = startCol + deltaCols
                                                    var newColSpan = startColSpan - deltaCols
                                                    var hitDescriptorLimit = false

                                                    // Clamp to min/max constraints, keeping RIGHT edge anchored
                                                    var rightEdge = startCol + startColSpan
                                                    if (newColSpan < model.minCols) {
                                                        newColSpan = model.minCols
                                                        newCol = rightEdge - newColSpan
                                                        hitDescriptorLimit = true
                                                    }
                                                    if (newColSpan > model.maxCols) {
                                                        newColSpan = model.maxCols
                                                        newCol = rightEdge - newColSpan
                                                        hitDescriptorLimit = true
                                                    }
                                                    if (newCol < 0) {
                                                        newCol = 0
                                                        newColSpan = rightEdge - newCol
                                                    }

                                                    homeScreen.updateEdgeResize(newCol, model.row, newColSpan, model.rowSpan,
                                                        model.instanceId, delegateItem.parent, hitDescriptorLimit)
                                                    selectionTimer.restart()
                                                }

                                                onReleased: {
                                                    if (!resizing) return
                                                    resizing = false
                                                    homeScreen.commitEdgeResize(model.instanceId)
                                                }

                                                onCanceled: {
                                                    resizing = false
                                                    homeScreen.cancelEdgeResize()
                                                }
                                            }
                                        }

                                        // Right edge handle
                                        Rectangle {
                                            id: rightEdgeHandle
                                            width: 4
                                            height: parent.height * 0.4
                                            radius: 2
                                            color: ThemeService.primary
                                            opacity: 0.7
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.right: parent.right
                                            anchors.rightMargin: -2
                                            visible: delegateItem.isSelected
                                            z: 20

                                            MouseArea {
                                                anchors.fill: parent
                                                anchors.margins: -UiMetrics.spacing
                                                preventStealing: true
                                                cursorShape: Qt.SizeHorCursor

                                                property real startX: 0
                                                property int startColSpan: 0
                                                property bool resizing: false

                                                onPressed: function(mouse) {
                                                    resizing = true
                                                    var mapped = mapToItem(delegateItem.parent, mouse.x, mouse.y)
                                                    startX = mapped.x
                                                    startColSpan = model.colSpan
                                                    homeScreen.startEdgeResize(model.instanceId, model.column, model.row,
                                                        model.colSpan, model.rowSpan, delegateItem.parent)
                                                    selectionTimer.restart()
                                                    mouse.accepted = true
                                                }

                                                onPositionChanged: function(mouse) {
                                                    if (!resizing) return
                                                    var current = mapToItem(delegateItem.parent, mouse.x, mouse.y)
                                                    var deltaCols = Math.round((current.x - startX) / homeScreen.cellSide)

                                                    // Right edge: position stays, only span changes
                                                    var newColSpan = startColSpan + deltaCols
                                                    var hitDescriptorLimit = false
                                                    if (newColSpan < model.minCols) { newColSpan = model.minCols; hitDescriptorLimit = true }
                                                    if (newColSpan > model.maxCols) { newColSpan = model.maxCols; hitDescriptorLimit = true }

                                                    homeScreen.updateEdgeResize(model.column, model.row, newColSpan, model.rowSpan,
                                                        model.instanceId, delegateItem.parent, hitDescriptorLimit)
                                                    selectionTimer.restart()
                                                }

                                                onReleased: {
                                                    if (!resizing) return
                                                    resizing = false
                                                    homeScreen.commitEdgeResize(model.instanceId)
                                                }

                                                onCanceled: {
                                                    resizing = false
                                                    homeScreen.cancelEdgeResize()
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
        }



    }

    // ---- Overlays that must stay OUTSIDE SwipeView (at homeScreen level) ----
    // These need full-screen coverage, not clipped to a single page.

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

    // Drag overlay -- captures touch movement when a widget is being dragged
    MouseArea {
        id: dragOverlay
        anchors.fill: parent
        z: 200
        enabled: homeScreen.draggingInstanceId !== ""
        visible: enabled

        onPositionChanged: function(mouse) {
            var d = homeScreen.draggingDelegate
            if (!d) return
            d.updateDragFrom(dragOverlay, mouse.x, mouse.y)
        }

        onReleased: function(mouse) {
            var d = homeScreen.draggingDelegate
            if (!d) return
            d.finalizeDrop()
        }
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

    // ---- Long-press empty space popup menu (PGM-01) ----
    Popup {
        id: emptySpaceMenu
        modal: true
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        parent: Overlay.overlay
        width: Math.min(parent ? parent.width * 0.45 : 200, 240)
        padding: UiMetrics.spacing

        background: Rectangle {
            color: ThemeService.surfaceContainerHighest
            radius: UiMetrics.radius
        }

        contentItem: Column {
            id: emptySpaceMenuContent
            width: emptySpaceMenu.availableWidth
            spacing: 0

            // "Add Widget" option
            Item {
                width: emptySpaceMenuContent.width
                height: UiMetrics.touchMin

                Rectangle {
                    anchors.fill: parent
                    radius: UiMetrics.radiusSmall
                    color: addWidgetMA.pressed ? ThemeService.primaryContainer : "transparent"
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: UiMetrics.marginPage
                    anchors.rightMargin: UiMetrics.marginPage
                    spacing: UiMetrics.gap

                    MaterialIcon {
                        icon: "\ue1bd"  // widgets
                        size: UiMetrics.iconSize
                        color: ThemeService.onSurface
                    }
                    NormalText {
                        text: "Add Widget"
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.onSurface
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        clip: true
                    }
                }

                MouseArea {
                    id: addWidgetMA
                    anchors.fill: parent
                    onClicked: {
                        emptySpaceMenu.close()
                        widgetPickerSheet.gridCols = homeScreen.gridCols
                        widgetPickerSheet.gridRows = homeScreen.gridRows
                        widgetPickerSheet.openPicker()
                    }
                }
            }

            // Divider
            Rectangle {
                width: emptySpaceMenuContent.width
                height: 1
                color: ThemeService.outlineVariant
                anchors.horizontalCenter: parent.horizontalCenter
            }

            // "Add Page" option
            Item {
                width: emptySpaceMenuContent.width
                height: UiMetrics.touchMin

                Rectangle {
                    anchors.fill: parent
                    radius: UiMetrics.radiusSmall
                    color: addPageMA.pressed ? ThemeService.primaryContainer : "transparent"
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: UiMetrics.marginPage
                    anchors.rightMargin: UiMetrics.marginPage
                    spacing: UiMetrics.gap

                    MaterialIcon {
                        icon: "\ue85d"  // library_add
                        size: UiMetrics.iconSize
                        color: ThemeService.onSurface
                    }
                    NormalText {
                        text: "Add Page"
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.onSurface
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        maximumLineCount: 1
                        clip: true
                    }
                }

                MouseArea {
                    id: addPageMA
                    anchors.fill: parent
                    onClicked: {
                        emptySpaceMenu.close()
                        WidgetGridModel.addPage()
                        pageView.setCurrentIndex(WidgetGridModel.pageCount - 2)
                    }
                }
            }
        }
    }

    // ---- Widget picker sheet (Phase 27) ----
    WidgetPickerSheet {
        id: widgetPickerSheet
    }

    Connections {
        target: widgetPickerSheet
        function onWidgetChosen(widgetId, defaultCols, defaultRows) {
            var defCols = defaultCols > 0 ? defaultCols : 2
            var defRows = defaultRows > 0 ? defaultRows : 2
            defCols = Math.min(defCols, homeScreen.gridCols)
            defRows = Math.min(defRows, homeScreen.gridRows)

            var cell = WidgetGridModel.findFirstAvailableCell(defCols, defRows)
            if (cell.col >= 0) {
                WidgetGridModel.placeWidget(widgetId, cell.col, cell.row, defCols, defRows)
                widgetPickerSheet.closePicker()
            } else {
                toast.show("No space available \u2014 remove a widget first")
            }
        }
    }

    // ---- Widget config sheet ----
    WidgetConfigSheet {
        id: configSheet
    }

    // ---- Toast (transient feedback) ----
    Toast {
        id: toast
        z: 200
    }
}
