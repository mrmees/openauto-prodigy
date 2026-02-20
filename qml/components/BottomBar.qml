import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: bottomBar

    color: ThemeService.barBackgroundColor

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 12

        // Volume icon
        MaterialIcon {
            Layout.alignment: Qt.AlignVCenter
            icon: volumeSlider.value > 50 ? "\ue050"   // volume_up
                : volumeSlider.value > 0  ? "\ue04d"   // volume_down
                :                           "\ue04f"   // volume_off
            color: ThemeService.normalFontColor
            size: 24
        }

        Slider {
            id: volumeSlider
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            from: 0
            to: 100
            value: 75
            stepSize: 1

            background: Rectangle {
                x: volumeSlider.leftPadding
                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                implicitWidth: 200
                implicitHeight: 6
                width: volumeSlider.availableWidth
                height: implicitHeight
                radius: 3
                color: ThemeService.controlBackgroundColor

                Rectangle {
                    width: volumeSlider.visualPosition * parent.width
                    height: parent.height
                    color: ThemeService.highlightColor
                    radius: 3
                }
            }

            handle: Rectangle {
                x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                implicitWidth: 22
                implicitHeight: 22
                radius: 11
                color: volumeSlider.pressed ? Qt.lighter(ThemeService.highlightColor, 1.2)
                                            : ThemeService.highlightColor
                border.color: ThemeService.controlForegroundColor
                border.width: 1
            }
        }
    }
}
