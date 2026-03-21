import QtQuick
import QtQuick.Layouts

Item {
    id: themeCycleWidget
    clip: true

    property QtObject widgetContext: null
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1

    // Derive current theme name from parallel lists
    readonly property int themeIndex: ThemeService.availableThemes.indexOf(ThemeService.currentThemeId)
    readonly property string themeName: themeIndex >= 0
        ? ThemeService.availableThemeNames[themeIndex]
        : ThemeService.currentThemeId

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: UiMetrics.spacing
        spacing: UiMetrics.spacing * 0.5

        MaterialIcon {
            icon: "\ue40a"  // palette
            size: Math.min(parent.width, parent.height) * 0.55
            color: ThemeService.primary  // live preview of active theme
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
        }

        NormalText {
            text: themeCycleWidget.themeName
            font.pixelSize: Math.min(parent.width, parent.height) * 0.2
            font.weight: Font.Bold
            fontSizeMode: Text.Fit
            minimumPixelSize: UiMetrics.fontSmall
            color: ThemeService.onSurface
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }

    MouseArea {
        anchors.fill: parent
        z: -1
        pressAndHoldInterval: 500
        onClicked: {
            // Advance to next theme, wrapping around
            var themes = ThemeService.availableThemes
            var idx = themes.indexOf(ThemeService.currentThemeId)
            var nextIdx = (idx + 1) % themes.length
            ThemeService.setTheme(themes[nextIdx])
        }
        onPressAndHold: {
            if (themeCycleWidget.parent && themeCycleWidget.parent.requestContextMenu)
                themeCycleWidget.parent.requestContextMenu()
        }
    }
}
