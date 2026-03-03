pragma Singleton
import QtQuick
import QtQuick.Window

QtObject {
    // --- Config reads (bound once at creation) ---
    readonly property var _cfgScale: ConfigService.value("ui.scale")
    readonly property var _cfgFontScale: ConfigService.value("ui.fontScale")

    // --- Effective scale factors (0 = not set = 1.0) ---
    readonly property real globalScale: {
        var v = _cfgScale;
        if (v !== undefined && v !== null && Number(v) > 0) return Number(v);
        return 1.0;
    }
    readonly property real fontScale: {
        var v = _cfgFontScale;
        if (v !== undefined && v !== null && Number(v) > 0) return Number(v);
        return 1.0;
    }

    // --- Auto scale from shortest screen edge vs 600px baseline ---
    readonly property real autoScale: {
        var shortest = Math.min(Screen.width, Screen.height);
        return Math.max(0.9, Math.min(1.35, shortest / 600));
    }

    // --- Combined scales ---
    readonly property real scale: autoScale * globalScale
    readonly property real _fontScaleTotal: scale * fontScale

    // --- Token override helper (returns NaN for "not set") ---
    function _tok(name) {
        var v = ConfigService.value("ui.tokens." + name);
        if (v !== undefined && v !== null && Number(v) > 0) return Number(v);
        return NaN;
    }

    // Touch targets (layout tokens — use scale)
    readonly property int rowH:     { var o = _tok("rowH"); return isNaN(o) ? Math.round(80 * scale) : o; }
    readonly property int touchMin: { var o = _tok("touchMin"); return isNaN(o) ? Math.round(56 * scale) : o; }

    // Spacing (non-overridable — scale only)
    readonly property int marginPage: Math.round(20 * scale)
    readonly property int marginRow:  Math.round(12 * scale)
    readonly property int spacing:    Math.round(8 * scale)
    readonly property int gap:        Math.round(16 * scale)
    readonly property int sectionGap: Math.round(20 * scale)

    // Fonts (overridable — use _fontScaleTotal)
    readonly property int fontTitle:   { var o = _tok("fontTitle"); return isNaN(o) ? Math.round(22 * _fontScaleTotal) : o; }
    readonly property int fontBody:    { var o = _tok("fontBody"); return isNaN(o) ? Math.round(20 * _fontScaleTotal) : o; }
    readonly property int fontSmall:   { var o = _tok("fontSmall"); return isNaN(o) ? Math.round(16 * _fontScaleTotal) : o; }
    readonly property int fontHeading: { var o = _tok("fontHeading"); return isNaN(o) ? Math.round(28 * _fontScaleTotal) : o; }

    // Font (non-overridable — benefits from fontScale but no individual override)
    readonly property int fontTiny: Math.round(14 * _fontScaleTotal)

    // Component sizing (overridable layout tokens — use scale)
    readonly property int headerH:     { var o = _tok("headerH"); return isNaN(o) ? Math.round(56 * scale) : o; }
    readonly property int sectionH:    Math.round(36 * scale)
    readonly property int backBtnSize: Math.round(44 * scale)
    readonly property int iconSize:    { var o = _tok("iconSize"); return isNaN(o) ? Math.round(36 * scale) : o; }
    readonly property int iconSmall:   Math.round(20 * scale)

    // Radii (overridable — use scale)
    readonly property int radius: { var o = _tok("radius"); return isNaN(o) ? Math.round(10 * scale) : o; }

    // Grid (non-overridable — scale only)
    readonly property int gridGap: Math.round(24 * scale)

    // Animation durations (ms) — not scaled, timing is absolute
    readonly property int animDuration: 150       // standard transition (StackView push/pop)
    readonly property int animDurationFast: 80    // quick feedback (press scale/opacity)

    // Category tile sizing (overridable — use scale)
    readonly property int tileW: { var o = _tok("tileW"); return isNaN(o) ? Math.round(280 * scale) : o; }
    readonly property int tileH: { var o = _tok("tileH"); return isNaN(o) ? Math.round(200 * scale) : o; }
    readonly property int tileIconSize: Math.round(48 * scale)
}
