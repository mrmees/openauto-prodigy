import QtQuick
import QtWebEngine

Window {
    id: win
    required property string targetUrl
    width: 1024
    height: 600
    visible: true
    title: "EME probe"

    WebEngineView {
        anchors.fill: parent
        url: win.targetUrl
        onJavaScriptConsoleMessage: function (level, message, lineNumber, sourceId) {
            console.log("[page]", message)
        }
    }
}
