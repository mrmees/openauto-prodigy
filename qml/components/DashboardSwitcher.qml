import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

/// Dashboard pill row — visible only while a widget is selected (edit mode).
/// Tap a pill to switch (via action, rail R4); "+" adds; long-press renames/removes.
Item {
    id: root
    property bool editing: false      // bound by HomeMenu to its selection state
    readonly property bool manageSheetOpen: manageSheet.visible
    visible: editing && DashboardManager.count > 0
    implicitHeight: UiMetrics.tileH * 0.35

    // Manage sheet has no reason to stay open once edit mode exits (pills hide)
    onEditingChanged: if (!editing && manageSheet.visible) manageSheet.close()

    RowLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        height: parent.height
        spacing: UiMetrics.spacing

        Repeater {
            model: DashboardManager.dashboardNames
            delegate: Rectangle {
                Layout.preferredHeight: root.implicitHeight
                Layout.preferredWidth: label.implicitWidth + UiMetrics.spacing * 3
                radius: height / 2
                color: index === DashboardManager.activeIndex
                       ? ThemeService.primaryContainer : ThemeService.surfaceContainer
                border.width: 1
                border.color: ThemeService.outlineVariant
                NormalText {
                    id: label
                    anchors.centerIn: parent
                    text: modelData
                    font.pixelSize: UiMetrics.fontBody
                    color: index === DashboardManager.activeIndex
                           ? ThemeService.onPrimaryContainer : ThemeService.onSurface
                }
                MouseArea {
                    anchors.fill: parent
                    // pressAndHold also fires clicked on release -- only dispatch the
                    // switch when this wasn't a long-press (Qt's documented guard)
                    onClicked: function(mouse) {
                        if (!mouse.wasHeld)
                            ActionRegistry.dispatch("app.dashboard.select",
                                                     DashboardManager.idAt(index))
                    }
                    onPressAndHold: manageSheet.openFor(DashboardManager.idAt(index), modelData)
                }
            }
        }

        Rectangle {  // add-dashboard chip
            Layout.preferredHeight: root.implicitHeight
            Layout.preferredWidth: root.implicitHeight
            radius: height / 2
            visible: DashboardManager.count < 8
            color: ThemeService.surfaceContainer
            border.width: 1; border.color: ThemeService.outlineVariant
            NormalText { anchors.centerIn: parent; text: "+"; color: ThemeService.onSurface }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    var id = DashboardManager.addDashboard(
                                 "Dash " + (DashboardManager.count + 1))
                    if (id !== "") ActionRegistry.dispatch("app.dashboard.select", id)
                }
            }
        }
    }

    // Rename / remove sheet
    Dialog {
        id: manageSheet
        parent: Overlay.overlay
        modal: true
        dim: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        anchors.centerIn: parent
        width: Math.min(parent ? parent.width * 0.6 : 360, 400)
        padding: UiMetrics.marginPage

        property string dashId: ""
        property string _originalName: ""  // skip no-op rename if unchanged

        function openFor(id, name) {
            dashId = id
            _originalName = name
            nameField.text = name
            open()
        }

        background: Rectangle {
            color: ThemeService.surfaceContainerHigh
            radius: UiMetrics.radius
            border.width: 1
            border.color: ThemeService.outline
        }

        contentItem: ColumnLayout {
            id: col
            spacing: UiMetrics.spacing

            TextField {
                id: nameField
                Layout.fillWidth: true
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.onSurface
                background: Rectangle {
                    color: ThemeService.surfaceContainerLow
                    radius: UiMetrics.radius / 2
                }
                onAccepted: {
                    if (text !== manageSheet._originalName)
                        DashboardManager.renameDashboard(manageSheet.dashId, text)
                    manageSheet.close()
                }
            }

            RowLayout {
                spacing: UiMetrics.spacing

                // touchMin-sized hit area, text kept visually unchanged
                Item {
                    Layout.preferredWidth: removeText.implicitWidth + UiMetrics.spacing * 2
                    Layout.preferredHeight: UiMetrics.touchMin

                    NormalText {
                        id: removeText
                        anchors.centerIn: parent
                        text: "Remove"
                        color: manageSheet.dashId === "home"
                               ? ThemeService.onSurfaceVariant : ThemeService.error
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: manageSheet.dashId !== "home"
                        onClicked: {
                            DashboardManager.removeDashboard(manageSheet.dashId)
                            manageSheet.close()
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Item {
                    Layout.preferredWidth: doneText.implicitWidth + UiMetrics.spacing * 2
                    Layout.preferredHeight: UiMetrics.touchMin

                    NormalText {
                        id: doneText
                        anchors.centerIn: parent
                        text: "Done"
                        color: ThemeService.primary
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (nameField.text !== manageSheet._originalName)
                                DashboardManager.renameDashboard(manageSheet.dashId, nameField.text)
                            manageSheet.close()
                        }
                    }
                }
            }
        }
    }
}
