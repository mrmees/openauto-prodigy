import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property string label: ""
    property string configPath: ""
    property var options: []      // display strings: ["30", "60"] or ["time", "gpio", "none"]
    property var values: []       // typed values to store: [30, 60] — if empty, stores options strings
    property bool restartRequired: false

    // Expose current index so parent pages can drive conditional visibility
    property alias currentIndex: combo.currentIndex
    readonly property var currentValue: {
        if (root.values.length > 0 && combo.currentIndex >= 0 && combo.currentIndex < root.values.length)
            return root.values[combo.currentIndex]
        if (combo.currentIndex >= 0 && combo.currentIndex < root.options.length)
            return root.options[combo.currentIndex]
        return undefined
    }

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

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

        ComboBox {
            id: combo
            model: root.options
            Layout.preferredWidth: root.width * 0.35
            onActivated: {
                if (root.configPath === "") return
                var writeVal = (root.values.length > 0 && currentIndex < root.values.length)
                    ? root.values[currentIndex] : currentText
                ConfigService.setValue(root.configPath, writeVal)
                ConfigService.save()
            }
        }
    }

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
            if (idx >= 0) combo.currentIndex = idx
        }
    }
}
