import QtQuick
import QtQuick.Layouts

/// Toast notification area that appears at the top of the screen.
/// Driven by NotificationModel — shows "toast" kind notifications
/// with auto-dismiss animation.
Item {
    id: notificationArea
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.top: parent.top
    anchors.topMargin: UiMetrics.spacing
    height: notificationColumn.implicitHeight
    z: 998  // below GestureOverlay (999), above everything else

    ColumnLayout {
        id: notificationColumn
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width * 0.6, 480)
        spacing: UiMetrics.spacing

        Repeater {
            model: NotificationModel

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: toastLayout.implicitHeight + UiMetrics.gap
                radius: UiMetrics.radiusSmall
                color: "#DD2a2a3e"
                border.color: "#0f3460"
                border.width: 1
                opacity: 0
                visible: model.kind === "toast"

                Component.onCompleted: opacity = 1

                Behavior on opacity {
                    NumberAnimation { duration: 200 }
                }

                RowLayout {
                    id: toastLayout
                    anchors.fill: parent
                    anchors.margins: UiMetrics.spacing
                    spacing: UiMetrics.spacing

                    MaterialIcon {
                        icon: "\ue88e"  // info
                        size: UiMetrics.iconSmall
                        color: "#e0e0e0"
                    }

                    Text {
                        Layout.fillWidth: true
                        text: model.message
                        font.pixelSize: UiMetrics.fontTiny
                        color: "#e0e0e0"
                        wrapMode: Text.WordWrap
                    }

                    MaterialIcon {
                        icon: "\ue5cd"  // close
                        size: UiMetrics.iconSmall
                        color: "#808080"

                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -4
                            onClicked: NotificationService.dismiss(model.notificationId)
                        }
                    }
                }
            }
        }
    }
}
