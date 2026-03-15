import QtQuick
import QtQuick.Layouts

Item {
    id: picker
    anchors.fill: parent

    property string targetPaneId: ""
    signal widgetSelected(string widgetId)
    signal cancelled()

    // Build category list from model whenever it resets
    property var categoryList: []

    function rebuildCategories() {
        var cats = [];
        var seen = {};
        for (var i = 0; i < WidgetPickerModel.rowCount(); ++i) {
            var idx = WidgetPickerModel.index(i, 0);
            var label = WidgetPickerModel.data(idx, 264); // CategoryLabelRole = UserRole+8 = 264
            var wid = WidgetPickerModel.data(idx, 257);   // WidgetIdRole = UserRole+1 = 257
            // Skip "No Widget" (empty label, empty id)
            if (label === "" && (wid === "" || wid === undefined)) continue;
            if (label !== "" && !seen[label]) {
                seen[label] = true;
                cats.push(label);
            }
        }
        categoryList = cats;
    }

    Connections {
        target: WidgetPickerModel
        function onModelReset() { picker.rebuildCategories(); }
    }

    Component.onCompleted: rebuildCategories()

    // Dim background
    Rectangle {
        anchors.fill: parent
        color: ThemeService.scrim
        opacity: 0.4

        MouseArea {
            anchors.fill: parent
            onClicked: picker.cancelled()
        }
    }

    // Picker panel
    Rectangle {
        id: panel
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        height: parent.height * 0.6
        radius: UiMetrics.radiusLarge
        color: ThemeService.surfaceContainerHighest
        clip: true

        // Slide up animation
        y: parent.height
        Component.onCompleted: y = parent.height - height - UiMetrics.marginPage

        Behavior on y {
            NumberAnimation { duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
        }

        Flickable {
            id: outerFlick
            anchors.fill: parent
            anchors.margins: UiMetrics.spacing
            contentHeight: contentCol.height
            clip: true
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: contentCol
                width: parent.width
                spacing: UiMetrics.spacing

                // --- "No Widget" button at the top ---
                Rectangle {
                    width: parent.width
                    height: UiMetrics.tileH * 0.3
                    radius: UiMetrics.radiusSmall
                    color: noWidgetMa.pressed ? ThemeService.primaryContainer : ThemeService.surfaceContainer

                    Row {
                        anchors.centerIn: parent
                        spacing: UiMetrics.spacing

                        MaterialIcon {
                            icon: "\ue5cd"
                            size: UiMetrics.iconSize
                            color: ThemeService.onSurface
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        NormalText {
                            text: "No Widget"
                            font.pixelSize: UiMetrics.fontBody
                            color: ThemeService.onSurface
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    MouseArea {
                        id: noWidgetMa
                        anchors.fill: parent
                        onClicked: picker.widgetSelected("")
                    }
                }

                // --- Category sections ---
                Repeater {
                    model: picker.categoryList

                    Column {
                        width: contentCol.width
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

                            model: {
                                // Filter WidgetPickerModel items by this category label
                                var items = [];
                                for (var i = 0; i < WidgetPickerModel.rowCount(); ++i) {
                                    var idx = WidgetPickerModel.index(i, 0);
                                    var itemLabel = WidgetPickerModel.data(idx, 264); // CategoryLabelRole
                                    if (itemLabel === catLabel) {
                                        items.push({
                                            widgetId: WidgetPickerModel.data(idx, 257),    // WidgetIdRole
                                            displayName: WidgetPickerModel.data(idx, 258), // DisplayNameRole
                                            iconName: WidgetPickerModel.data(idx, 259),    // IconNameRole
                                            description: WidgetPickerModel.data(idx, 263)  // DescriptionRole
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
                                    onClicked: picker.widgetSelected(modelData.widgetId)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
