import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string label: ""
    property string configPath: ""
    property string value: ""
    property string placeholder: "\u2014"
    property bool showWebHint: true

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

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: root.value !== "" ? root.value : root.placeholder
                font.pixelSize: UiMetrics.fontBody
                color: root.value !== "" ? ThemeService.normalFontColor : ThemeService.descriptionFontColor
                elide: Text.ElideRight
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
            }

            Text {
                text: "Edit via web panel"
                font.pixelSize: UiMetrics.fontTiny
                font.italic: true
                color: ThemeService.descriptionFontColor
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                visible: root.showWebHint
            }
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
