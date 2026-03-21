import QtQuick
import QtQuick.Layouts

Item {
    id: dateWidget
    clip: true

    // Widget contract: context injection from host
    property QtObject widgetContext: null

    // Span-based breakpoints for responsive layout
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1

    // Load-state gating
    readonly property bool isCurrentPage: widgetContext ? widgetContext.isCurrentPage : true

    // Declarative binding to effectiveConfig
    readonly property var currentEffectiveConfig: widgetContext ? widgetContext.effectiveConfig : ({})
    readonly property string dateOrder: currentEffectiveConfig.dateOrder || "us"

    // Date parts updated by timer
    property int dayOfMonth: 1
    property string dayOfWeekShort: ""
    property string dayOfWeekLong: ""
    property string monthShort: ""
    property string monthLong: ""

    function ordinalSuffix(n) {
        var s = ["th", "st", "nd", "rd"];
        var v = n % 100;
        return n + (s[(v - 20) % 10] || s[v] || s[0]);
    }

    function formattedDate() {
        var ord = ordinalSuffix(dayOfMonth)

        if (colSpan >= 4) {
            // Full detail with long month
            if (dateOrder === "intl")
                return dayOfWeekLong + ", " + ord + " " + monthLong
            return dayOfWeekLong + ", " + monthLong + " " + ord
        }
        if (colSpan >= 3) {
            // Medium with short month
            if (dateOrder === "intl")
                return dayOfWeekLong + ", " + ord + " " + monthShort
            return dayOfWeekLong + ", " + monthShort + " " + ord
        }
        if (colSpan >= 2) {
            // Short: day-of-week abbrev + ordinal
            return dayOfWeekShort + " " + ord
        }
        // 1x1 hero: just the ordinal number
        return ord
    }

    Timer {
        interval: 60000
        running: dateWidget.isCurrentPage
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            var now = new Date()
            dateWidget.dayOfMonth = now.getDate()
            dateWidget.dayOfWeekShort = now.toLocaleDateString(Qt.locale(), "ddd")
            dateWidget.dayOfWeekLong = now.toLocaleDateString(Qt.locale(), "dddd")
            dateWidget.monthShort = now.toLocaleDateString(Qt.locale(), "MMM")
            dateWidget.monthLong = now.toLocaleDateString(Qt.locale(), "MMMM")
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: UiMetrics.spacing

        NormalText {
            anchors.fill: parent
            text: dateWidget.formattedDate()
            font.pixelSize: {
                var base
                if (colSpan >= 3)
                    base = UiMetrics.fontHeading * 1.5
                else if (colSpan >= 2)
                    base = UiMetrics.fontHeading * 2.0
                else
                    base = UiMetrics.fontHeading * 3.0

                return rowSpan >= 2 ? base * 1.5 : base
            }
            font.weight: Font.Bold
            fontSizeMode: Text.Fit
            minimumPixelSize: UiMetrics.fontHeading
            color: ThemeService.onSurface
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // pressAndHold forwarding for host context menu
    MouseArea {
        anchors.fill: parent
        z: -1
        pressAndHoldInterval: 500
        onPressAndHold: {
            if (dateWidget.parent && dateWidget.parent.requestContextMenu)
                dateWidget.parent.requestContextMenu()
        }
    }
}
