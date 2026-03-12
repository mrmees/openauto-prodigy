import QtQuick
import QtQuick.Layouts

Item {
    id: navWidget
    clip: true

    // Pixel-based breakpoints for responsive layout
    readonly property bool showRoadName: width >= 400
    readonly property bool isTall: height >= 180
    readonly property bool isWide: width >= 500

    // Data bindings
    property bool navActive: typeof NavigationBridge !== "undefined"
                             && NavigationBridge.navActive
    property bool hasIcon: typeof NavigationBridge !== "undefined"
                           && NavigationBridge.hasManeuverIcon
    property string roadName: typeof NavigationBridge !== "undefined"
                              ? (NavigationBridge.roadName || "") : ""
    property string distance: typeof NavigationBridge !== "undefined"
                              ? (NavigationBridge.formattedDistance || "") : ""
    property int iconVer: typeof NavigationBridge !== "undefined"
                          ? NavigationBridge.iconVersion : 0

    // ---- Active state ----

    // Tall layout (3x2, 4x2): icon + distance top, road name below divider
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.spacing
        spacing: UiMetrics.spacing * 0.5
        visible: navActive && isTall

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: UiMetrics.spacing

            Image {
                source: hasIcon
                        ? "image://navicon/current?" + iconVer
                        : ""
                visible: hasIcon
                sourceSize.width: isWide ? UiMetrics.iconSize * 2.5 : UiMetrics.iconSize * 2
                sourceSize.height: sourceSize.width
                Layout.preferredWidth: sourceSize.width
                Layout.preferredHeight: sourceSize.height
                fillMode: Image.PreserveAspectFit
            }

            MaterialIcon {
                icon: "\ue55c"  // navigation
                size: isWide ? UiMetrics.iconSize * 2.5 : UiMetrics.iconSize * 2
                color: ThemeService.primary
                visible: !hasIcon
            }

            NormalText {
                text: distance
                font.pixelSize: isWide ? UiMetrics.fontHeading * 1.5 : UiMetrics.fontHeading
                font.bold: true
                color: ThemeService.onSurface
                Layout.alignment: Qt.AlignVCenter
            }
        }

        // Divider
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: ThemeService.outlineVariant
            Layout.leftMargin: UiMetrics.spacing
            Layout.rightMargin: UiMetrics.spacing
        }

        NormalText {
            text: roadName || "Navigating"
            font.pixelSize: isWide ? UiMetrics.fontTitle : UiMetrics.fontBody
            color: ThemeService.onSurfaceVariant
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Item { Layout.fillHeight: true }
    }

    // Single-row layout (2x1, 3x1): icon + distance [+ road name]
    RowLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.spacing
        spacing: UiMetrics.spacing
        visible: navActive && !isTall

        Image {
            source: hasIcon
                    ? "image://navicon/current?" + iconVer
                    : ""
            visible: hasIcon
            sourceSize.width: UiMetrics.iconSize * 1.5
            sourceSize.height: sourceSize.width
            Layout.preferredWidth: sourceSize.width
            Layout.preferredHeight: sourceSize.height
            fillMode: Image.PreserveAspectFit
        }

        MaterialIcon {
            icon: "\ue55c"  // navigation
            size: UiMetrics.iconSize * 1.5
            color: ThemeService.primary
            visible: !hasIcon
        }

        NormalText {
            text: distance
            font.pixelSize: UiMetrics.fontTitle
            font.bold: true
            color: ThemeService.onSurface
        }

        // Vertical divider when road name is shown
        Rectangle {
            visible: showRoadName
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            Layout.topMargin: UiMetrics.spacing
            Layout.bottomMargin: UiMetrics.spacing
            color: ThemeService.outlineVariant
        }

        NormalText {
            visible: showRoadName
            text: roadName || "Navigating"
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.onSurfaceVariant
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }

    // ---- Inactive state: muted placeholder ----
    ColumnLayout {
        anchors.centerIn: parent
        spacing: UiMetrics.spacing
        visible: !navActive
        opacity: 0.4

        MaterialIcon {
            icon: "\ue55c"  // navigation
            size: isTall ? UiMetrics.iconSize * 2 : UiMetrics.iconSize * 1.5
            color: ThemeService.onSurfaceVariant
            Layout.alignment: Qt.AlignHCenter
        }

        NormalText {
            text: "No navigation"
            visible: showRoadName || isTall
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.onSurfaceVariant
            Layout.alignment: Qt.AlignHCenter
        }
    }

    // Tap to open AA fullscreen
    MouseArea {
        anchors.fill: parent
        pressAndHoldInterval: 500
        onClicked: {
            Qt.callLater(function() {
                PluginModel.setActivePlugin("org.openauto.android-auto")
            })
        }
        onPressAndHold: {
            if (navWidget.parent && navWidget.parent.requestContextMenu)
                navWidget.parent.requestContextMenu()
        }
    }
}
