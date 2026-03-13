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
        anchors.leftMargin: UiMetrics.settingsPageInset
        anchors.rightMargin: UiMetrics.settingsPageInset
        anchors.topMargin: UiMetrics.marginPage
        spacing: 0

        property bool aaConnected: typeof ProjectionStatus !== "undefined"
                                  && ProjectionStatus !== null
                                  && ProjectionStatus.projectionState === 3

        SectionHeader { text: "Video Decoding" }

        Repeater {
            model: typeof CodecCapabilityModel !== "undefined" ? CodecCapabilityModel : null

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                // --- Codec enable/disable row ---
                SettingsRow {
                    rowIndex: index * 3

                    Item {
                        anchors.fill: parent

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: UiMetrics.marginRow
                            anchors.rightMargin: UiMetrics.marginRow
                            spacing: UiMetrics.gap

                            Text {
                                text: codecDisplayName(codecName)
                                font.pixelSize: UiMetrics.fontBody
                                color: ThemeService.onSurface
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

                    SettingsHoldArea {
                        anchors.fill: parent
                        enableBackHold: false
                        onShortClicked: {
                            if (codecName === "h264")
                                return
                            codecSwitch.checked = !codecSwitch.checked
                        }
                    }
                }

                // --- Hardware / Software toggle ---
                SettingsRow {
                    rowIndex: index * 3 + 1
                    visible: codecSwitch.checked

                    Item {
                        anchors.fill: parent
                        anchors.leftMargin: UiMetrics.marginRow * 2

                        RowLayout {
                            anchors.fill: parent
                            anchors.rightMargin: UiMetrics.marginRow
                            spacing: UiMetrics.gap

                            Text {
                                text: "Decoder"
                                font.pixelSize: UiMetrics.fontBody
                                color: ThemeService.onSurface
                                Layout.fillWidth: true
                            }

                            Row {
                                spacing: 0

                                Rectangle {
                                    width: Math.max(UiMetrics.touchMin * 1.5, swLabel.implicitWidth + UiMetrics.gap)
                                    height: UiMetrics.touchMin
                                    color: !isHardware ? ThemeService.secondaryContainer : ThemeService.surfaceContainerLow
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
                                        color: !isHardware ? ThemeService.onSecondaryContainer : ThemeService.onSurface
                                    }

                                    SettingsHoldArea {
                                        anchors.fill: parent
                                        enableBackHold: false
                                        onShortClicked: {
                                            if (!isHardware) return
                                            CodecCapabilityModel.setHardwareMode(index, false)
                                            saveDecoderConfig(codecName, "auto")
                                        }
                                    }
                                }

                                Rectangle {
                                    width: Math.max(UiMetrics.touchMin * 1.5, hwLabel.implicitWidth + UiMetrics.gap)
                                    height: UiMetrics.touchMin
                                    color: isHardware ? ThemeService.secondaryContainer : ThemeService.surfaceContainerLow
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
                                        color: isHardware ? ThemeService.onSecondaryContainer : ThemeService.onSurface
                                        opacity: hwAvailable ? 1.0 : 0.4
                                    }

                                    SettingsHoldArea {
                                        anchors.fill: parent
                                        enabled: hwAvailable
                                        enableBackHold: false
                                        onShortClicked: {
                                            if (isHardware) return
                                            CodecCapabilityModel.setHardwareMode(index, true)
                                            saveDecoderConfig(codecName, "auto")
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // --- Decoder dropdown (shown when >1 real decoder, i.e. "auto" + 2+ decoders) ---
                SettingsRow {
                    rowIndex: index * 3 + 2
                    visible: codecSwitch.checked && decoderList.length > 2

                    Item {
                        anchors.fill: parent
                        anchors.leftMargin: UiMetrics.marginRow * 2

                        RowLayout {
                            anchors.fill: parent
                            anchors.rightMargin: UiMetrics.marginRow
                            spacing: UiMetrics.gap

                            Text {
                                text: "Decoder name"
                                font.pixelSize: UiMetrics.fontBody
                                color: ThemeService.onSurfaceVariant
                                Layout.fillWidth: true
                            }

                            Text {
                                text: selectedDecoder
                                font.pixelSize: UiMetrics.fontBody
                                color: ThemeService.onSurfaceVariant
                                horizontalAlignment: Text.AlignRight
                            }

                            MaterialIcon {
                                icon: "\ue5cf"
                                size: UiMetrics.iconSize
                                color: ThemeService.onSurfaceVariant
                            }
                        }

                        SettingsHoldArea {
                            anchors.fill: parent
                            enableBackHold: false
                            onShortClicked: {
                                decoderPickerDialog.codecIndex = index
                                decoderPickerDialog.codecLabel = codecDisplayName(codecName) + " Decoder"
                                decoderPickerDialog.decoders = decoderList
                                decoderPickerDialog.currentDecoder = selectedDecoder
                                decoderPickerDialog.decoderCodecName = codecName
                                decoderPickerDialog.open()
                            }
                        }
                    }
                }
            }
        }

        SectionHeader { text: "Logging" }

        SettingsRow { rowIndex: 0
            SettingsToggle {
                label: "Verbose Logging"
                configPath: "logging.verbose"
            }
        }

        SectionHeader { text: "Protocol Capture" }

        SettingsRow { rowIndex: 0
            SettingsToggle {
                label: "Enable Capture"
                configPath: "connection.protocol_capture.enabled"
            }
        }

        SettingsRow { rowIndex: 1
            SegmentedButton {
                label: "Format"
                configPath: "connection.protocol_capture.format"
                options: ["JSONL", "TSV"]
                values: ["jsonl", "tsv"]
            }
        }

        SettingsRow { rowIndex: 2
            SettingsToggle {
                label: "Include Media Frames"
                configPath: "connection.protocol_capture.include_media"
            }
        }

        SettingsRow { rowIndex: 3
            ReadOnlyField {
                label: "Capture Path"
                configPath: "connection.protocol_capture.path"
                placeholder: "/tmp/oaa-protocol-capture.jsonl"
            }
        }

        SectionHeader { text: "Connection Info" }

        SettingsRow { rowIndex: 0
            ReadOnlyField {
                label: "TCP Port"
                configPath: "connection.tcp_port"
                placeholder: "5277"
            }
        }

        SectionHeader { text: "WiFi Access Point" }

        SettingsRow { rowIndex: 0
            ReadOnlyField {
                label: "Channel"
                configPath: "connection.wifi_ap.channel"
            }
        }

        SettingsRow { rowIndex: 1
            ReadOnlyField {
                label: "Band"
                configPath: "connection.wifi_ap.band"
            }
        }

        SectionHeader { text: "AA Protocol Test" }

        // Accordion toggle row
        SettingsRow {
            rowIndex: 0
            interactive: true
            onClicked: root.showTestButtons = !root.showTestButtons

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: UiMetrics.marginRow
                anchors.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.gap

                // Status dot: green if connected, gray if not
                Rectangle {
                    width: UiMetrics.iconSmall * 0.5
                    height: width
                    radius: width / 2
                    color: content.aaConnected ? ThemeService.success : ThemeService.onSurfaceVariant
                }

                Text {
                    text: "AA Protocol Test"
                    font.pixelSize: UiMetrics.fontBody
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                }

                MaterialIcon {
                    icon: root.showTestButtons ? "\ue5ce" : "\ue5cf"
                    size: UiMetrics.iconSmall
                    color: ThemeService.onSurfaceVariant
                }
            }
        }

        // Expandable test button grid
        ColumnLayout {
            Layout.fillWidth: true
            spacing: UiMetrics.spacing
            visible: root.showTestButtons

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: UiMetrics.marginRow
                Layout.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.spacing

                ElevatedButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: UiMetrics.rowH
                    text: "Play/Pause"
                    buttonEnabled: content.aaConnected
                    onClicked: ActionRegistry.dispatch("aa.sendButton",85)
                }

                ElevatedButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: UiMetrics.rowH
                    text: "Prev"
                    buttonEnabled: content.aaConnected
                    onClicked: ActionRegistry.dispatch("aa.sendButton",88)
                }

                ElevatedButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: UiMetrics.rowH
                    text: "Next"
                    buttonEnabled: content.aaConnected
                    onClicked: ActionRegistry.dispatch("aa.sendButton",87)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: UiMetrics.marginRow
                Layout.rightMargin: UiMetrics.marginRow
                spacing: UiMetrics.spacing

                ElevatedButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: UiMetrics.rowH
                    text: "Search (84)"
                    buttonEnabled: content.aaConnected
                    onClicked: ActionRegistry.dispatch("aa.sendButton",84)
                }

                ElevatedButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: UiMetrics.rowH
                    text: "Assist (219)"
                    buttonEnabled: content.aaConnected
                    onClicked: ActionRegistry.dispatch("aa.sendButton",219)
                }

                ElevatedButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: UiMetrics.rowH
                    text: "Voice (231)"
                    buttonEnabled: content.aaConnected
                    onClicked: ActionRegistry.dispatch("aa.sendButton",231)
                }
            }
        }
    }

    // Property for accordion toggle (accessible from the SettingsRow)
    property bool showTestButtons: false

    // --- Decoder picker dialog ---
    Dialog {
        id: decoderPickerDialog
        modal: true
        dim: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

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
            color: ThemeService.surface
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
                    color: ThemeService.onSurface
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
                        color: ThemeService.onSurface
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: ThemeService.outlineVariant
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
                            ? ThemeService.primary : ThemeService.onSurface
                        Layout.fillWidth: true
                    }

                    MaterialIcon {
                        icon: "\ue876"
                        size: UiMetrics.iconSize
                        color: ThemeService.primary
                        visible: modelData === decoderPickerDialog.currentDecoder
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.leftMargin: UiMetrics.marginPage
                    anchors.rightMargin: UiMetrics.marginPage
                    height: 1
                    color: ThemeService.outlineVariant
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
            if (CodecCapabilityModel.isEnabled(i)) {
                enabled.push(CodecCapabilityModel.codecName(i))
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
                var name = CodecCapabilityModel.codecName(i)
                if (name === "h264") continue // always enabled
                CodecCapabilityModel.setEnabled(i, enabledList.indexOf(name) >= 0)
            }
        }

        // Load decoder selections from config and restore hw/sw mode
        for (var j = 0; j < CodecCapabilityModel.rowCount(); j++) {
            var cName = CodecCapabilityModel.codecName(j)
            var decoder = ConfigService.value("video.decoder." + cName)
            if (decoder && decoder !== "auto") {
                if (CodecCapabilityModel.isHwDecoder(j, decoder)) {
                    CodecCapabilityModel.setHardwareMode(j, true)
                }
                CodecCapabilityModel.setSelectedDecoder(j, decoder)
            }
        }
    }

    SettingsScrollHints {
        flickable: root
    }
}
