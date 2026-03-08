import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string icon: ""
    property string label: ""
    property string configPath: ""
    property string value: ""
    property string placeholder: "\u2014"

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
            color: ThemeService.textPrimary
            visible: root.icon !== ""
            Layout.preferredWidth: UiMetrics.iconSize
        }

        Text {
            text: root.label
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.textPrimary
            Layout.preferredWidth: root.width * 0.35
        }

        Text {
            text: root.value !== "" ? root.value : root.placeholder
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.textSecondary
            elide: Text.ElideRight
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignRight
        }
    }

    Component.onCompleted: {
        if (root.configPath !== "" && root.value === "") {
            var v = ConfigService.value(root.configPath)
            if (v !== undefined && v !== null)
                root.value = v
        }
    }
}
