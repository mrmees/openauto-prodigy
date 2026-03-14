import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: homeScreen
    anchors.fill: parent

    Component.onCompleted: ApplicationController.setTitle("Home")

    // Edit mode state
    property bool editMode: false

    // Track active drag/resize for cleanup on exit
    property string draggingInstanceId: ""
    property string resizingInstanceId: ""
    property var draggingDelegate: null
    property int _dragColSpan: 1
    property int _dragRowSpan: 1

    // Grid frame computation from DisplayInfo.cellSide (cells are square)
    readonly property real cellSide: DisplayInfo ? DisplayInfo.cellSide : 120
    readonly property int gridCols: pageView.width > 0 ? Math.max(3, Math.floor(pageView.width / cellSide)) : 3
    readonly property int gridRows: pageView.height > 0 ? Math.max(2, Math.floor(pageView.height / cellSide)) : 2
    readonly property real gridW: gridCols * cellSide
    readonly property real gridH: gridRows * cellSide
    readonly property real offsetX: (pageView.width - gridW) / 2
    readonly property real offsetY: (pageView.height - gridH) / 2

    // Legacy aliases for backward compat in overlays
    readonly property real cellWidth: cellSide
    readonly property real cellHeight: cellSide

    // Grid is invisible until first valid layout
    property bool gridReady: false

    // Push grid dimensions to C++ model when they change
    onGridColsChanged: {
        if (gridCols > 0 && gridRows > 0 && pageView.width > 0 && pageView.height > 0) {
            WidgetGridModel.setGridDimensions(gridCols, gridRows)
            gridReady = true
        }
    }
    onGridRowsChanged: {
        if (gridCols > 0 && gridRows > 0 && pageView.width > 0 && pageView.height > 0) {
            WidgetGridModel.setGridDimensions(gridCols, gridRows)
            gridReady = true
        }
    }

    function exitEditMode() {
        editMode = false
        inactivityTimer.stop()
        pickerOverlay.visible = false
        // Reset drag state
        dropHighlight.visible = false
        dragPlaceholder.visible = false
        resizeGhost.visible = false
        draggingInstanceId = ""
        draggingDelegate = null
        resizingInstanceId = ""

        // Auto-clean empty pages (except page 0) after SwipeView settles
        Qt.callLater(function() {
            for (var i = WidgetGridModel.pageCount - 1; i > 0; i--) {
                if (WidgetGridModel.widgetCountOnPage(i) === 0) {
                    WidgetGridModel.removePage(i)
                }
            }
            if (pageView.currentIndex >= WidgetGridModel.pageCount)
                pageView.setCurrentIndex(WidgetGridModel.pageCount - 1)
        })
    }

    // Inactivity timer -- exits edit mode after 10s of no interaction
    Timer {
        id: inactivityTimer
        interval: 10000
        onTriggered: homeScreen.exitEditMode()
    }

    onEditModeChanged: {
        WidgetGridModel.setEditMode(editMode)
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

        // Multi-page SwipeView -- fills available space above dots and dock
        SwipeView {
            id: pageView
            Layout.fillWidth: true
            Layout.fillHeight: true
            interactive: !homeScreen.editMode
            clip: true

            Component.onCompleted: {
                contentItem.boundsBehavior = Flickable.DragAndOvershootBounds
                contentItem.highlightMoveDuration = 350
            }

            onCurrentIndexChanged: {
                WidgetGridModel.activePage = currentIndex
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
                                    model: homeScreen.gridCols + 1
                                    delegate: Column {
                                        x: homeScreen.offsetX + index * homeScreen.cellSide
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

                                // Horizontal lines
                                Repeater {
                                    model: homeScreen.gridRows + 1
                                    delegate: Row {
                                        x: homeScreen.offsetX
                                        y: homeScreen.offsetY + index * homeScreen.cellSide
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
                                    z: dragging ? 100 : 1

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

                                    // Force-reset drag state when edit mode exits (EVIOCGRAB safety)
                                    Connections {
                                        target: homeScreen
                                        function onEditModeChanged() {
                                            if (!homeScreen.editMode && delegateItem.dragging) {
                                                delegateItem.dragging = false
                                                delegateItem.opacity = 1.0
                                                delegateItem.scale = 1.0
                                                delegateItem.x = Qt.binding(function() { return homeScreen.offsetX + model.column * homeScreen.cellSide })
                                                delegateItem.y = Qt.binding(function() { return homeScreen.offsetY + model.row * homeScreen.cellSide })
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

                                            function requestContextMenu() {
                                                if (homeScreen.editMode) {
                                                    widgetMouseArea.initiateDrag()
                                                } else {
                                                    homeScreen.editMode = true
                                                }
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
                                                homeScreen.draggingDelegate = delegateItem
                                                homeScreen._dragColSpan = model.colSpan
                                                homeScreen._dragRowSpan = model.rowSpan
                                                delegateItem.originalX = delegateItem.x
                                                delegateItem.originalY = delegateItem.y
                                                delegateItem.pressOffsetX = delegateItem.width / 2
                                                delegateItem.pressOffsetY = delegateItem.height / 2
                                                delegateItem.opacity = 0.5
                                                delegateItem.scale = 1.05
                                                // Map placeholder position from page-local to homeScreen coords
                                                var phPos = delegateItem.parent.mapToItem(homeScreen, delegateItem.originalX, delegateItem.originalY)
                                                dragPlaceholder.x = phPos.x
                                                dragPlaceholder.y = phPos.y
                                                dragPlaceholder.width = model.colSpan * homeScreen.cellSide
                                                dragPlaceholder.height = model.rowSpan * homeScreen.cellSide
                                                dragPlaceholder.visible = true
                                                inactivityTimer.restart()
                                            }

                                            onPressAndHold: function(mouse) {
                                                if (!homeScreen.editMode) {
                                                    homeScreen.editMode = true
                                                    return
                                                }
                                                delegateItem.pressOffsetX = mouse.x
                                                delegateItem.pressOffsetY = mouse.y
                                                initiateDrag()
                                                delegateItem.pressOffsetX = mouse.x
                                                delegateItem.pressOffsetY = mouse.y
                                            }

                                            onClicked: {
                                                if (homeScreen.editMode) {
                                                    homeScreen.exitEditMode()
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
                                            visible: homeScreen.editMode && !model.isSingleton
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
                                                    startGlobal = mapToItem(pageGridContent, mouse.x, mouse.y)
                                                    startColSpan = model.colSpan
                                                    startRowSpan = model.rowSpan
                                                    proposedColSpan = startColSpan
                                                    proposedRowSpan = startRowSpan
                                                    lastClampedCols = startColSpan
                                                    lastClampedRows = startRowSpan

                                                    // Map ghost position from page-local to homeScreen coords
                                                    var ghostPos = pageGridContent.mapToItem(homeScreen,
                                                        homeScreen.offsetX + model.column * homeScreen.cellSide, homeScreen.offsetY + model.row * homeScreen.cellSide)
                                                    resizeGhost.x = ghostPos.x
                                                    resizeGhost.y = ghostPos.y
                                                    resizeGhost.width = model.colSpan * homeScreen.cellSide
                                                    resizeGhost.height = model.rowSpan * homeScreen.cellSide
                                                    resizeGhost.visible = true
                                                    resizeGhost.isValid = true

                                                    inactivityTimer.restart()
                                                    mouse.accepted = true
                                                }

                                                onPositionChanged: function(mouse) {
                                                    if (!resizing) return
                                                    var current = mapToItem(pageGridContent, mouse.x, mouse.y)
                                                    var deltaCols = Math.round((current.x - startGlobal.x) / homeScreen.cellSide)
                                                    var deltaRows = Math.round((current.y - startGlobal.y) / homeScreen.cellSide)

                                                    var newColSpan = startColSpan + deltaCols
                                                    var newRowSpan = startRowSpan + deltaRows

                                                    var wasAtLimit = false
                                                    if (newColSpan < model.minCols) { newColSpan = model.minCols; wasAtLimit = true }
                                                    if (newColSpan > model.maxCols) { newColSpan = model.maxCols; wasAtLimit = true }
                                                    if (newRowSpan < model.minRows) { newRowSpan = model.minRows; wasAtLimit = true }
                                                    if (newRowSpan > model.maxRows) { newRowSpan = model.maxRows; wasAtLimit = true }

                                                    if (model.column + newColSpan > homeScreen.gridCols) {
                                                        newColSpan = homeScreen.gridCols - model.column
                                                        wasAtLimit = true
                                                    }
                                                    if (model.row + newRowSpan > homeScreen.gridRows) {
                                                        newRowSpan = homeScreen.gridRows - model.row
                                                        wasAtLimit = true
                                                    }

                                                    newColSpan = Math.max(1, newColSpan)
                                                    newRowSpan = Math.max(1, newRowSpan)

                                                    proposedColSpan = newColSpan
                                                    proposedRowSpan = newRowSpan

                                                    var valid = WidgetGridModel.canPlace(model.column, model.row,
                                                        newColSpan, newRowSpan, model.instanceId)
                                                    resizeGhost.isValid = valid

                                                    if (wasAtLimit && (newColSpan !== lastClampedCols || newRowSpan !== lastClampedRows)) {
                                                        limitFlash.restart()
                                                    }
                                                    lastClampedCols = newColSpan
                                                    lastClampedRows = newRowSpan

                                                    resizeGhost.width = newColSpan * homeScreen.cellSide
                                                    resizeGhost.height = newRowSpan * homeScreen.cellSide

                                                    inactivityTimer.restart()
                                                }

                                                onReleased: {
                                                    resizing = false
                                                    homeScreen.resizingInstanceId = ""
                                                    resizeGhost.visible = false

                                                    if (proposedColSpan !== startColSpan || proposedRowSpan !== startRowSpan) {
                                                        WidgetGridModel.resizeWidget(model.instanceId, proposedColSpan, proposedRowSpan)
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

        // Page indicator dots (PAGE-02)
        PageIndicator {
            id: pageIndicator
            Layout.alignment: Qt.AlignHCenter
            count: pageView.count
            currentIndex: pageView.currentIndex
            interactive: !homeScreen.editMode  // Disabled during edit mode (edit is page-scoped)

            delegate: Rectangle {
                implicitWidth: UiMetrics.spacing * 1.2
                implicitHeight: implicitWidth
                radius: width / 2
                color: index === pageIndicator.currentIndex
                       ? ThemeService.primary
                       : ThemeService.onSurfaceVariant
                opacity: index === pageIndicator.currentIndex ? 1.0 : 0.4

                required property int index
                Behavior on opacity { OpacityAnimator { duration: 150 } }
            }

            onCurrentIndexChanged: {
                if (currentIndex !== pageView.currentIndex)
                    pageView.setCurrentIndex(currentIndex)
            }
        }

    }

    // Launcher dock -- overlays grid at z=10 (GL-02: grid uses full height, dock on top)
    LauncherDock {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: UiMetrics.marginPage
        anchors.rightMargin: UiMetrics.marginPage
        anchors.bottomMargin: UiMetrics.marginPage
        z: 10
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
            // Convert mouse position to page-local coordinates
            var pageLocal = mapToItem(d.parent, mouse.x, mouse.y)
            d.x = pageLocal.x - d.pressOffsetX
            d.y = pageLocal.y - d.pressOffsetY
            var cs = homeScreen._dragColSpan
            var rs = homeScreen._dragRowSpan
            var targetCol = Math.round((d.x - homeScreen.offsetX) / homeScreen.cellSide)
            var targetRow = Math.round((d.y - homeScreen.offsetY) / homeScreen.cellSide)
            targetCol = Math.max(0, Math.min(targetCol, homeScreen.gridCols - cs))
            targetRow = Math.max(0, Math.min(targetRow, homeScreen.gridRows - rs))
            var valid = WidgetGridModel.canPlace(targetCol, targetRow, cs, rs, homeScreen.draggingInstanceId)

            // Position drop highlight -- map from page-local to homeScreen coords
            var pageItem = d.parent
            var hlPos = pageItem.mapToItem(homeScreen,
                homeScreen.offsetX + targetCol * homeScreen.cellSide,
                homeScreen.offsetY + targetRow * homeScreen.cellSide)
            dropHighlight.x = hlPos.x
            dropHighlight.y = hlPos.y
            dropHighlight.width = cs * homeScreen.cellSide
            dropHighlight.height = rs * homeScreen.cellSide
            dropHighlight.isValid = valid
            dropHighlight.visible = true

            inactivityTimer.restart()
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

    // ---- FABs (floating action buttons) ----

    // Add-page FAB (edit mode, above add-widget FAB)
    Rectangle {
        id: addPageFab
        width: UiMetrics.touchMin * 1.2
        height: width
        radius: width / 2
        color: ThemeService.primary
        visible: homeScreen.editMode
        z: 50
        anchors.right: parent.right
        anchors.bottom: fab.top
        anchors.rightMargin: UiMetrics.marginPage * 2
        anchors.bottomMargin: UiMetrics.spacing

        MaterialIcon {
            icon: "\ue85d"  // library_add
            size: UiMetrics.iconSize
            color: ThemeService.onPrimary
            anchors.centerIn: parent
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                inactivityTimer.restart()
                WidgetGridModel.addPage()
                // Navigate to the newly inserted page (second-to-last, since reserved page shifted to last)
                pageView.setCurrentIndex(WidgetGridModel.pageCount - 2)
            }
        }
    }

    // Add-widget FAB
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
                var cell = WidgetGridModel.findFirstAvailableCell(1, 1)
                if (cell.col < 0) {
                    toast.show("No space available \u2014 remove a widget first")
                } else {
                    WidgetPickerModel.filterByAvailableSpace(
                        homeScreen.gridCols,
                        homeScreen.gridRows)
                    pickerOverlay.visible = true
                }
            }
        }
    }

    // Delete-page FAB (edit mode, left side, only when pageCount > 1 and not on page 0)
    Rectangle {
        id: deletePageFab
        width: UiMetrics.touchMin * 1.2
        height: width
        radius: width / 2
        color: ThemeService.error
        visible: homeScreen.editMode && WidgetGridModel.pageCount > 1 && pageView.currentIndex > 0 && !WidgetGridModel.isReservedPage(pageView.currentIndex)
        z: 50
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: UiMetrics.marginPage * 2
        anchors.bottomMargin: UiMetrics.marginPage * 2 + UiMetrics.touchMin + UiMetrics.spacing

        MaterialIcon {
            icon: "\ue872"  // delete
            size: UiMetrics.iconSize
            color: ThemeService.onError
            anchors.centerIn: parent
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                inactivityTimer.restart()
                deletePageDialog.targetPage = pageView.currentIndex
                deletePageDialog.widgetCount = WidgetGridModel.widgetCountOnPage(pageView.currentIndex)
                deletePageDialog.visible = true
            }
        }
    }

    // ---- Delete page confirmation dialog ----
    Item {
        id: deletePageDialog
        anchors.fill: parent
        visible: false
        z: 300

        property int targetPage: -1
        property int widgetCount: 0

        // Scrim
        Rectangle {
            anchors.fill: parent
            color: ThemeService.scrim
            opacity: 0.4
            MouseArea {
                anchors.fill: parent
                onClicked: deletePageDialog.visible = false
            }
        }

        // Dialog card
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.6, 400)
            height: dialogContent.implicitHeight + UiMetrics.spacing * 3
            radius: UiMetrics.radiusLarge
            color: ThemeService.surfaceContainerHighest

            ColumnLayout {
                id: dialogContent
                anchors.fill: parent
                anchors.margins: UiMetrics.spacing * 1.5
                spacing: UiMetrics.spacing

                NormalText {
                    text: "Delete page and " + deletePageDialog.widgetCount + " widget"
                          + (deletePageDialog.widgetCount !== 1 ? "s" : "") + "?"
                    font.pixelSize: UiMetrics.fontTitle
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: UiMetrics.spacing

                    // Cancel button
                    Rectangle {
                        width: cancelText.implicitWidth + UiMetrics.spacing * 2
                        height: UiMetrics.touchMin * 0.8
                        radius: UiMetrics.radiusSmall
                        color: cancelMA.pressed ? ThemeService.surfaceContainer : "transparent"

                        NormalText {
                            id: cancelText
                            text: "Cancel"
                            font.pixelSize: UiMetrics.fontSmall
                            color: ThemeService.onSurface
                            anchors.centerIn: parent
                        }

                        MouseArea {
                            id: cancelMA
                            anchors.fill: parent
                            onClicked: deletePageDialog.visible = false
                        }
                    }

                    // Delete button
                    Rectangle {
                        width: deleteText.implicitWidth + UiMetrics.spacing * 2
                        height: UiMetrics.touchMin * 0.8
                        radius: UiMetrics.radiusSmall
                        color: deleteMA.pressed ? Qt.darker(ThemeService.error, 1.2) : ThemeService.error

                        NormalText {
                            id: deleteText
                            text: "Delete"
                            font.pixelSize: UiMetrics.fontSmall
                            color: ThemeService.onError
                            anchors.centerIn: parent
                        }

                        MouseArea {
                            id: deleteMA
                            anchors.fill: parent
                            onClicked: {
                                var savedIndex = deletePageDialog.targetPage
                                WidgetGridModel.removePage(deletePageDialog.targetPage)
                                var newIndex = Math.min(savedIndex, WidgetGridModel.pageCount - 1)
                                pageView.setCurrentIndex(newIndex)
                                deletePageDialog.visible = false
                                inactivityTimer.restart()
                            }
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
                                    var defCols = model.defaultCols > 0 ? model.defaultCols : 2
                                    var defRows = model.defaultRows > 0 ? model.defaultRows : 2
                                    defCols = Math.min(defCols, homeScreen.gridCols)
                                    defRows = Math.min(defRows, homeScreen.gridRows)

                                    var cell = WidgetGridModel.findFirstAvailableCell(defCols, defRows)
                                    if (cell.col < 0)
                                        cell = WidgetGridModel.findFirstAvailableCell(1, 1)
                                    if (cell.col >= 0) {
                                        var placeCols = Math.min(defCols, homeScreen.gridCols - cell.col)
                                        var placeRows = Math.min(defRows, homeScreen.gridRows - cell.row)
                                        WidgetGridModel.placeWidget(model.widgetId,
                                            cell.col, cell.row, placeCols, placeRows)
                                    } else {
                                        toast.show("No space available \u2014 remove a widget first")
                                    }
                                    pickerOverlay.visible = false
                                    inactivityTimer.restart()
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
