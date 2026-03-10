pragma Singleton
import QtQuick

QtObject {
    // --- Config reads (updated reactively via Connections property) ---
    property var _cfgScale: ConfigService.value("ui.scale")
    property var _cfgFontScale: ConfigService.value("ui.fontScale")

    // Declared as property — QtObject cannot have child objects
    property Connections _configWatcher: Connections {
        target: ConfigService
        function onConfigChanged(path, value) {
            if (path === "ui.scale") _cfgScale = value
            else if (path === "ui.fontScale") _cfgFontScale = value
        }
    }

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

    // --- Window dimensions from C++ DisplayInfo bridge ---
    readonly property int _winW: DisplayInfo ? DisplayInfo.windowWidth : 1024
    readonly property int _winH: DisplayInfo ? DisplayInfo.windowHeight : 600

    // --- DPI-based scaling (replaces hardcoded 1024/600 resolution ratio) ---
    readonly property real _screenSize: DisplayInfo ? DisplayInfo.screenSizeInches : 7.0
    readonly property real _diagPx: Math.sqrt(_winW * _winW + _winH * _winH)
    readonly property real _dpi: _diagPx / _screenSize
    readonly property real _referenceDpi: 160.0  // Android mdpi standard

    // --- Dual-axis scale factors (unclamped) ---
    // DPI-based: actual_DPI / 160 (uniform for square pixels, preserves dual-axis infrastructure)
    readonly property real scaleH: _dpi / _referenceDpi
    readonly property real scaleV: _dpi / _referenceDpi

    // --- Layout scale: min of axes (prevents overflow) ---
    readonly property real autoScale: Math.min(scaleH, scaleV)

    // --- Font auto-scale: geometric mean (balanced readability) ---
    readonly property real _autoFontScale: Math.sqrt(scaleH * scaleV)

    // --- Combined scales ---
    readonly property real scale: autoScale * globalScale
    readonly property real _fontScaleTotal: _autoFontScale * globalScale * fontScale

    // --- Token override helper (returns NaN for "not set") ---
    function _tok(name) {
        var v = ConfigService.value("ui.tokens." + name);
        if (v !== undefined && v !== null && Number(v) > 0) return Number(v);
        return NaN;
    }

    // --- Font floor helper ---
    // Returns the pixel floor for a font token.
    // Checks per-token floor first, then global floor, then default.
    function _floor(tokenName, defaultFloor) {
        // Per-token floor: ui.fontFloor.<tokenName>
        var perToken = ConfigService.value("ui.fontFloor." + tokenName);
        if (perToken !== undefined && perToken !== null && Number(perToken) > 0)
            return Number(perToken);
        // Global floor: ui.fontFloor (scalar)
        var global = ConfigService.value("ui.fontFloor");
        if (global !== undefined && global !== null && Number(global) > 0)
            return Number(global);
        // Built-in default
        return defaultFloor;
    }

    // Touch targets (layout tokens -- use scale)
    readonly property int rowH:     { var o = _tok("rowH"); return isNaN(o) ? Math.round(80 * scale) : o; }
    readonly property int touchMin: { var o = _tok("touchMin"); return isNaN(o) ? Math.round(56 * scale) : o; }
    readonly property int navbarThick: { var o = _tok("navbarThick"); return isNaN(o) ? Math.round(56 * scale) : o; }

    // Spacing (non-overridable -- scale only)
    readonly property int marginPage: Math.round(20 * scale)
    readonly property int marginRow:  Math.round(12 * scale)
    readonly property int spacing:    Math.round(8 * scale)
    readonly property int gap:        Math.round(16 * scale)
    readonly property int sectionGap: Math.round(20 * scale)

    // Fonts (overridable -- use _fontScaleTotal, with pixel floors)
    readonly property int fontTiny:    { var o = _tok("fontTiny"); return isNaN(o) ? Math.max(Math.round(14 * _fontScaleTotal), _floor("fontTiny", 10)) : o; }
    readonly property int fontSmall:   { var o = _tok("fontSmall"); return isNaN(o) ? Math.max(Math.round(16 * _fontScaleTotal), _floor("fontSmall", 12)) : o; }
    readonly property int fontBody:    { var o = _tok("fontBody"); return isNaN(o) ? Math.max(Math.round(20 * _fontScaleTotal), _floor("fontBody", 14)) : o; }
    readonly property int fontTitle:   { var o = _tok("fontTitle"); return isNaN(o) ? Math.max(Math.round(22 * _fontScaleTotal), _floor("fontTitle", 16)) : o; }
    readonly property int fontHeading: { var o = _tok("fontHeading"); return isNaN(o) ? Math.max(Math.round(28 * _fontScaleTotal), _floor("fontHeading", 18)) : o; }

    // Component sizing (overridable layout tokens -- use scale)
    readonly property int headerH:     { var o = _tok("headerH"); return isNaN(o) ? Math.round(56 * scale) : o; }
    readonly property int sectionH:    Math.round(36 * scale)
    readonly property int backBtnSize: Math.round(44 * scale)
    readonly property int iconSize:    { var o = _tok("iconSize"); return isNaN(o) ? Math.round(36 * scale) : o; }
    readonly property int iconSmall:   Math.round(20 * scale)

    // Radii (overridable -- use scale)
    readonly property int radius: { var o = _tok("radius"); return isNaN(o) ? Math.round(10 * scale) : o; }

    // Grid (non-overridable -- scale only)
    readonly property int gridGap: Math.round(24 * scale)

    // Animation durations (ms) -- not scaled, timing is absolute
    readonly property int animDuration: 150       // standard transition (StackView push/pop)
    readonly property int animDurationFast: 80    // quick feedback (press scale/opacity)

    // Category tile sizing (overridable -- use scale)
    readonly property int tileW: { var o = _tok("tileW"); return isNaN(o) ? Math.round(280 * scale) : o; }
    readonly property int tileH: { var o = _tok("tileH"); return isNaN(o) ? Math.round(200 * scale) : o; }
    readonly property int tileIconSize: Math.round(48 * scale)

    // Slider/scrollbar tokens (overridable -- use scale)
    readonly property int trackThick:    { var o = _tok("trackThick"); return isNaN(o) ? Math.round(6 * scale) : o; }
    readonly property int trackThin:     { var o = _tok("trackThin"); return isNaN(o) ? Math.max(Math.round(1 * scale), 1) : o; }
    readonly property int knobSize:      { var o = _tok("knobSize"); return isNaN(o) ? Math.round(22 * scale) : o; }
    readonly property int knobSizeSmall: { var o = _tok("knobSizeSmall"); return isNaN(o) ? Math.round(18 * scale) : o; }

    // Radii (additional tiers)
    readonly property int radiusSmall: { var o = _tok("radiusSmall"); return isNaN(o) ? Math.round(8 * scale) : o; }
    readonly property int radiusLarge: { var o = _tok("radiusLarge"); return isNaN(o) ? Math.round(16 * scale) : o; }

    // Content sizing
    readonly property int albumArt: { var o = _tok("albumArt"); return isNaN(o) ? Math.round(200 * scale) : o; }

    // Button sizing (with touch-target floors)
    readonly property int callBtnSize: { var o = _tok("callBtnSize"); return isNaN(o) ? Math.max(Math.round(72 * scale), touchMin) : o; }
    readonly property int overlayBtnW: { var o = _tok("overlayBtnW"); return isNaN(o) ? Math.round(100 * scale) : o; }
    readonly property int overlayBtnH: { var o = _tok("overlayBtnH"); return isNaN(o) ? Math.max(Math.round(36 * scale), touchMin) : o; }

    // Small indicators
    readonly property int statusDot: Math.round(10 * scale)
    readonly property int progressH: Math.round(4 * scale)

    // --- Startup logging ---
    Component.onCompleted: {
        console.log("[UiMetrics] window:", _winW + "x" + _winH,
                    "screen:", _screenSize + "\"",
                    "dpi:", Math.round(_dpi),
                    "scaleH:", scaleH.toFixed(3), "scaleV:", scaleV.toFixed(3),
                    "layoutScale:", autoScale.toFixed(3), "fontScale:", _autoFontScale.toFixed(3),
                    "globalScale:", globalScale, "fontScaleCfg:", fontScale,
                    "combinedScale:", scale.toFixed(3), "combinedFontScale:", _fontScaleTotal.toFixed(3));

        if (autoScale < 0.7)
            console.warn("[UiMetrics] WARNING: autoScale", autoScale.toFixed(3), "< 0.7 -- UI may be unusably small");
        if (autoScale > 2.0)
            console.warn("[UiMetrics] WARNING: autoScale", autoScale.toFixed(3), "> 2.0 -- UI may be oversized");

        // Log font floor activations
        var fonts = [
            { name: "fontTiny",    base: 14, floor: _floor("fontTiny", 10) },
            { name: "fontSmall",   base: 16, floor: _floor("fontSmall", 12) },
            { name: "fontBody",    base: 20, floor: _floor("fontBody", 14) },
            { name: "fontTitle",   base: 22, floor: _floor("fontTitle", 16) },
            { name: "fontHeading", base: 28, floor: _floor("fontHeading", 18) }
        ];
        for (var i = 0; i < fonts.length; i++) {
            var f = fonts[i];
            var scaled = Math.round(f.base * _fontScaleTotal);
            if (scaled < f.floor)
                console.log("[UiMetrics] floor active:", f.name, "scaled=" + scaled, "floor=" + f.floor);
        }
    }
}
