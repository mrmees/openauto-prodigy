import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    readonly property bool blocksBackHold: true

    // --- Config-driven mode ---
    property string label: ""
    property string configPath: ""
    property var options: []       // display strings
    property var values: []        // typed values to store (if empty, stores options strings)
    property bool restartRequired: false
    property bool flat: false

    // --- Model-driven mode ---
    property var model: null       // QAbstractItemModel or JS array
    property string textRole: ""

    // --- Output ---
    property int currentIndex: -1
    readonly property var currentValue: {
        if (root.values.length > 0 && root.currentIndex >= 0 && root.currentIndex < root.values.length)
            return root.values[root.currentIndex]
        if (root.options.length > 0 && root.currentIndex >= 0 && root.currentIndex < root.options.length)
            return root.options[root.currentIndex]
        return undefined
    }

    signal activated(int index)

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    // --- Whether we're in model-driven mode ---
    readonly property bool _useModel: root.model !== null && root.options.length === 0

    // --- Resolved display text for current selection ---
    readonly property string _displayText: {
        if (root.currentIndex < 0) return ""
        if (root._useModel) {
            // Read from model via textRole
            if (root.model && root.textRole !== "" && typeof root.model.data === "function") {
                // QAbstractItemModel path
                var roleEnum = -1
                var roleNames = root.model.roleNames ? root.model.roleNames() : null
                if (roleNames) {
                    for (var r in roleNames) {
                        if (roleNames[r] === root.textRole) { roleEnum = parseInt(r); break }
                    }
                }
                if (roleEnum >= 0 && root.model.index) {
                    var idx = root.model.index(root.currentIndex, 0)
                    return root.model.data(idx, roleEnum) || ""
                }
            }
            // JS array fallback
            if (Array.isArray(root.model) && root.currentIndex < root.model.length)
                return String(root.model[root.currentIndex])
            return ""
        }
        if (root.currentIndex < root.options.length)
            return root.options[root.currentIndex]
        return ""
    }

    readonly property bool _isPressed: rowMouseArea.pressed

    scale: (!root.flat && _isPressed) ? 0.97 : 1.0
    Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

    // Background rectangle (source for MultiEffect shadow)
    Rectangle {
        id: rowBg
        anchors.fill: parent
        radius: UiMetrics.radiusSmall
        color: ThemeService.surfaceContainerLow
        border.width: 1
        border.color: ThemeService.outlineVariant
        layer.enabled: !root.flat
        visible: false
    }

    // Shadow effect (Level 2 resting, reduced on press)
    MultiEffect {
        source: rowBg
        anchors.fill: rowBg
        visible: !root.flat
        shadowEnabled: !root.flat
        shadowColor: ThemeService.shadow
        shadowBlur: root._isPressed ? 0.35 : 0.65
        shadowVerticalOffset: root._isPressed ? 2 : 5
        shadowOpacity: root._isPressed ? 0.30 : 0.55
        shadowHorizontalOffset: 0
        shadowScale: 1.0
        autoPaddingEnabled: true

        Behavior on shadowBlur { NumberAnimation { duration: UiMetrics.animDurationFast } }
        Behavior on shadowVerticalOffset { NumberAnimation { duration: UiMetrics.animDurationFast } }
        Behavior on shadowOpacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
    }

    // State layer overlay
    Rectangle {
        anchors.fill: parent
        radius: UiMetrics.radiusSmall
        color: ThemeService.onSurface
        opacity: (!root.flat && root._isPressed) ? 0.10 : 0.0
        visible: !root.flat
        Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
    }

    // --- Tappable row ---
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginRow
        anchors.rightMargin: UiMetrics.marginRow
        spacing: UiMetrics.gap

        Text {
            text: root.label
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.onSurface
            Layout.fillWidth: true
        }

        MaterialIcon {
            icon: "\ue86a"
            size: UiMetrics.iconSmall
            color: ThemeService.onSurfaceVariant
            visible: root.restartRequired
        }

        Text {
            text: root._displayText
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
        id: rowMouseArea
        anchors.fill: parent
        onShortClicked: pickerDialog.open()
    }

    // --- Modal bottom-sheet dialog ---
    Dialog {
        id: pickerDialog
        modal: true
        dim: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        // Position at bottom of window, full width, ~60% height
        parent: Overlay.overlay
        x: 0
        y: parent ? parent.height * 0.4 : 0
        width: parent ? parent.width : 0
        height: parent ? parent.height * 0.6 : 0

        padding: 0
        topPadding: 0
        bottomPadding: 0

        background: Rectangle {
            color: ThemeService.surface
            radius: UiMetrics.radius
            // Only round top corners
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
                    text: root.label
                    font.pixelSize: UiMetrics.fontTitle
                    font.bold: true
                    color: ThemeService.onSurface
                    Layout.fillWidth: true
                }

                MouseArea {
                    Layout.preferredWidth: UiMetrics.backBtnSize
                    Layout.preferredHeight: UiMetrics.backBtnSize
                    onClicked: pickerDialog.close()

                    MaterialIcon {
                        anchors.centerIn: parent
                        icon: "\ue5cd"
                        size: UiMetrics.iconSize
                        color: ThemeService.onSurface
                    }
                }
            }

            // Bottom separator
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: ThemeService.outlineVariant
            }
        }

        contentItem: ListView {
            id: optionsList
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            model: root._useModel ? root.model : root.options

            delegate: Item {
                id: delegateItem
                width: optionsList.width
                height: UiMetrics.rowH

                readonly property bool _isPressed: delegateMouseArea.pressed

                scale: _isPressed ? 0.97 : 1.0
                Behavior on scale { NumberAnimation { duration: UiMetrics.animDurationFast; easing.type: Easing.OutCubic } }

                readonly property string _itemText: {
                    if (root._useModel && root.textRole !== "") {
                        // model-driven: read textRole from model data
                        var val = model[root.textRole]
                        return val !== undefined ? String(val) : modelData || ""
                    }
                    return modelData || ""
                }

                // Background source for shadow
                Rectangle {
                    id: delegateBg
                    anchors.fill: parent
                    anchors.leftMargin: UiMetrics.marginPage
                    anchors.rightMargin: UiMetrics.marginPage
                    radius: UiMetrics.radiusSmall
                    color: ThemeService.surfaceContainerLow
                    layer.enabled: true
                    visible: false
                }

                // Shadow effect (Level 2)
                MultiEffect {
                    source: delegateBg
                    anchors.fill: delegateBg
                    shadowEnabled: true
                    shadowColor: ThemeService.shadow
                    shadowBlur: delegateItem._isPressed ? 0.35 : 0.65
                    shadowVerticalOffset: delegateItem._isPressed ? 2 : 5
                    shadowOpacity: delegateItem._isPressed ? 0.30 : 0.55
                    shadowHorizontalOffset: 0
                    shadowScale: 1.0
                    autoPaddingEnabled: true

                    Behavior on shadowBlur { NumberAnimation { duration: UiMetrics.animDurationFast } }
                    Behavior on shadowVerticalOffset { NumberAnimation { duration: UiMetrics.animDurationFast } }
                    Behavior on shadowOpacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
                }

                // State layer overlay
                Rectangle {
                    anchors.fill: delegateBg
                    radius: UiMetrics.radiusSmall
                    color: ThemeService.onSurface
                    opacity: delegateItem._isPressed ? 0.10 : 0.0
                    Behavior on opacity { NumberAnimation { duration: UiMetrics.animDurationFast } }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: UiMetrics.marginPage
                    anchors.rightMargin: UiMetrics.marginPage
                    spacing: UiMetrics.gap

                    Text {
                        text: _itemText
                        font.pixelSize: UiMetrics.fontBody
                        color: index === root.currentIndex
                            ? ThemeService.primary
                            : ThemeService.onSurface
                        Layout.fillWidth: true
                    }

                    MaterialIcon {
                        icon: "\ue876"
                        size: UiMetrics.iconSize
                        color: ThemeService.primary
                        visible: index === root.currentIndex
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: UiMetrics.marginPage
                    anchors.rightMargin: UiMetrics.marginPage
                    height: 1
                    color: ThemeService.outlineVariant
                    visible: index < optionsList.count - 1
                }

                MouseArea {
                    id: delegateMouseArea
                    anchors.fill: parent
                    onClicked: {
                        root.currentIndex = index

                        // Config-driven: persist to config
                        if (root.configPath !== "") {
                            var writeVal = (root.values.length > 0 && index < root.values.length)
                                ? root.values[index] : _itemText
                            ConfigService.setValue(root.configPath, writeVal)
                            ConfigService.save()
                        }

                        // Model-driven: emit signal
                        root.activated(index)

                        pickerDialog.close()
                    }
                }
            }
        }
    }

    // --- Initialize from config on load ---
    Component.onCompleted: {
        if (root.configPath !== "") {
            var current = ConfigService.value(root.configPath)
            var idx = -1
            if (root.values.length > 0) {
                for (var i = 0; i < root.values.length; i++) {
                    if (root.values[i] == current) { idx = i; break }
                }
            }
            if (idx < 0)
                idx = root.options.indexOf(String(current))
            if (idx >= 0) root.currentIndex = idx
        }
    }
}
