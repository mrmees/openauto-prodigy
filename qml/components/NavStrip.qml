import QtQuick
import QtQuick.Layouts

Rectangle {
    id: navStrip
    color: ThemeService.barBackgroundColor

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        Repeater {
            model: PluginModel

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: navStrip.height * 1.2
                color: isActive ? ThemeService.highlightColor : "transparent"
                radius: 8

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 2

                    MaterialIcon {
                        Layout.alignment: Qt.AlignHCenter
                        icon: pluginIcon || "\ue5c3"
                        size: 20
                        color: isActive ? ThemeService.highlightFontColor
                                        : ThemeService.normalFontColor
                    }

                    NormalText {
                        Layout.alignment: Qt.AlignHCenter
                        text: pluginName
                        font.pixelSize: 10
                        color: isActive ? ThemeService.highlightFontColor
                                        : ThemeService.normalFontColor
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: PluginModel.setActivePlugin(pluginId)
                }
            }
        }
    }
}
