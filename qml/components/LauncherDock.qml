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
                implicitWidth: UiMetrics.tileW * 0.6
                implicitHeight: UiMetrics.tileH * 0.45

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

                MaterialIcon {
                    anchors.centerIn: parent
                    icon: model.tileIcon
                    size: Math.round(dockItem.height * 0.9)
                    opticalSize: Math.min(size, 48)
                    color: ThemeService.onSurface
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
