import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

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
        updatePresetLabel()
    }

    function updatePresetLabel() {
        var name = EqualizerService.activePresetForStream(root.currentStream)
        presetLabel.text = name !== "" ? name : "Custom"
    }

    function buildPresetList() {
        presetListModel.clear()
        var bundled = EqualizerService.bundledPresetNames()
        for (var i = 0; i < bundled.length; i++)
            presetListModel.append({ name: bundled[i], isUser: false, isSection: false })
        var userNames = EqualizerService.userPresetNames()
        if (userNames.length > 0) {
            presetListModel.append({ name: "User Presets", isUser: false, isSection: true })
            for (var j = 0; j < userNames.length; j++)
                presetListModel.append({ name: userNames[j], isUser: true, isSection: false })
        }
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

        // Preset label (clickable -- opens picker)
        Item {
            Layout.preferredHeight: UiMetrics.touchMin
            Layout.preferredWidth: presetLabel.implicitWidth + dropdownArrow.implicitWidth + UiMetrics.spacing

            RowLayout {
                anchors.fill: parent
                spacing: UiMetrics.spacing / 2

                Text {
                    id: presetLabel
                    text: {
                        var name = EqualizerService.activePresetForStream(root.currentStream)
                        return name !== "" ? name : "Custom"
                    }
                    font.pixelSize: UiMetrics.fontSmall
                    color: ThemeService.descriptionFontColor
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    id: dropdownArrow
                    text: "\ue5c5"
                    font.family: "Material Icons"
                    font.pixelSize: UiMetrics.iconSmall
                    color: ThemeService.descriptionFontColor
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    root.buildPresetList()
                    presetPickerDialog.open()
                }
            }
        }

        // Save button
        Rectangle {
            Layout.preferredWidth: UiMetrics.touchMin
            Layout.preferredHeight: UiMetrics.touchMin
            radius: UiMetrics.radius / 2
            color: "transparent"

            Text {
                anchors.centerIn: parent
                text: "\ue161"
                font.family: "Material Icons"
                font.pixelSize: UiMetrics.iconSize
                color: ThemeService.descriptionFontColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    saveNameField.text = ""
                    savePresetDialog.open()
                }
            }
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

        function onPresetListChanged() {
            root.buildPresetList()
        }

        function onMediaPresetChanged() {
            if (root.currentStream === 0) root.updatePresetLabel()
        }

        function onNavigationPresetChanged() {
            if (root.currentStream === 1) root.updatePresetLabel()
        }

        function onPhonePresetChanged() {
            if (root.currentStream === 2) root.updatePresetLabel()
        }
    }

    Component.onCompleted: {
        refreshSliders()
    }

    // ========== Preset Picker Dialog ==========
    ListModel {
        id: presetListModel
    }

    Dialog {
        id: presetPickerDialog
        modal: true
        dim: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        parent: Overlay.overlay
        x: 0
        y: parent ? parent.height * 0.3 : 0
        width: parent ? parent.width : 0
        height: parent ? parent.height * 0.7 : 0

        padding: 0
        topPadding: 0
        bottomPadding: 0

        background: Rectangle {
            color: ThemeService.controlBoxBackgroundColor
            radius: UiMetrics.radius
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: UiMetrics.radius
                color: parent.color
            }
        }

        header: Item {
            implicitHeight: UiMetrics.headerH

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginPage
                anchors.rightMargin: UiMetrics.marginRow

                Text {
                    text: "Presets"
                    font.pixelSize: UiMetrics.fontTitle
                    font.bold: true
                    color: ThemeService.normalFontColor
                    Layout.fillWidth: true
                }

                MouseArea {
                    Layout.preferredWidth: UiMetrics.backBtnSize
                    Layout.preferredHeight: UiMetrics.backBtnSize
                    onClicked: presetPickerDialog.close()

                    Text {
                        anchors.centerIn: parent
                        text: "\ue5cd"
                        font.family: "Material Icons"
                        font.pixelSize: UiMetrics.iconSize
                        color: ThemeService.normalFontColor
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: ThemeService.controlBackgroundColor
            }
        }

        contentItem: ListView {
            id: presetList
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: presetListModel

            delegate: Loader {
                width: presetList.width
                property string itemName: model.name
                property bool itemIsUser: model.isUser
                property bool itemIsSection: model.isSection
                sourceComponent: itemIsSection ? sectionDelegate : presetRowDelegate
            }
        }
    }

    Component {
        id: sectionDelegate
        SectionHeader { text: parent ? parent.itemName : "" }
    }

    Component {
        id: presetRowDelegate

        Item {
            id: presetRowRoot
            width: parent ? parent.width : 0
            height: UiMetrics.rowH
            clip: true

            property string presetName: parent ? parent.itemName : ""
            property bool isUserPreset: parent ? parent.itemIsUser : false
            readonly property int deleteWidth: UiMetrics.rowH * 1.5

            // Delete background (revealed by swipe)
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: presetRowRoot.deleteWidth
                color: "#d32f2f"
                visible: presetRowRoot.isUserPreset

                Text {
                    anchors.centerIn: parent
                    text: "\ue872"
                    font.family: "Material Icons"
                    font.pixelSize: UiMetrics.iconSize
                    color: "white"
                }
            }

            // Swipeable content row
            Rectangle {
                id: contentRow
                width: parent.width
                height: parent.height
                color: ThemeService.controlBoxBackgroundColor
                x: 0

                Behavior on x { NumberAnimation { duration: 150 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: UiMetrics.marginPage
                    anchors.rightMargin: UiMetrics.marginPage
                    spacing: UiMetrics.gap

                    Text {
                        text: presetRowRoot.presetName
                        font.pixelSize: UiMetrics.fontBody
                        color: presetRowRoot.presetName === EqualizerService.activePresetForStream(root.currentStream)
                            ? ThemeService.highlightColor
                            : ThemeService.normalFontColor
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "\ue876"
                        font.family: "Material Icons"
                        font.pixelSize: UiMetrics.iconSize
                        color: ThemeService.highlightColor
                        visible: presetRowRoot.presetName === EqualizerService.activePresetForStream(root.currentStream)
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: UiMetrics.marginPage
                    anchors.rightMargin: UiMetrics.marginPage
                    height: 1
                    color: ThemeService.controlBackgroundColor
                }

                MouseArea {
                    anchors.fill: parent
                    drag.target: presetRowRoot.isUserPreset ? contentRow : undefined
                    drag.axis: Drag.XAxis
                    drag.minimumX: -presetRowRoot.deleteWidth
                    drag.maximumX: 0

                    onClicked: {
                        EqualizerService.applyPresetForStream(root.currentStream, presetRowRoot.presetName)
                        root.refreshSliders()
                        presetPickerDialog.close()
                    }

                    onReleased: {
                        if (!presetRowRoot.isUserPreset) return
                        if (contentRow.x < -presetRowRoot.deleteWidth / 2) {
                            EqualizerService.deleteUserPreset(presetRowRoot.presetName)
                            root.buildPresetList()
                            root.updatePresetLabel()
                        } else {
                            contentRow.x = 0
                        }
                    }
                }
            }
        }
    }

    // ========== Save Preset Dialog ==========
    Dialog {
        id: savePresetDialog
        modal: true
        dim: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        title: "Save Preset"

        parent: Overlay.overlay
        anchors.centerIn: parent

        width: parent ? Math.min(parent.width * 0.8, UiMetrics.marginPage * 20) : 0

        background: Rectangle {
            color: ThemeService.controlBoxBackgroundColor
            radius: UiMetrics.radius
        }

        header: Item {
            implicitHeight: UiMetrics.headerH

            Text {
                anchors.left: parent.left
                anchors.leftMargin: UiMetrics.marginPage
                anchors.verticalCenter: parent.verticalCenter
                text: "Save Preset"
                font.pixelSize: UiMetrics.fontTitle
                font.bold: true
                color: ThemeService.normalFontColor
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: ThemeService.controlBackgroundColor
            }
        }

        contentItem: ColumnLayout {
            spacing: UiMetrics.gap

            Item { Layout.preferredHeight: UiMetrics.spacing }

            TextField {
                id: saveNameField
                Layout.fillWidth: true
                Layout.leftMargin: UiMetrics.marginPage
                Layout.rightMargin: UiMetrics.marginPage
                placeholderText: "Enter preset name (or leave blank)"
                font.pixelSize: UiMetrics.fontBody
                color: ThemeService.normalFontColor

                background: Rectangle {
                    color: ThemeService.controlBackgroundColor
                    radius: UiMetrics.radius / 2
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: UiMetrics.marginPage
                Layout.rightMargin: UiMetrics.marginPage
                Layout.bottomMargin: UiMetrics.marginPage
                spacing: UiMetrics.gap

                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.preferredWidth: cancelBtnText.implicitWidth + UiMetrics.gap * 2
                    Layout.preferredHeight: UiMetrics.touchMin
                    radius: UiMetrics.radius / 2
                    color: ThemeService.controlBackgroundColor

                    Text {
                        id: cancelBtnText
                        anchors.centerIn: parent
                        text: "Cancel"
                        font.pixelSize: UiMetrics.fontBody
                        color: ThemeService.normalFontColor
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: savePresetDialog.close()
                    }
                }

                Rectangle {
                    Layout.preferredWidth: saveBtnText.implicitWidth + UiMetrics.gap * 2
                    Layout.preferredHeight: UiMetrics.touchMin
                    radius: UiMetrics.radius / 2
                    color: ThemeService.highlightColor

                    Text {
                        id: saveBtnText
                        anchors.centerIn: parent
                        text: "Save"
                        font.pixelSize: UiMetrics.fontBody
                        font.bold: true
                        color: ThemeService.backgroundColor
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            EqualizerService.saveUserPreset(root.currentStream, saveNameField.text)
                            root.updatePresetLabel()
                            savePresetDialog.close()
                        }
                    }
                }
            }
        }
    }
}
