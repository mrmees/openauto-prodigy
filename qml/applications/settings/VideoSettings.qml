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
    }
}
