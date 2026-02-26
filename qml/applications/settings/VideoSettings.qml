import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        SectionHeader { text: "Playback" }

        SegmentedButton {
            label: "FPS"
            configPath: "video.fps"
            options: ["30", "60"]
            values: [30, 60]
        }

        FullScreenPicker {
            label: "Resolution"
            configPath: "video.resolution"
            options: ["480p", "720p", "1080p"]
            values: ["480p", "720p", "1080p"]
        }

        Text {
            text: "FPS and resolution changes reconnect Android Auto automatically."
            font.pixelSize: UiMetrics.fontTiny
            color: ThemeService.descriptionFontColor
            font.italic: true
            Layout.leftMargin: UiMetrics.marginRow
        }

        SettingsSlider {
            label: "DPI"
            configPath: "video.dpi"
            from: 80; to: 400
            stepSize: 10
            restartRequired: true
        }

        SectionHeader { text: "Video Decoding" }

        Repeater {
            model: typeof CodecCapabilityModel !== "undefined" ? CodecCapabilityModel : null

            ColumnLayout {
                Layout.fillWidth: true
                spacing: UiMetrics.spacing

                // --- Codec enable/disable checkbox ---
                Item {
                    Layout.fillWidth: true
                    implicitHeight: UiMetrics.rowH

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: UiMetrics.marginRow
                        anchors.rightMargin: UiMetrics.marginRow
                        spacing: UiMetrics.gap

                        Text {
                            text: codecDisplayName(codecName)
                            font.pixelSize: UiMetrics.fontBody
                            color: ThemeService.normalFontColor
                            Layout.fillWidth: true
                        }

                        Switch {
                            id: codecSwitch
                            checked: codecEnabled
                            enabled: codecName !== "h264"
                            onToggled: {
                                CodecCapabilityModel.setEnabled(index, checked)
                                saveCodecConfig()
                            }
                        }
                    }
                }

                // --- Hardware / Software toggle ---
                Item {
                    Layout.fillWidth: true
                    implicitHeight: UiMetrics.rowH
                    visible: codecSwitch.checked

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: UiMetrics.marginRow * 3
                        anchors.rightMargin: UiMetrics.marginRow
                        spacing: UiMetrics.gap

                        Text {
                            text: "Decoder"
                            font.pixelSize: UiMetrics.fontBody
                            color: ThemeService.normalFontColor
                            Layout.fillWidth: true
                        }

                        Row {
                            spacing: 0

                            Rectangle {
                                width: Math.max(UiMetrics.touchMin * 1.5, swLabel.implicitWidth + UiMetrics.gap)
                                height: UiMetrics.touchMin
                                color: !isHardware ? ThemeService.highlightColor : ThemeService.controlBackgroundColor
                                radius: UiMetrics.radius

                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    width: parent.radius
                                    height: parent.height
                                    color: parent.color
                                }

                                Text {
                                    id: swLabel
                                    anchors.centerIn: parent
                                    text: "Software"
                                    font.pixelSize: UiMetrics.fontSmall
                                    color: !isHardware ? ThemeService.backgroundColor : ThemeService.normalFontColor
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        if (!isHardware) return
                                        CodecCapabilityModel.setHardwareMode(index, false)
                                        saveDecoderConfig(codecName, "auto")
                                    }
                                }
                            }

                            Rectangle {
                                width: Math.max(UiMetrics.touchMin * 1.5, hwLabel.implicitWidth + UiMetrics.gap)
                                height: UiMetrics.touchMin
                                color: isHardware ? ThemeService.highlightColor : ThemeService.controlBackgroundColor
                                opacity: hwAvailable ? 1.0 : 0.4
                                radius: UiMetrics.radius

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    width: parent.radius
                                    height: parent.height
                                    color: parent.color
                                }

                                Text {
                                    id: hwLabel
                                    anchors.centerIn: parent
                                    text: "Hardware"
                                    font.pixelSize: UiMetrics.fontSmall
                                    color: isHardware ? ThemeService.backgroundColor : ThemeService.normalFontColor
                                    opacity: hwAvailable ? 1.0 : 0.4
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: hwAvailable
                                    onClicked: {
                                        if (isHardware) return
                                        CodecCapabilityModel.setHardwareMode(index, true)
                                        saveDecoderConfig(codecName, "auto")
                                    }
                                }
                            }
                        }
                    }
                }

                // --- Decoder dropdown (shown when >1 real decoder, i.e. "auto" + 2+ decoders) ---
                Item {
                    Layout.fillWidth: true
                    implicitHeight: UiMetrics.rowH
                    visible: codecSwitch.checked && decoderList.length > 2

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: UiMetrics.marginRow * 3
                        anchors.rightMargin: UiMetrics.marginRow
                        spacing: UiMetrics.gap

                        Text {
                            text: "Decoder name"
                            font.pixelSize: UiMetrics.fontBody
                            color: ThemeService.descriptionFontColor
                            Layout.fillWidth: true
                        }

                        Text {
                            text: selectedDecoder
                            font.pixelSize: UiMetrics.fontBody
                            color: ThemeService.descriptionFontColor
                            horizontalAlignment: Text.AlignRight
                        }

                        MaterialIcon {
                            icon: "\ue5cf"
                            size: UiMetrics.iconSize
                            color: ThemeService.descriptionFontColor
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            decoderPickerDialog.codecIndex = index
                            decoderPickerDialog.codecLabel = codecDisplayName(codecName) + " Decoder"
                            decoderPickerDialog.decoders = decoderList
                            decoderPickerDialog.currentDecoder = selectedDecoder
                            decoderPickerDialog.decoderCodecName = codecName
                            decoderPickerDialog.open()
                        }
                    }
                }

                // --- Separator between codecs ---
                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: UiMetrics.marginRow
                    Layout.rightMargin: UiMetrics.marginRow
                    height: 1
                    color: ThemeService.controlBackgroundColor
                    visible: index < (typeof CodecCapabilityModel !== "undefined" ? CodecCapabilityModel.rowCount() - 1 : 0)
                }
            }
        }

        Text {
            text: "Codec and decoder changes take effect on next AA connection."
            font.pixelSize: UiMetrics.fontTiny
            color: ThemeService.descriptionFontColor
            font.italic: true
            Layout.leftMargin: UiMetrics.marginRow
        }

        SectionHeader { text: "Sidebar" }

        SettingsToggle {
            label: "Show sidebar during Android Auto"
            configPath: "video.sidebar.enabled"
            restartRequired: true
        }

        SegmentedButton {
            label: "Position"
            configPath: "video.sidebar.position"
            options: ["Left", "Right"]
            values: ["left", "right"]
            restartRequired: true
        }

        SettingsSlider {
            label: "Sidebar Width (px)"
            configPath: "video.sidebar.width"
            from: 80; to: 300
            stepSize: 10
            restartRequired: true
        }

        Text {
            text: "Sidebar changes take effect on next app restart."
            font.pixelSize: UiMetrics.fontTiny
            color: ThemeService.descriptionFontColor
            font.italic: true
            Layout.leftMargin: UiMetrics.marginRow
        }
    }

    // --- Decoder picker dialog ---
    Dialog {
        id: decoderPickerDialog
        modal: true
        dim: true

        property int codecIndex: -1
        property string codecLabel: ""
        property var decoders: []
        property string currentDecoder: "auto"
        property string decoderCodecName: ""

        parent: Overlay.overlay
        x: 0
        y: parent ? parent.height * 0.4 : 0
        width: parent ? parent.width : 0
        height: parent ? parent.height * 0.6 : 0
        padding: 0; topPadding: 0; bottomPadding: 0

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
                    text: decoderPickerDialog.codecLabel
                    font.pixelSize: UiMetrics.fontTitle
                    font.bold: true
                    color: ThemeService.normalFontColor
                    Layout.fillWidth: true
                }

                MouseArea {
                    Layout.preferredWidth: UiMetrics.backBtnSize
                    Layout.preferredHeight: UiMetrics.backBtnSize
                    onClicked: decoderPickerDialog.close()

                    MaterialIcon {
                        anchors.centerIn: parent
                        icon: "\ue5cd"
                        size: UiMetrics.iconSize
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
            id: decoderListView
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: decoderPickerDialog.decoders

            delegate: Item {
                width: decoderListView.width
                height: UiMetrics.rowH

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: UiMetrics.marginPage
                    anchors.rightMargin: UiMetrics.marginPage
                    spacing: UiMetrics.gap

                    Text {
                        text: modelData
                        font.pixelSize: UiMetrics.fontBody
                        color: modelData === decoderPickerDialog.currentDecoder
                            ? ThemeService.highlightColor : ThemeService.normalFontColor
                        Layout.fillWidth: true
                    }

                    MaterialIcon {
                        icon: "\ue876"
                        size: UiMetrics.iconSize
                        color: ThemeService.highlightColor
                        visible: modelData === decoderPickerDialog.currentDecoder
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.leftMargin: UiMetrics.marginPage
                    anchors.rightMargin: UiMetrics.marginPage
                    height: 1
                    color: ThemeService.controlBackgroundColor
                    visible: index < decoderPickerDialog.decoders.length - 1
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        CodecCapabilityModel.setSelectedDecoder(decoderPickerDialog.codecIndex, modelData)
                        saveDecoderConfig(decoderPickerDialog.decoderCodecName, modelData)
                        decoderPickerDialog.close()
                    }
                }
            }
        }
    }

    // --- Display name helper ---
    function codecDisplayName(name) {
        if (name === "h264") return "H.264"
        if (name === "h265") return "H.265"
        if (name === "vp9") return "VP9"
        if (name === "av1") return "AV1"
        return name
    }

    // --- Config persistence helpers ---

    function saveCodecConfig() {
        if (typeof CodecCapabilityModel === "undefined") return
        var enabled = []
        for (var i = 0; i < CodecCapabilityModel.rowCount(); i++) {
            var idx = CodecCapabilityModel.index(i, 0)
            if (CodecCapabilityModel.data(idx, 258)) { // EnabledRole
                enabled.push(CodecCapabilityModel.data(idx, 257)) // CodecNameRole
            }
        }
        ConfigService.setValue("video.codecs", enabled)
        ConfigService.save()
    }

    function saveDecoderConfig(codec, decoder) {
        ConfigService.setValue("video.decoder." + codec, decoder)
        ConfigService.save()
    }

    // --- Load saved config into model on startup ---
    Component.onCompleted: {
        if (typeof CodecCapabilityModel === "undefined") return

        // Load enabled codecs from config
        var enabledList = ConfigService.value("video.codecs")
        if (enabledList && Array.isArray(enabledList)) {
            for (var i = 0; i < CodecCapabilityModel.rowCount(); i++) {
                var idx = CodecCapabilityModel.index(i, 0)
                var name = CodecCapabilityModel.data(idx, 257) // CodecNameRole
                if (name === "h264") continue // always enabled
                CodecCapabilityModel.setEnabled(i, enabledList.indexOf(name) >= 0)
            }
        }

        // Load decoder selections from config and restore hw/sw mode
        for (var j = 0; j < CodecCapabilityModel.rowCount(); j++) {
            var idx2 = CodecCapabilityModel.index(j, 0)
            var codecName = CodecCapabilityModel.data(idx2, 257) // CodecNameRole
            var decoder = ConfigService.value("video.decoder." + codecName)
            if (decoder && decoder !== "auto") {
                if (CodecCapabilityModel.isHwDecoder(j, decoder)) {
                    CodecCapabilityModel.setHardwareMode(j, true)
                }
                CodecCapabilityModel.setSelectedDecoder(j, decoder)
            }
        }
    }
}
