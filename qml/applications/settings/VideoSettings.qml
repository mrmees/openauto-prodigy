import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    property StackView stackRef: StackView.view

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        SettingsPageHeader {
            title: "Video"
            stack: root.stackRef
        }

        SettingsComboBox {
            label: "FPS"
            configPath: "video.fps"
            options: ["30", "60"]
            values: [30, 60]
            restartRequired: true
        }

        SettingsComboBox {
            label: "Resolution"
            configPath: "video.resolution"
            options: ["480p", "720p", "1080p"]
            restartRequired: true
        }

        SettingsSpinBox {
            label: "DPI"
            configPath: "video.dpi"
            from: 80; to: 400
            restartRequired: true
        }

        // Sidebar section header
        Text {
            text: "Sidebar"
            font.pixelSize: UiMetrics.fontTitle
            font.bold: true
            color: ThemeService.normalFontColor
            Layout.topMargin: UiMetrics.sectionGap
        }

        SettingsToggle {
            label: "Show sidebar during Android Auto"
            configPath: "video.sidebar.enabled"
            restartRequired: true
        }

        SettingsComboBox {
            label: "Sidebar Position"
            configPath: "video.sidebar.position"
            options: ["Right", "Left"]
            values: ["right", "left"]
            restartRequired: true
        }

        SettingsSpinBox {
            label: "Sidebar Width (px)"
            configPath: "video.sidebar.width"
            from: 80; to: 300
            restartRequired: true
        }

        Text {
            text: "Sidebar changes take effect on next app restart."
            font.pixelSize: UiMetrics.fontTiny
            color: ThemeService.descriptionFontColor
            font.italic: true
            Layout.leftMargin: UiMetrics.marginRow
        }
    }
}
