import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property string label: ""
    property string configPath: ""
    property string placeholder: ""
    property bool restartRequired: false
    property bool readOnly: false

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
            Layout.preferredWidth: root.width * 0.35
        }

        MaterialIcon {
            icon: "\ue86a"
            size: UiMetrics.iconSmall
            color: ThemeService.descriptionFontColor
            visible: root.restartRequired
        }

        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: root.placeholder
            readOnly: root.readOnly
            color: ThemeService.normalFontColor
            font.pixelSize: UiMetrics.fontSmall
            background: Rectangle {
                color: ThemeService.controlBackgroundColor
                border.color: field.activeFocus ? ThemeService.highlightColor : ThemeService.controlForegroundColor
                border.width: 1
                radius: UiMetrics.radius / 2
            }
            onEditingFinished: {
                if (!root.readOnly && root.configPath !== "") {
                    ConfigService.setValue(root.configPath, text)
                    ConfigService.save()
                }
            }
        }
    }

    Component.onCompleted: {
        if (root.configPath !== "") {
            var v = ConfigService.value(root.configPath)
            if (v !== undefined && v !== null)
                field.text = String(v)
        }
    }
}
