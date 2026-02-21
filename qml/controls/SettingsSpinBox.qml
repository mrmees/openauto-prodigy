import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    property string label: ""
    property string configPath: ""
    property int from: 0
    property int to: 99999
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
            icon: "\ue86a"
            size: 16
            color: ThemeService.descriptionFontColor
            visible: root.restartRequired
        }

        SpinBox {
            id: spin
            from: root.from
            to: root.to
            editable: true
            Layout.preferredWidth: 140
            onValueModified: {
                if (root.configPath === "") return
                ConfigService.setValue(root.configPath, value)
                ConfigService.save()
            }
        }
    }

    Component.onCompleted: {
        if (root.configPath !== "") {
            var v = ConfigService.value(root.configPath)
            if (v !== undefined && v !== null)
                spin.value = Number(v)
        }
    }
}
