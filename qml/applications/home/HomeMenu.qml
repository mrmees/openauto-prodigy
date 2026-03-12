import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: homeScreen
    anchors.fill: parent

    Component.onCompleted: ApplicationController.setTitle("Home")

    // Track which grid cell was long-pressed (for widget placement)
    property int _targetCol: 0
    property int _targetRow: 0
    property string _targetInstanceId: ""  // non-empty = editing existing widget

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

            // Background long-press detector for empty grid space
            MouseArea {
                anchors.fill: parent
                pressAndHoldInterval: 500
                onPressAndHold: function(mouse) {
                    // Calculate which cell was pressed
                    var col = Math.floor(mouse.x / gridContainer.cellWidth)
                    var row = Math.floor(mouse.y / gridContainer.cellHeight)
                    col = Math.max(0, Math.min(col, WidgetGridModel.gridColumns - 1))
                    row = Math.max(0, Math.min(row, WidgetGridModel.gridRows - 1))

                    // Only open picker on empty cells
                    if (WidgetGridModel.canPlace(col, row, 1, 1)) {
                        homeScreen._targetCol = col
                        homeScreen._targetRow = row
                        homeScreen._targetInstanceId = ""
                        WidgetPickerModel.filterByAvailableSpace(
                            WidgetGridModel.gridColumns - col,
                            WidgetGridModel.gridRows - row)
                        pickerOverlay.visible = true
                    }
                }
            }

            Repeater {
                model: WidgetGridModel

                delegate: Item {
                    x: model.column * gridContainer.cellWidth
                    y: model.row * gridContainer.cellHeight
                    width: model.colSpan * gridContainer.cellWidth
                    height: model.rowSpan * gridContainer.cellHeight
                    visible: model.visible

                    // Inner content with gutter margins
                    Item {
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
                                homeScreen._targetCol = model.column
                                homeScreen._targetRow = model.row
                                homeScreen._targetInstanceId = model.instanceId
                                contextMenuOverlay.anchorX = parent.mapToItem(homeScreen, parent.width / 2, parent.height / 2).x
                                contextMenuOverlay.anchorY = parent.mapToItem(homeScreen, parent.width / 2, parent.height / 2).y
                                contextMenuOverlay.visible = true
                            }

                            onStatusChanged: {
                                if (status === Loader.Error)
                                    console.warn("HomeMenu: failed to load widget", model.qmlComponent)
                            }
                        }

                        // Long-press detector behind widget content
                        MouseArea {
                            anchors.fill: parent
                            z: -1
                            pressAndHoldInterval: 500
                            onPressAndHold: {
                                homeScreen._targetCol = model.column
                                homeScreen._targetRow = model.row
                                homeScreen._targetInstanceId = model.instanceId
                                contextMenuOverlay.anchorX = mapToItem(homeScreen, mouse.x, mouse.y).x
                                contextMenuOverlay.anchorY = mapToItem(homeScreen, mouse.x, mouse.y).y
                                contextMenuOverlay.visible = true
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

        // Scrim
        Rectangle {
            anchors.fill: parent
            color: ThemeService.scrim
            opacity: 0.4
            MouseArea {
                anchors.fill: parent
                onClicked: pickerOverlay.visible = false
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
                                // Place with default size (2x2, clamped to available space)
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
