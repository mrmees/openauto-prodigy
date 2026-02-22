import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: sidebar
    color: Qt.rgba(0, 0, 0, 0.85)

    property string position: "right"

    // Subtle border on the AA-facing edge
    Rectangle {
        width: 1
        height: parent.height
        color: ThemeService.borderColor
        anchors.left: position === "right" ? parent.left : undefined
        anchors.right: position === "left" ? parent.right : undefined
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 12

        // Volume up button
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 48

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue050"
                size: 28
                color: ThemeService.specialFontColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    var vol = Math.min(100, AudioService.masterVolume + 5)
                    AudioService.setMasterVolume(vol)
                    ConfigService.setValue("audio.master_volume", vol)
                    ConfigService.save()
                }
            }
        }

        // Volume slider (vertical)
        Slider {
            id: volumeSlider
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical
            from: 0; to: 100
            value: AudioService.masterVolume
            onMoved: {
                AudioService.setMasterVolume(value)
                ConfigService.setValue("audio.master_volume", value)
                ConfigService.save()
            }

            background: Rectangle {
                x: volumeSlider.leftPadding + (volumeSlider.availableWidth - width) / 2
                y: volumeSlider.topPadding
                width: 4
                height: volumeSlider.availableHeight
                radius: 2
                color: ThemeService.borderColor

                Rectangle {
                    width: parent.width
                    height: volumeSlider.visualPosition * parent.height
                    radius: 2
                    color: ThemeService.specialFontColor
                }
            }

            handle: Rectangle {
                x: volumeSlider.leftPadding + (volumeSlider.availableWidth - width) / 2
                y: volumeSlider.topPadding + volumeSlider.visualPosition * (volumeSlider.availableHeight - height)
                width: 20; height: 20
                radius: 10
                color: volumeSlider.pressed ? ThemeService.specialFontColor : ThemeService.normalFontColor
            }
        }

        // Volume down button
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 48

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue04d"
                size: 28
                color: ThemeService.specialFontColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    var vol = Math.max(0, AudioService.masterVolume - 5)
                    AudioService.setMasterVolume(vol)
                    ConfigService.setValue("audio.master_volume", vol)
                    ConfigService.save()
                }
            }
        }

        // Separator
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: ThemeService.borderColor
        }

        // Home button
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 56

            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue9b2"
                size: 32
                color: ThemeService.specialFontColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: ApplicationController.goHome()
            }
        }
    }
}
