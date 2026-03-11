import QtQuick

MouseArea {
    id: root

    signal shortClicked()

    property bool holdTriggered: false

    pressAndHoldInterval: 500
    preventStealing: true

    onPressed: holdTriggered = false
    onCanceled: holdTriggered = false
    onPressAndHold: {
        holdTriggered = true
        ApplicationController.requestBack()
    }
    onClicked: {
        if (holdTriggered) {
            holdTriggered = false
            return
        }
        shortClicked()
    }
}
