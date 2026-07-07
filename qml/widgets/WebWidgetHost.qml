import QtQuick
import QtWebEngine

// Hosts one web widget package (design 2026-07-06-js-runtime §5).
// Lazy: the WebEngineView instantiates on first page visibility and stays
// alive afterwards (D4). Crash recovery per D5. Locked-down settings +
// same-origin navigation (§5, §7).
Item {
    id: hostRoot

    // Marker for HomeMenu.qml's delegate: identifies this Loader's item as a
    // web widget host (web widgets need their own edit-mode entry -- the
    // WebEngineView eats every touch, so the z:-1 long-press detector never
    // fires for them; native widgets must NOT get this behavior).
    readonly property bool isWebWidgetHost: true

    // Emitted when the injected host-gestures.js detects a >=500ms hold that
    // ends with the finger LIFTING in place (in-page detection + sentinel
    // navigation). Deliberately fires only after pointer release: any Qt
    // pointer handler over the view SIGSEGVs the UI process when the scene
    // mutates mid-touch-stream (on-device 2026-07-07, Codex-diagnosed).
    signal longPressed()

    property QtObject widgetContext: null
    readonly property var effectiveCfg: widgetContext ? widgetContext.effectiveConfig : ({})
    readonly property string widgetUrl: effectiveCfg && effectiveCfg.url ? effectiveCfg.url : ""
    property bool everVisible: false
    property int retryCount: 0

    function maybeActivate() {
        if (widgetContext && widgetContext.isCurrentPage)
            everVisible = true
    }
    Component.onCompleted: maybeActivate()
    onWidgetContextChanged: maybeActivate()
    Connections {
        target: hostRoot.widgetContext
        function onIsCurrentPageChanged() { hostRoot.maybeActivate() }
        function onColSpanChanged() { hostRoot.pushContext() }
        function onRowSpanChanged() { hostRoot.pushContext() }
    }

    function contextObject() {
        return {
            instanceId: widgetContext ? widgetContext.instanceId : "",
            widgetId: widgetContext ? widgetContext.widgetId : "",
            colSpan: widgetContext ? widgetContext.colSpan : 1,
            rowSpan: widgetContext ? widgetContext.rowSpan : 1,
            kind: "widget"
        }
    }
    function bootstrapSource() {
        var boot = {
            apiUrl: "ws://127.0.0.1:" + ConfigService.value("api.ws_port"),
            context: contextObject(),
            themeTokens: ThemeService.themeTokenMap()
        }
        return "window.__prodigyBootstrap = " + JSON.stringify(boot) + ";"
    }
    function pushContext() {
        if (viewLoader.item)
            viewLoader.item.runJavaScript(
                "window.prodigy && prodigy._updateContext("
                + JSON.stringify(contextObject()) + ")")
    }

    Loader {
        id: viewLoader
        anchors.fill: parent
        active: hostRoot.everVisible && hostRoot.widgetUrl !== ""
        sourceComponent: WebEngineView {
            backgroundColor: "transparent"
            settings.javascriptCanOpenWindows: false
            settings.localContentCanAccessFileUrls: false
            settings.localContentCanAccessRemoteUrls: true   // https subresources OK (§5)
            settings.fullScreenSupportEnabled: false          // §5: fullscreen requests denied

            Component.onCompleted: {
                var bs = WebEngine.script()
                bs.name = "prodigy-bootstrap"
                bs.injectionPoint = WebEngineScript.DocumentCreation
                bs.worldId = WebEngineScript.MainWorld
                bs.sourceCode = hostRoot.bootstrapSource()

                var rt = WebEngine.script()
                rt.name = "protobuf-runtime"
                rt.injectionPoint = WebEngineScript.DocumentCreation
                rt.worldId = WebEngineScript.MainWorld
                rt.sourceUrl = "qrc:/web/protobuf.min.js"

                var gen = WebEngine.script()
                gen.name = "prodigy-proto"
                gen.injectionPoint = WebEngineScript.DocumentCreation
                gen.worldId = WebEngineScript.MainWorld
                gen.sourceUrl = "qrc:/web/prodigy-proto.js"

                var shim = WebEngine.script()
                shim.name = "prodigy-shim"
                shim.injectionPoint = WebEngineScript.DocumentCreation
                shim.worldId = WebEngineScript.MainWorld
                shim.sourceUrl = "qrc:/web/prodigy.js"

                var gestures = WebEngine.script()
                gestures.name = "host-gestures"
                gestures.injectionPoint = WebEngineScript.DocumentCreation
                gestures.worldId = WebEngineScript.MainWorld
                gestures.sourceUrl = "qrc:/web/host-gestures.js"

                userScripts.collection = [bs, rt, gen, shim, gestures]
                url = hostRoot.widgetUrl
            }

            onRenderProcessTerminated: function (terminationStatus, exitCode) {
                if (hostRoot.retryCount >= 3) {        // D5: 3 attempts then error card
                    errorCard.visible = true
                    return
                }
                hostRoot.retryCount += 1
                reloadTimer.interval = 1000 * Math.pow(2, hostRoot.retryCount) // 2s/4s/8s
                reloadTimer.start()
            }
            onLoadingChanged: function (loadingInfo) {
                if (loadingInfo.status === WebEngineView.LoadSucceededStatus) {
                    hostRoot.retryCount = 0
                    errorCard.visible = false
                }
            }
            onNavigationRequested: function (request) {
                // Sentinel from host-gestures.js: long-press completed (finger
                // lifted). Never navigates -- ignore + signal the host.
                if (request.url.toString().indexOf("prodigy://host/longpress") === 0) {
                    request.action = WebEngineNavigationRequest.IgnoreRequest
                    hostRoot.longPressed()
                    return
                }
                // Same-origin top-level navigation only (§5).
                if (request.url.toString().indexOf("prodigy://widgets/") !== 0)
                    request.action = WebEngineNavigationRequest.IgnoreRequest
            }
            onTouchSelectionMenuRequested: function (request) {
                // Suppress Chromium's touch-selection menu on long-press
                // (Codex hardening: it races the host's long-press path).
                request.accepted = true
            }
        }
    }

    Timer {
        id: reloadTimer
        repeat: false
        onTriggered: if (viewLoader.item) viewLoader.item.reload()
    }

    Rectangle {
        id: errorCard
        anchors.fill: parent
        visible: false
        radius: 8
        color: ThemeService.surfaceContainerHigh

        Column {
            anchors.centerIn: parent
            spacing: 8
            Text {
                text: hostRoot.widgetContext ? hostRoot.widgetContext.widgetId : "Web widget"
                color: ThemeService.onSurface
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: qsTr("Failed to load — tap to retry")
                color: ThemeService.onSurfaceVariant
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                hostRoot.retryCount = 0
                errorCard.visible = false
                if (viewLoader.item) viewLoader.item.reload()
            }
        }
    }
}
