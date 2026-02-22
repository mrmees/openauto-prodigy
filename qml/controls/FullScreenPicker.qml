import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root

    // --- Config-driven mode ---
    property string label: ""
    property string configPath: ""
    property var options: []       // display strings
    property var values: []        // typed values to store (if empty, stores options strings)
    property bool restartRequired: false

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

    // --- Tappable row ---
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginRow
        anchors.rightMargin: UiMetrics.marginRow
        spacing: UiMetrics.gap

        Text {
            text: root.label
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.normalFontColor
            Layout.fillWidth: true
        }

        MaterialIcon {
            icon: "\ue86a"
            size: UiMetrics.iconSmall
            color: ThemeService.descriptionFontColor
            visible: root.restartRequired
        }

        Text {
            text: root._displayText
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
        onClicked: pickerDialog.open()
    }

    // --- Modal bottom-sheet dialog ---
    Dialog {
        id: pickerDialog
        modal: true
        dim: true

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
            color: ThemeService.controlBoxBackgroundColor
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
                    color: ThemeService.normalFontColor
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
                        color: ThemeService.normalFontColor
                    }
                }
            }

            // Bottom separator
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: ThemeService.controlBackgroundColor
            }
        }

        contentItem: ListView {
            id: optionsList
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            model: root._useModel ? root.model : root.options

            delegate: Item {
                width: optionsList.width
                height: UiMetrics.rowH

                readonly property string _itemText: {
                    if (root._useModel && root.textRole !== "") {
                        // model-driven: read textRole from model data
                        var val = model[root.textRole]
                        return val !== undefined ? String(val) : modelData || ""
                    }
                    return modelData || ""
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
                            ? ThemeService.highlightColor
                            : ThemeService.normalFontColor
                        Layout.fillWidth: true
                    }

                    MaterialIcon {
                        icon: "\ue876"
                        size: UiMetrics.iconSize
                        color: ThemeService.highlightColor
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
                    color: ThemeService.controlBackgroundColor
                    visible: index < optionsList.count - 1
                }

                MouseArea {
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
