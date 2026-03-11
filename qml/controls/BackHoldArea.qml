import QtQuick

// Background long-press detector for settings pages.
// Place as a child of a Flickable/Item — sits behind content at z:-1.
// Long-press (500ms) on non-control areas triggers back navigation.
MouseArea {
    anchors.fill: parent
    z: -1
    pressAndHoldInterval: 500
    preventStealing: false
    onPressAndHold: ApplicationController.requestBack()
}
