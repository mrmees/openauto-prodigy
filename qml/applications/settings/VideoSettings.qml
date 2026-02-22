import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + 32
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    property StackView stackRef: StackView.view

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        spacing: 4

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
            font.pixelSize: 17
            font.bold: true
            color: ThemeService.normalFontColor
            Layout.topMargin: 16
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
            font.pixelSize: 12
            color: ThemeService.descriptionFontColor
            font.italic: true
            Layout.leftMargin: 8
        }
    }
}
