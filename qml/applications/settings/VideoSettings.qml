import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Flickable {
    id: root
    contentHeight: content.implicitHeight + UiMetrics.marginPage * 2
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        spacing: UiMetrics.spacing

        SectionHeader { text: "Playback" }

        SegmentedButton {
            label: "FPS"
            configPath: "video.fps"
            options: ["30", "60"]
            values: [30, 60]
            restartRequired: true
        }

        FullScreenPicker {
            label: "Resolution"
            configPath: "video.resolution"
            options: ["480p", "720p", "1080p"]
            values: ["480p", "720p", "1080p"]
            restartRequired: true
        }

        SettingsSlider {
            label: "DPI"
            configPath: "video.dpi"
            from: 80; to: 400
            stepSize: 10
            restartRequired: true
        }

        SectionHeader { text: "Sidebar" }

        SettingsToggle {
            label: "Show sidebar during Android Auto"
            configPath: "video.sidebar.enabled"
            restartRequired: true
        }

        SegmentedButton {
            label: "Position"
            configPath: "video.sidebar.position"
            options: ["Left", "Right"]
            values: ["left", "right"]
            restartRequired: true
        }

        SettingsSlider {
            label: "Sidebar Width (px)"
            configPath: "video.sidebar.width"
            from: 80; to: 300
            stepSize: 10
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
