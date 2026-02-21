import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property string label: ""
    property string configPath: ""
    property bool restartRequired: false

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
            Layout.fillWidth: true
        }

        MaterialIcon {
            icon: "\ue86a"  // restart_alt
            size: 16
            color: ThemeService.descriptionFontColor
            visible: root.restartRequired
        }

        Switch {
            id: toggle
            checked: ConfigService.value(root.configPath) === true
            onToggled: {
                ConfigService.setValue(root.configPath, checked)
                ConfigService.save()
            }
        }
    }

    Component.onCompleted: {
        if (root.configPath !== "")
            toggle.checked = ConfigService.value(root.configPath) === true
    }
}
