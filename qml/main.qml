import QtQuick
import QtQuick.Window

Window {
    id: root
    width: 1024
    height: 600
    visible: true
    title: "OpenAuto Prodigy"
    color: "#1a1a2e"

    Text {
        anchors.centerIn: parent
        text: "OpenAuto Prodigy v0.1.0"
        color: "#e94560"
        font.pixelSize: 32
    }
}
