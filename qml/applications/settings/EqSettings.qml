import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property int currentStream: 0
    property bool currentBypassed: false

    function refreshSliders() {
        var gains = EqualizerService.gainsAsList(currentStream)
        for (var i = 0; i < sliderRepeater.count; i++) {
            var item = sliderRepeater.itemAt(i)
            if (item && i < gains.length)
                item.gainValue = gains[i]
        }
        currentBypassed = EqualizerService.isBypassedForStream(currentStream)
    }

    // Control bar
    RowLayout {
        id: controlBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        height: UiMetrics.rowH
        spacing: UiMetrics.gap

        // Stream selector (no configPath -- transient UI state)
        SegmentedButton {
            label: ""
            configPath: ""
            options: ["Media", "Nav", "Phone"]
            Layout.fillWidth: true

            onCurrentIndexChanged: {
                root.currentStream = currentIndex
                root.refreshSliders()
            }
        }

        // Preset label (non-interactive for now)
        Text {
            id: presetLabel
            text: {
                var name = EqualizerService.activePresetForStream(root.currentStream)
                return name !== "" ? name : "Custom"
            }
            font.pixelSize: UiMetrics.fontSmall
            color: ThemeService.descriptionFontColor
            horizontalAlignment: Text.AlignHCenter
            Layout.preferredWidth: implicitWidth + UiMetrics.gap
        }

        // Bypass badge
        Rectangle {
            id: bypassBadge
            Layout.preferredWidth: bypassText.implicitWidth + UiMetrics.gap * 2
            Layout.preferredHeight: UiMetrics.touchMin
            radius: UiMetrics.radius / 2
            color: root.currentBypassed ? ThemeService.highlightColor : ThemeService.controlBackgroundColor

            Behavior on color { ColorAnimation { duration: 150 } }

            Text {
                id: bypassText
                anchors.centerIn: parent
                text: "BYPASS"
                font.pixelSize: UiMetrics.fontTiny
                font.bold: true
                color: root.currentBypassed ? ThemeService.backgroundColor : ThemeService.descriptionFontColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    var newState = !root.currentBypassed
                    EqualizerService.setBypassedForStream(root.currentStream, newState)
                    root.currentBypassed = newState
                }
            }
        }
    }

    // Slider area
    Row {
        id: sliderRow
        anchors.top: controlBar.bottom
        anchors.topMargin: UiMetrics.spacing
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: UiMetrics.marginPage / 2
        anchors.rightMargin: UiMetrics.marginPage / 2

        Repeater {
            id: sliderRepeater
            model: EqualizerService.bandCount()

            EqBandSlider {
                width: sliderRow.width / 10
                height: sliderRow.height
                bandIndex: index
                freqLabel: EqualizerService.bandLabel(index)
                bypassed: root.currentBypassed

                onGainChanged: function(dB) {
                    EqualizerService.setGainForStream(root.currentStream, bandIndex, dB)
                }

                onResetRequested: {
                    EqualizerService.setGainForStream(root.currentStream, bandIndex, 0)
                }
            }
        }
    }

    // React to external changes
    Connections {
        target: EqualizerService

        function onGainsChangedForStream(stream) {
            if (stream === root.currentStream)
                root.refreshSliders()
        }

        function onBypassedChanged(stream) {
            if (stream === root.currentStream)
                root.currentBypassed = EqualizerService.isBypassedForStream(root.currentStream)
        }
    }

    Component.onCompleted: {
        refreshSliders()
    }
}
