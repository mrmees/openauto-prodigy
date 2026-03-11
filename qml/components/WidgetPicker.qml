import QtQuick
import QtQuick.Layouts

Item {
    id: picker
    anchors.fill: parent

    property string targetPaneId: ""
    signal widgetSelected(string widgetId)
    signal cancelled()

    // Dim background
    Rectangle {
        anchors.fill: parent
        color: ThemeService.scrim
        opacity: 0.4

        MouseArea {
            anchors.fill: parent
            onClicked: picker.cancelled()
        }
    }

    // Picker panel
    Rectangle {
        id: panel
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: UiMetrics.marginPage
        height: UiMetrics.tileH * 0.7
        radius: UiMetrics.radiusLarge
        color: ThemeService.surfaceContainerHighest

        // Slide up animation
        y: parent.height
        Component.onCompleted: y = parent.height - height - UiMetrics.marginPage

        Behavior on y {
            NumberAnimation { duration: UiMetrics.animDuration; easing.type: Easing.OutCubic }
        }

        // Widget options
        ListView {
            anchors.fill: parent
            anchors.margins: UiMetrics.spacing
            orientation: ListView.Horizontal
            spacing: UiMetrics.spacing
            clip: true

            model: WidgetPickerModel

            delegate: Rectangle {
                width: UiMetrics.tileW * 0.4
                height: ListView.view.height
                radius: UiMetrics.radiusSmall
                color: optionMa.pressed ? ThemeService.primaryContainer : ThemeService.surfaceContainer

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: UiMetrics.spacing * 0.25

                    MaterialIcon {
                        icon: model.iconName
                        size: UiMetrics.iconSmall
                        color: ThemeService.onSurface
                        Layout.alignment: Qt.AlignHCenter
                    }
                    NormalText {
                        text: model.displayName
                        font.pixelSize: UiMetrics.fontSmall
                        color: ThemeService.onSurface
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                MouseArea {
                    id: optionMa
                    anchors.fill: parent
                    onClicked: picker.widgetSelected(model.widgetId)
                }
            }
        }
    }
}
