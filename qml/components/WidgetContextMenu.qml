import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: contextMenu
    anchors.fill: parent

    property string targetPaneId: ""
    property real anchorX: 0
    property real anchorY: 0

    signal changeRequested()
    signal opacityChanged(real value)
    signal clearRequested()
    signal dismissed()

    property bool _opacityExpanded: false
    property real _currentOpacity: WidgetPlacementModel.paneOpacity(targetPaneId)

    // Scrim
    Rectangle {
        anchors.fill: parent
        color: ThemeService.scrim
        opacity: 0.4
        MouseArea {
            anchors.fill: parent
            onClicked: contextMenu.dismissed()
        }
    }

    // Floating card positioned near the pane
    Rectangle {
        id: card
        width: Math.min(UiMetrics.tileW * 0.8, parent.width * 0.4)
        height: cardContent.implicitHeight + UiMetrics.spacing * 2
        x: Math.max(UiMetrics.spacing, Math.min(anchorX - width / 2, parent.width - width - UiMetrics.spacing))
        y: Math.max(UiMetrics.spacing, Math.min(anchorY - height / 2, parent.height - height - UiMetrics.spacing))
        radius: UiMetrics.radiusLarge
        color: ThemeService.surfaceContainerHighest

        // Border
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 1
            border.color: ThemeService.outlineVariant
            opacity: 0.4
        }

        // Scale-in animation
        scale: 0.8
        Component.onCompleted: scale = 1.0
        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

        ColumnLayout {
            id: cardContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: UiMetrics.spacing
            spacing: UiMetrics.gap * 0.5

            // Change Widget option
            Rectangle {
                Layout.fillWidth: true
                height: UiMetrics.touchMin
                radius: UiMetrics.radiusSmall
                color: changeMa.pressed ? ThemeService.primaryContainer : ThemeService.surfaceContainer

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: UiMetrics.spacing
                    anchors.rightMargin: UiMetrics.spacing
                    spacing: UiMetrics.spacing

                    MaterialIcon {
                        icon: "\ue41f"  // swap_horiz
                        size: UiMetrics.iconSmall
                        color: ThemeService.onSurface
                    }
                    NormalText {
                        text: "Change Widget"
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.onSurface
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    id: changeMa
                    anchors.fill: parent
                    onClicked: contextMenu.changeRequested()
                }
            }

            // Opacity option
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: opacityContent.implicitHeight
                radius: UiMetrics.radiusSmall
                color: ThemeService.surfaceContainer

                ColumnLayout {
                    id: opacityContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 0

                    // Opacity header row
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: UiMetrics.touchMin

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: UiMetrics.spacing
                            anchors.rightMargin: UiMetrics.spacing
                            spacing: UiMetrics.spacing

                            MaterialIcon {
                                icon: "\ue3a8"  // opacity
                                size: UiMetrics.iconSmall
                                color: ThemeService.onSurface
                            }
                            NormalText {
                                text: "Opacity"
                                font.pixelSize: UiMetrics.fontBody
                                color: ThemeService.onSurface
                                Layout.fillWidth: true
                            }
                            NormalText {
                                text: Math.round(contextMenu._currentOpacity * 100) + "%"
                                font.pixelSize: UiMetrics.fontSmall
                                color: ThemeService.onSurfaceVariant
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: contextMenu._opacityExpanded = !contextMenu._opacityExpanded
                        }
                    }

                    // Slider row (visible when expanded)
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: _opacityExpanded ? UiMetrics.touchMin : 0
                        visible: _opacityExpanded
                        clip: true

                        Slider {
                            anchors.fill: parent
                            anchors.leftMargin: UiMetrics.spacing
                            anchors.rightMargin: UiMetrics.spacing
                            from: 0; to: 1; stepSize: 0.05
                            value: contextMenu._currentOpacity
                            onMoved: {
                                contextMenu._currentOpacity = value
                                WidgetPlacementModel.setPaneOpacity(contextMenu.targetPaneId, value)
                                contextMenu.opacityChanged(value)
                            }
                        }
                    }
                }
            }

            // Clear option — separate from opacity, with gap between
            Rectangle {
                Layout.fillWidth: true
                height: UiMetrics.touchMin
                radius: UiMetrics.radiusSmall
                color: clearMa.pressed ? ThemeService.errorContainer : ThemeService.surfaceContainer
                visible: WidgetPlacementModel.qmlComponentForPane(contextMenu.targetPaneId).toString() !== ""

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: UiMetrics.spacing
                    anchors.rightMargin: UiMetrics.spacing
                    spacing: UiMetrics.spacing

                    MaterialIcon {
                        icon: "\ue5cd"  // close
                        size: UiMetrics.iconSmall
                        color: ThemeService.error
                    }
                    NormalText {
                        text: "Clear"
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.error
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    id: clearMa
                    anchors.fill: parent
                    onClicked: contextMenu.clearRequested()
                }
            }
        }
    }
}
