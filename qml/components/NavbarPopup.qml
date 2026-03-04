import QtQuick
import QtQuick.Layouts

/// Popup slider for volume or brightness, anchored to a navbar control.
/// Always renders as a vertical slider regardless of navbar edge.
Item {
    id: root

    property int controlIndex: 0
    property bool isVolume: true
    property Item anchorItem: null
    property string navbarEdge: "bottom"

    // Popup dimensions — full screen height for easy touch target
    readonly property int popupW: UiMetrics.touchMin * 3
    readonly property int popupH: root.parent ? root.parent.height : Math.round(400 * UiMetrics.scale)
    readonly property int sliderTrackW: Math.round(UiMetrics.trackThick * 2.5)
    readonly property int knobDiameter: Math.round(UiMetrics.knobSize * 1.5)

    visible: NavbarController.popupVisible && NavbarController.popupControlIndex === root.controlIndex
    z: 200

    // Position: full height, centered horizontally on the anchor control
    onVisibleChanged: {
        if (visible && anchorItem) {
            var pos = anchorItem.mapToItem(root.parent, 0, 0)

            // Always span full height
            root.y = 0

            if (navbarEdge === "left") {
                root.x = pos.x + anchorItem.width + UiMetrics.spacing
            } else if (navbarEdge === "right") {
                root.x = pos.x - popupW - UiMetrics.spacing
            } else {
                // top/bottom: center on the control
                var centerX = pos.x + anchorItem.width / 2 - popupW / 2
                if (centerX < 0) centerX = 0
                if (centerX + popupW > parent.width) centerX = parent.width - popupW
                root.x = centerX
            }
        }
    }

    width: popupW
    height: popupH

    // Current value binding
    readonly property int currentValue: root.isVolume
        ? (typeof AudioService !== "undefined" ? AudioService.masterVolume : 50)
        : (typeof DisplayService !== "undefined" ? DisplayService.brightness : 80)
    readonly property int minVal: root.isVolume ? 0 : 5
    readonly property int maxVal: 100

    // Background
    Rectangle {
        anchors.fill: parent
        color: ThemeService.controlBoxBackgroundColor
        radius: UiMetrics.radius
        border.color: ThemeService.barShadowColor
        border.width: 1

        // Prevent tap-through
        MouseArea {
            anchors.fill: parent
            onPressed: function(mouse) { mouse.accepted = true }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: UiMetrics.marginRow
            spacing: UiMetrics.spacing

            // Value label
            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: UiMetrics.spacing
                text: root.currentValue
                font.pixelSize: UiMetrics.fontHeading
                font.bold: true
                color: ThemeService.normalFontColor
            }

            // Slider track area — with vertical padding so knob doesn't clip
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: root.knobDiameter / 2
                Layout.bottomMargin: root.knobDiameter / 2 + UiMetrics.spacing

                // Track background
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: root.sliderTrackW
                    radius: root.sliderTrackW / 2
                    color: ThemeService.controlBackgroundColor

                    // Filled portion (from bottom)
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: parent.height * (root.currentValue - root.minVal) / (root.maxVal - root.minVal)
                        radius: parent.radius
                        color: ThemeService.highlightColor
                    }
                }

                // Knob
                Rectangle {
                    id: knob
                    width: root.knobDiameter
                    height: root.knobDiameter
                    radius: root.knobDiameter / 2
                    color: ThemeService.highlightColor
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height - (parent.height * (root.currentValue - root.minVal) / (root.maxVal - root.minVal)) - root.knobDiameter / 2
                }

                // Drag area — extends beyond track for easy finger targeting
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -UiMetrics.spacing
                    onPressed: function(mouse) { updateValue(mouse.y + UiMetrics.spacing) }
                    onPositionChanged: function(mouse) { updateValue(mouse.y + UiMetrics.spacing) }

                    function updateValue(mouseY) {
                        var normalized = 1.0 - (mouseY / parent.height)
                        normalized = Math.max(0.0, Math.min(1.0, normalized))
                        var val = Math.round(root.minVal + normalized * (root.maxVal - root.minVal))
                        if (root.isVolume) {
                            if (typeof AudioService !== "undefined")
                                AudioService.setMasterVolume(val)
                        } else {
                            if (typeof DisplayService !== "undefined")
                                DisplayService.setBrightness(val)
                        }
                        // Reset auto-dismiss timer on interaction
                        NavbarController.showPopup(root.controlIndex)
                    }
                }
            }
        }
    }
}
