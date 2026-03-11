import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    readonly property bool blocksBackHold: true
    property string icon: ""
    property string label: ""
    property string configPath: ""
    property bool restartRequired: false

    Layout.fillWidth: true
    implicitHeight: UiMetrics.rowH

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: UiMetrics.marginRow
        anchors.rightMargin: UiMetrics.marginRow
        spacing: UiMetrics.gap

        MaterialIcon {
            icon: root.icon
            size: UiMetrics.iconSize
            color: ThemeService.onSurface
            visible: root.icon !== ""
            Layout.preferredWidth: UiMetrics.iconSize
        }

        Text {
            text: root.label
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.onSurface
            Layout.fillWidth: true
        }

        MaterialIcon {
            icon: "\ue86a"  // restart_alt
            size: UiMetrics.iconSmall
            color: ThemeService.onSurfaceVariant
            visible: root.restartRequired
        }

        Switch {
            id: toggle
            onToggled: {
                if (root.configPath === "") return
                ConfigService.setValue(root.configPath, checked)
                ConfigService.save()
            }
        }
    }

    SettingsHoldArea {
        anchors.fill: parent
        enableBackHold: false
        onShortClicked: toggle.checked = !toggle.checked
    }

    Component.onCompleted: {
        if (root.configPath !== "") {
            var v = ConfigService.value(root.configPath)
            toggle.checked = (v === true || v === 1 || v === "true")
        }
    }
}
