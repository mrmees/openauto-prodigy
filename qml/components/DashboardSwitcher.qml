import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

/// Dashboard pill row — visible only while a widget is selected (edit mode).
/// Tap a pill to switch (via action, rail R4); "+" adds; long-press renames/removes.
Item {
    id: root
    property bool editing: false      // bound by HomeMenu to its selection state
    visible: editing && DashboardManager.count > 0
    implicitHeight: UiMetrics.tileH * 0.35

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
                    onClicked: ActionRegistry.dispatch("app.dashboard.select",
                                                       DashboardManager.idAt(index))
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
    Rectangle {
        id: manageSheet
        property string dashId: ""
        function openFor(id, name) { dashId = id; nameField.text = name; visible = true }
        visible: false
        anchors.centerIn: parent
        width: 360; height: col.implicitHeight + UiMetrics.marginPage * 2
        radius: UiMetrics.radius
        color: ThemeService.surfaceContainerHigh
        border.width: 1; border.color: ThemeService.outline
        z: 50
        ColumnLayout {
            id: col
            anchors.centerIn: parent
            width: parent.width - UiMetrics.marginPage * 2
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
                    DashboardManager.renameDashboard(manageSheet.dashId, text)
                    manageSheet.visible = false
                }
            }
            RowLayout {
                spacing: UiMetrics.spacing
                NormalText {
                    text: "Remove"
                    color: manageSheet.dashId === "home"
                           ? ThemeService.onSurfaceVariant : ThemeService.error
                    MouseArea {
                        anchors.fill: parent
                        enabled: manageSheet.dashId !== "home"
                        onClicked: {
                            DashboardManager.removeDashboard(manageSheet.dashId)
                            manageSheet.visible = false
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                NormalText {
                    text: "Done"
                    color: ThemeService.primary
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            DashboardManager.renameDashboard(manageSheet.dashId, nameField.text)
                            manageSheet.visible = false
                        }
                    }
                }
            }
        }
    }
}
