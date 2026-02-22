pragma Singleton
import QtQuick
import QtQuick.Window

QtObject {
    // Scale from shortest screen edge vs 600px baseline.
    // Screen.height gives the physical display height even in fullscreen.
    readonly property real scale: {
        var shortest = Math.min(Screen.width, Screen.height);
        return Math.max(0.9, Math.min(1.35, shortest / 600));
    }

    // Touch targets
    readonly property int rowH:     Math.round(48 * scale)
    readonly property int touchMin: Math.round(44 * scale)

    // Spacing
    readonly property int marginPage: Math.round(16 * scale)
    readonly property int marginRow:  Math.round(8 * scale)
    readonly property int spacing:    Math.round(4 * scale)
    readonly property int gap:        Math.round(12 * scale)
    readonly property int sectionGap: Math.round(16 * scale)

    // Fonts
    readonly property int fontTitle:   Math.round(18 * scale)
    readonly property int fontBody:    Math.round(15 * scale)
    readonly property int fontSmall:   Math.round(13 * scale)
    readonly property int fontTiny:    Math.round(12 * scale)
    readonly property int fontHeading: Math.round(22 * scale)

    // Component sizing
    readonly property int headerH:     Math.round(40 * scale)
    readonly property int sectionH:    Math.round(32 * scale)
    readonly property int backBtnSize: Math.round(36 * scale)
    readonly property int iconSize:    Math.round(24 * scale)
    readonly property int iconSmall:   Math.round(16 * scale)

    // Radii
    readonly property int radius: Math.round(8 * scale)

    // Grid (settings menu tiles)
    readonly property int gridGap: Math.round(20 * scale)
}
