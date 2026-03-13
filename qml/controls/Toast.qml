import QtQuick

Rectangle {
    id: toast

    property string message: ""

    function show(msg, durationMs) {
        message = msg
        opacity = 1.0
        hideTimer.interval = durationMs !== undefined ? durationMs : 2500
        hideTimer.restart()
    }

    anchors.bottom: parent ? parent.bottom : undefined
    anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
    anchors.bottomMargin: UiMetrics.gap * 2

    width: toastText.implicitWidth + UiMetrics.spacing * 2
    height: toastText.implicitHeight + UiMetrics.spacing

    radius: UiMetrics.radius
    color: ThemeService.inverseSurface
    opacity: 0
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation { duration: 200 }
    }

    NormalText {
        id: toastText
        text: toast.message
        color: ThemeService.inverseOnSurface
        anchors.centerIn: parent
    }

    Timer {
        id: hideTimer
        interval: 2500
        onTriggered: toast.opacity = 0
    }
}
