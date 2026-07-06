import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root

    signal widgetChosen(string widgetId, int defaultCols, int defaultRows)
    readonly property bool isOpen: pickerDialog.visible

    property int gridCols: 1
    property int gridRows: 1

    property var categoryList: []

    function openPicker() {
        WidgetPickerModel.filterByAvailableSpace(gridCols, gridRows, false)
        rebuildCategories()
        pickerDialog.open()
    }

    function closePicker() {
        pickerDialog.close()
    }

    function rebuildCategories() {
        var cats = [];
        var seen = {};
        for (var i = 0; i < WidgetPickerModel.rowCount(); ++i) {
            var idx = WidgetPickerModel.index(i, 0);
            var label = WidgetPickerModel.data(idx, 264); // CategoryLabelRole
            var wid = WidgetPickerModel.data(idx, 257);   // WidgetIdRole
            if (label === "" && (wid === "" || wid === undefined)) continue;
            if (label !== "" && !seen[label]) {
                seen[label] = true;
                cats.push(label);
            }
        }
        categoryList = cats;
    }

    // Paid-alternative-parity size options: fixed preset set clamped to the descriptor's bounds
    function presetsFor(w) {
        var candidates = [[1,1],[2,1],[2,2],[3,2]]
        var out = []
        for (var i = 0; i < candidates.length; ++i) {
            var c = candidates[i][0], r = candidates[i][1]
            if (c >= w.minCols && c <= w.maxCols && r >= w.minRows && r <= w.maxRows)
                out.push({cols: c, rows: r})
        }
        if (out.length === 0) out.push({cols: w.defaultCols, rows: w.defaultRows})
        return out
    }

    Connections {
        target: WidgetPickerModel
        function onModelReset() { root.rebuildCategories(); }
    }

    Dialog {
        id: pickerDialog
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
                from: pickerDialog.parent ? pickerDialog.parent.height : 0
                to: pickerDialog.parent ? pickerDialog.parent.height * 0.4 : 0
                duration: 250
                easing.type: Easing.OutCubic
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "y"
                from: pickerDialog.parent ? pickerDialog.parent.height * 0.4 : 0
                to: pickerDialog.parent ? pickerDialog.parent.height : 0
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
                    icon: "\ue1bd"
                    size: UiMetrics.iconSize
                    color: ThemeService.onSurface
                }

                Text {
                    text: "Add Widget"
                    font.pixelSize: UiMetrics.fontTitle
                    font.bold: true
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                }

                MouseArea {
                    Layout.preferredWidth: UiMetrics.backBtnSize
                    Layout.preferredHeight: UiMetrics.backBtnSize
                    onClicked: pickerDialog.close()

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

        contentItem: Item {
            id: pickerContentArea

            Flickable {
                id: pickerFlickable
                anchors.fill: parent
                clip: true
                contentHeight: pickerColumn.height
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick

            Column {
                id: pickerColumn
                width: parent.width
                spacing: UiMetrics.spacing

                topPadding: UiMetrics.spacing
                bottomPadding: UiMetrics.spacing

                Repeater {
                    model: root.categoryList

                    Column {
                        width: pickerColumn.width
                        spacing: UiMetrics.spacing * 0.5

                        property string catLabel: modelData

                        // Category header
                        NormalText {
                            text: catLabel
                            font.pixelSize: UiMetrics.fontBody
                            color: ThemeService.onSurfaceVariant
                            leftPadding: UiMetrics.marginPage * 0.5
                        }

                        // Horizontal card row
                        ListView {
                            id: cardRow
                            width: parent.width
                            height: UiMetrics.tileH * 0.55
                            orientation: ListView.Horizontal
                            spacing: UiMetrics.spacing
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            leftMargin: UiMetrics.marginPage * 0.5

                            model: {
                                var items = [];
                                for (var i = 0; i < WidgetPickerModel.rowCount(); ++i) {
                                    var idx = WidgetPickerModel.index(i, 0);
                                    var itemLabel = WidgetPickerModel.data(idx, 264); // CategoryLabelRole
                                    if (itemLabel === catLabel) {
                                        items.push({
                                            widgetId: WidgetPickerModel.data(idx, 257),    // WidgetIdRole
                                            displayName: WidgetPickerModel.data(idx, 258), // DisplayNameRole
                                            iconName: WidgetPickerModel.data(idx, 259),    // IconNameRole
                                            description: WidgetPickerModel.data(idx, 263), // DescriptionRole
                                            defaultCols: WidgetPickerModel.data(idx, 260), // DefaultColsRole
                                            defaultRows: WidgetPickerModel.data(idx, 261), // DefaultRowsRole
                                            minCols: WidgetPickerModel.data(idx, 265),     // MinColsRole
                                            minRows: WidgetPickerModel.data(idx, 266),     // MinRowsRole
                                            maxCols: WidgetPickerModel.data(idx, 267),     // MaxColsRole
                                            maxRows: WidgetPickerModel.data(idx, 268)      // MaxRowsRole
                                        });
                                    }
                                }
                                return items;
                            }

                            delegate: Rectangle {
                                width: UiMetrics.tileW * 0.55
                                height: cardRow.height
                                radius: UiMetrics.radius
                                color: cardMa.pressed ? ThemeService.primaryContainer : ThemeService.surfaceContainer

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: UiMetrics.spacing
                                    spacing: UiMetrics.spacing * 0.5

                                    MaterialIcon {
                                        icon: modelData.iconName
                                        size: UiMetrics.iconSize
                                        color: ThemeService.onSurface
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                    NormalText {
                                        text: modelData.displayName
                                        font.pixelSize: UiMetrics.fontBody
                                        color: ThemeService.onSurface
                                        Layout.alignment: Qt.AlignHCenter
                                        horizontalAlignment: Text.AlignHCenter
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                    NormalText {
                                        text: modelData.description || ""
                                        font.pixelSize: UiMetrics.fontSmall
                                        color: ThemeService.onSurfaceVariant
                                        Layout.alignment: Qt.AlignHCenter
                                        horizontalAlignment: Text.AlignHCenter
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                    Item { Layout.fillHeight: true }
                                }

                                MouseArea {
                                    id: cardMa
                                    anchors.fill: parent
                                    onClicked: {
                                        var presets = root.presetsFor(modelData)
                                        if (presets.length <= 1)
                                            root.widgetChosen(modelData.widgetId, modelData.defaultCols, modelData.defaultRows)
                                        else
                                            sizePopup.openFor(modelData, presets)
                                    }
                                }
                            }
                        }
                    }
                }
            }
            }

            // In-sheet size preset popup — nested inside the Dialog's content area
            // (not a sibling of the Dialog under root) so it actually renders above
            // the open Dialog's cards: the Dialog reparents to Overlay.overlay, which
            // composites above root's normal children, so a popup living under root
            // would be hidden behind the modal Dialog instead of stacking above it.
            Rectangle {
                id: sizePopup
                property var widget: null
                property var presets: []
                function openFor(w, p) { widget = w; presets = p; visible = true }
                visible: false
                anchors.centerIn: parent
                width: presetRow.implicitWidth + UiMetrics.marginPage * 2
                height: presetRow.implicitHeight + UiMetrics.marginPage * 2
                radius: UiMetrics.radius
                color: ThemeService.surfaceContainerHigh
                border.width: 1; border.color: ThemeService.outline
                z: 10
                Row {
                    id: presetRow
                    anchors.centerIn: parent
                    spacing: UiMetrics.spacing
                    Repeater {
                        model: sizePopup.presets
                        delegate: Rectangle {
                            width: UiMetrics.tileW * 0.3; height: UiMetrics.tileH * 0.3
                            radius: UiMetrics.radiusSmall
                            color: ThemeService.surfaceContainer
                            border.width: 1; border.color: ThemeService.outlineVariant
                            NormalText {
                                anchors.centerIn: parent
                                text: modelData.cols + "×" + modelData.rows
                                color: ThemeService.onSurface
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    root.widgetChosen(sizePopup.widget.widgetId,
                                                      modelData.cols, modelData.rows)
                                    sizePopup.visible = false
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
