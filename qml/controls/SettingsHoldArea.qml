import QtQuick

MouseArea {
    id: root

    signal shortClicked()

    preventStealing: true

    onClicked: shortClicked()
}
