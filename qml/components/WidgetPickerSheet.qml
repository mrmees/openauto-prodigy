import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root

    signal widgetChosen(string widgetId, int defaultCols, int defaultRows)
    readonly property bool isOpen: pickerDialog.visible
    readonly property int touchTarget: Math.max(UiMetrics.touchMin,
                                                Math.round(64 * UiMetrics.scale))

    property int gridCols: 1
    property int gridRows: 1

    function openPicker() {
        WidgetPickerModel.filterByAvailableSpace(gridCols, gridRows, false)
        pickerDialog.open()
    }

    function closePicker() {
        pickerDialog.close()
    }

    Dialog {
        id: pickerDialog
        modal: true
        dim: false
        closePolicy: Popup.CloseOnEscape

        parent: Overlay.overlay
        x: 0
        y: 0
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0

        padding: 0
        topPadding: 0
        bottomPadding: 0

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
                    icon: "\ue1bd"
                    size: UiMetrics.iconSize
                    color: ThemeService.primary
                }

                Text {
                    text: "Add Widget"
                    font.pixelSize: UiMetrics.fontTitle
                    font.bold: true
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                }

                MouseArea {
                    Layout.preferredWidth: root.touchTarget
                    Layout.preferredHeight: root.touchTarget
                    onClicked: pickerDialog.close()

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
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: ThemeService.outlineVariant
            }
        }

        contentItem: ColumnLayout {
            spacing: 0

            ListView {
                id: categoryTabs
                Layout.fillWidth: true
                Layout.preferredHeight: root.touchTarget + UiMetrics.spacing * 2
                orientation: ListView.Horizontal
                spacing: UiMetrics.spacing
                leftMargin: UiMetrics.marginPage
                rightMargin: UiMetrics.marginPage
                topMargin: UiMetrics.spacing
                bottomMargin: UiMetrics.spacing
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: WidgetPickerModel.categories

                delegate: Rectangle {
                    required property var modelData

                    width: Math.max(Math.round(126 * UiMetrics.scale),
                                    categoryLabel.implicitWidth + UiMetrics.marginPage * 2)
                    height: categoryTabs.height - categoryTabs.topMargin - categoryTabs.bottomMargin
                    radius: UiMetrics.radius
                    color: WidgetPickerModel.categoryFilter === modelData.id
                        ? ThemeService.primary
                        : (categoryMouse.pressed
                           ? ThemeService.primaryContainer
                           : "transparent")

                    Text {
                        id: categoryLabel
                        anchors.centerIn: parent
                        text: modelData.label
                        font.pixelSize: UiMetrics.fontSmall
                        font.bold: true
                        color: WidgetPickerModel.categoryFilter === modelData.id
                            ? ThemeService.onPrimary
                            : ThemeService.onSurfaceVariant
                    }

                    MouseArea {
                        id: categoryMouse
                        anchors.fill: parent
                        onClicked: WidgetPickerModel.setCategoryFilter(modelData.id)
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: ThemeService.outlineVariant
                }
            }

            GridView {
                id: widgetGrid
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: UiMetrics.marginPage
                Layout.rightMargin: UiMetrics.marginPage
                Layout.topMargin: UiMetrics.spacing
                Layout.bottomMargin: UiMetrics.marginPage
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: WidgetPickerModel

                readonly property int columnCount: 3
                cellWidth: width / columnCount
                cellHeight: Math.round(190 * UiMetrics.scale)
                ScrollIndicator.vertical: ScrollIndicator { }

                delegate: Rectangle {
                    id: card

                    required property string widgetId
                    required property string displayName
                    required property string iconName
                    required property int defaultCols
                    required property int defaultRows
                    required property string defaultSizeLabel
                    required property string sizeRangeLabel

                    width: widgetGrid.cellWidth - UiMetrics.spacing
                    height: widgetGrid.cellHeight - UiMetrics.spacing
                    radius: UiMetrics.radiusLarge
                    color: cardMouse.pressed
                        ? ThemeService.primaryContainer
                        : ThemeService.surfaceContainer
                    border.width: 1
                    border.color: cardMouse.pressed
                        ? ThemeService.primary
                        : ThemeService.outlineVariant

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: UiMetrics.gap
                        spacing: UiMetrics.spacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: UiMetrics.gap

                            Rectangle {
                                Layout.preferredWidth: Math.round(52 * UiMetrics.scale)
                                Layout.preferredHeight: Math.round(52 * UiMetrics.scale)
                                radius: UiMetrics.radius
                                color: ThemeService.surfaceContainerHigh

                                MaterialIcon {
                                    anchors.centerIn: parent
                                    icon: card.iconName
                                    size: UiMetrics.iconSize
                                    color: ThemeService.primary
                                }
                            }

                            Text {
                                text: card.displayName
                                font.pixelSize: UiMetrics.fontTitle
                                font.bold: true
                                color: ThemeService.onSurface
                                wrapMode: Text.WordWrap
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                Layout.fillWidth: true
                            }
                        }

                        Item { Layout.fillHeight: true }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: UiMetrics.spacing

                            Text {
                                text: "SIZE"
                                font.pixelSize: UiMetrics.fontTiny
                                font.bold: true
                                color: ThemeService.onSurfaceVariant
                            }

                            Rectangle {
                                Layout.preferredWidth: defaultSizeText.implicitWidth + UiMetrics.spacing * 2
                                Layout.preferredHeight: Math.round(38 * UiMetrics.scale)
                                radius: height / 2
                                color: ThemeService.primary

                                Text {
                                    id: defaultSizeText
                                    anchors.centerIn: parent
                                    text: card.defaultSizeLabel
                                    font.pixelSize: UiMetrics.fontTiny
                                    font.bold: true
                                    color: ThemeService.onPrimary
                                }
                            }

                            Rectangle {
                                visible: card.sizeRangeLabel !== ""
                                Layout.preferredWidth: sizeRangeText.implicitWidth + UiMetrics.spacing * 2
                                Layout.preferredHeight: Math.round(38 * UiMetrics.scale)
                                radius: height / 2
                                color: ThemeService.surfaceContainerHigh
                                border.width: 1
                                border.color: ThemeService.outline

                                Text {
                                    id: sizeRangeText
                                    anchors.centerIn: parent
                                    text: card.sizeRangeLabel
                                    font.pixelSize: UiMetrics.fontTiny
                                    font.bold: true
                                    color: ThemeService.onSurface
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }

                    MouseArea {
                        id: cardMouse
                        anchors.fill: parent
                        onClicked: root.widgetChosen(card.widgetId,
                                                     card.defaultCols,
                                                     card.defaultRows)
                    }
                }
            }
        }
    }
}
