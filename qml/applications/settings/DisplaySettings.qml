import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    property real _currentScale: {
        var v = ConfigService.value("ui.scale")
        if (v !== undefined && v !== null && Number(v) > 0) return Number(v)
        return 1.0
    }
    Connections {
        target: ConfigService
        function onConfigChanged(path, value) {
            if (path === "ui.scale") {
                root._currentScale = (value !== undefined && value !== null && Number(value) > 0) ? Number(value) : 1.0
            }
        }
    }

    function _applyScale(newVal) {
        ConfigService.setValue("ui.scale", newVal)
        ConfigService.save()
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: UiMetrics.marginPage
        spacing: 0

        SettingsRow { rowIndex: 0
            ReadOnlyField {
                label: "Screen"
                value: {
                    var size = DisplayInfo ? DisplayInfo.screenSizeInches : 7.0;
                    var dpi = DisplayInfo ? DisplayInfo.computedDpi : 170;
                    return size.toFixed(1) + "\" / " + dpi + " PPI";
                }
            }
        }

        SettingsRow { rowIndex: 1
            // Scale stepper: [-] value [+] (reset)
            Item {
                anchors.fill: parent

                RowLayout {
                    anchors.fill: parent
                    spacing: UiMetrics.gap

                    Text {
                        text: "UI Scale"
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.onSurface
                        Layout.fillWidth: true
                    }

                    // Reset button (left of controls so [-] [+] don't shift)
                    Rectangle {
                        width: UiMetrics.touchMin
                        height: UiMetrics.touchMin
                        radius: UiMetrics.touchMin / 2
                        color: "transparent"
                        border.color: Math.abs(root._currentScale - 1.0) > 0.05 ? ThemeService.onSurfaceVariant : "transparent"
                        border.width: 1
                        opacity: Math.abs(root._currentScale - 1.0) > 0.05 ? 1.0 : 0.0

                        MaterialIcon {
                            anchors.centerIn: parent
                            icon: "\ue042"
                            size: UiMetrics.iconSmall
                            color: ThemeService.onSurfaceVariant
                        }
                        MouseArea {
                            anchors.fill: parent
                            enabled: Math.abs(root._currentScale - 1.0) > 0.05
                            onClicked: root._applyScale(1.0)
                        }
                    }

                    // [-] button
                    Rectangle {
                        width: UiMetrics.touchMin
                        height: UiMetrics.touchMin
                        radius: UiMetrics.radius
                        color: root._currentScale <= 0.5 ? ThemeService.surfaceContainerLow : ThemeService.primary
                        opacity: root._currentScale <= 0.5 ? 0.3 : 1.0

                        Text {
                            anchors.centerIn: parent
                            text: "\u2212"
                            font.pixelSize: UiMetrics.fontHeading
                            font.weight: Font.Bold
                            color: "white"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (root._currentScale <= 0.5) return
                                var newVal = Math.round((root._currentScale - 0.1) * 10) / 10
                                root._applyScale(newVal)
                            }
                        }
                    }

                    // Value display
                    Text {
                        text: root._currentScale.toFixed(1)
                        font.pixelSize: UiMetrics.fontTitle
                        font.weight: Font.DemiBold
                        color: ThemeService.onSurface
                        horizontalAlignment: Text.AlignHCenter
                        Layout.preferredWidth: UiMetrics.touchMin
                    }

                    // [+] button
                    Rectangle {
                        width: UiMetrics.touchMin
                        height: UiMetrics.touchMin
                        radius: UiMetrics.radius
                        color: root._currentScale >= 2.0 ? ThemeService.surfaceContainerLow : ThemeService.primary
                        opacity: root._currentScale >= 2.0 ? 0.3 : 1.0

                        Text {
                            anchors.centerIn: parent
                            text: "+"
                            font.pixelSize: UiMetrics.fontHeading
                            font.weight: Font.Bold
                            color: "white"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (root._currentScale >= 2.0) return
                                var newVal = Math.round((root._currentScale + 0.1) * 10) / 10
                                root._applyScale(newVal)
                            }
                        }
                    }
                }
            }
        }

        SectionHeader { text: "Display" }

        SettingsRow { rowIndex: 0
            SettingsSlider {
                id: brightnessSlider
                label: typeof DisplayService !== "undefined" && DisplayService.hasHardwareBrightness ? "Brightness" : "Screen Dimming"
                configPath: "display.brightness"
                from: 5; to: 100; stepSize: 1
                onMoved: {
                    if (typeof DisplayService !== "undefined")
                        DisplayService.setBrightness(Math.round(value))
                }
            }
        }

        SectionHeader { text: "Navbar" }

        SettingsRow { rowIndex: 0
            FullScreenPicker {
                flat: true
                label: "Navbar Position"
                configPath: "navbar.edge"
                options: ["Bottom", "Top", "Left", "Right"]
                values: ["bottom", "top", "left", "right"]
                onActivated: function(index) {
                    var edges = ["bottom", "top", "left", "right"]
                    NavbarController.setEdge(edges[index])
                }
            }
        }

        SettingsRow { rowIndex: 1
            SettingsToggle {
                label: "Show Navbar during Android Auto"
                configPath: "navbar.show_during_aa"
                restartRequired: true
            }
        }
    }
}
