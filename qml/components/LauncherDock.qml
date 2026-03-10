import QtQuick
import QtQuick.Effects
import QtQuick.Layouts

Item {
    id: dock
    height: UiMetrics.tileH * 0.5 + UiMetrics.spacing

    RowLayout {
        anchors.centerIn: parent
        spacing: UiMetrics.gridGap

        Repeater {
            model: LauncherModel

            Item {
                id: dockItem
                width: UiMetrics.tileW * 0.6
                height: UiMetrics.tileH * 0.45

                readonly property bool _isPressed: dockMa.pressed

                scale: _isPressed ? 0.95 : 1.0
                Behavior on scale { NumberAnimation { duration: 80 } }

                Rectangle {
                    id: dockBg
                    anchors.fill: parent
                    radius: UiMetrics.radiusSmall
                    color: dockItem._isPressed ? ThemeService.primaryContainer : ThemeService.surfaceContainerHigh
                    layer.enabled: true
                    visible: false
                }

                MultiEffect {
                    source: dockBg
                    anchors.fill: dockBg
                    shadowEnabled: true
                    shadowBlur: 0.4
                    shadowVerticalOffset: 2
                    shadowOpacity: 0.25
                    shadowColor: ThemeService.shadow
                    shadowHorizontalOffset: 0
                    shadowScale: 1.0
                    autoPaddingEnabled: true
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: UiMetrics.spacing * 0.25

                    MaterialIcon {
                        icon: model.tileIcon
                        size: UiMetrics.iconSmall
                        color: ThemeService.onSurface
                        Layout.alignment: Qt.AlignHCenter
                    }

                    NormalText {
                        text: model.tileLabel
                        font.pixelSize: UiMetrics.fontSmall
                        color: ThemeService.onSurface
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                MouseArea {
                    id: dockMa
                    anchors.fill: parent
                    onClicked: {
                        var action = model.tileAction
                        if (action.startsWith("plugin:")) {
                            PluginModel.setActivePlugin(action.substring(7))
                        } else if (action.startsWith("navigate:")) {
                            var target = action.substring(9)
                            if (target === "settings") {
                                PluginModel.setActivePlugin("")
                                ApplicationController.navigateTo(6)
                            }
                        }
                    }
                }
            }
        }
    }
}
