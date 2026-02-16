import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: bottomBar

    color: ThemeController.barBackgroundColor

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 12

        // Volume icon
        Text {
            Layout.alignment: Qt.AlignVCenter
            text: volumeSlider.value > 50 ? "\uD83D\uDD0A"
                : volumeSlider.value > 0  ? "\uD83D\uDD09"
                :                           "\uD83D\uDD07"
            font.pixelSize: 22
            color: ThemeController.normalFontColor
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
                color: ThemeController.controlBackgroundColor

                Rectangle {
                    width: volumeSlider.visualPosition * parent.width
                    height: parent.height
                    color: ThemeController.highlightColor
                    radius: 3
                }
            }

            handle: Rectangle {
                x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                implicitWidth: 22
                implicitHeight: 22
                radius: 11
                color: volumeSlider.pressed ? Qt.lighter(ThemeController.highlightColor, 1.2)
                                            : ThemeController.highlightColor
                border.color: ThemeController.controlForegroundColor
                border.width: 1
            }
        }
    }
}
