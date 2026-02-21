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
    implicitHeight: 48

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 12

        Text {
            text: root.label
            font.pixelSize: 15
            color: ThemeService.normalFontColor
            Layout.preferredWidth: root.width * 0.35
        }

        MaterialIcon {
            icon: "\ue86a"
            size: 16
            color: ThemeService.descriptionFontColor
            visible: root.restartRequired
        }

        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: root.placeholder
            readOnly: root.readOnly
            color: ThemeService.normalFontColor
            font.pixelSize: 14
            background: Rectangle {
                color: ThemeService.controlBackgroundColor
                border.color: field.activeFocus ? ThemeService.highlightColor : ThemeService.controlForegroundColor
                border.width: 1
                radius: 4
            }
            onEditingFinished: {
                if (!root.readOnly) {
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
