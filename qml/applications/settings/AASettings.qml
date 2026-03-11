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
        anchors.topMargin: UiMetrics.marginPage
        spacing: 0

        SettingsRow { rowIndex: 0
            FullScreenPicker {
                flat: true
                label: "Resolution"
                configPath: "video.resolution"
                options: ["480p", "720p", "1080p"]
                values: ["480p", "720p", "1080p"]
            }
        }

        SettingsRow { rowIndex: 1
            SettingsSlider {
                label: "DPI"
                configPath: "video.dpi"
                from: 80; to: 400
                stepSize: 10
                restartRequired: true
            }
        }

        SettingsRow { rowIndex: 2
            SettingsToggle {
                label: "Auto-connect"
                configPath: "connection.auto_connect_aa"
            }
        }
    }
}
