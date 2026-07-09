/* host-gestures.js — host-side long-press detection injected into every web
 * widget page. This is host plumbing, NOT widget API (prodigy.js / rail R3
 * is untouched): a Qt pointer handler over the WebEngineView SIGSEGVs the UI
 * process when the scene mutates mid-touch-stream (on-device 2026-07-07,
 * Codex-diagnosed), so the long-press is detected INSIDE the page and
 * reported to the host only after pointer release, via a sentinel
 * navigation the host intercepts and ignores.
 *
 * Observe-only: never calls preventDefault/stopPropagation — page buttons
 * and widget interactions must keep working exactly as before.
 */
(function () {
    'use strict';
    if (window.location.protocol === 'about:') return;   // initial about:blank gets
    // the DocumentCreation scripts too — never run host plumbing there

    var HOLD_MS = 500;          // match widgetMouseArea.pressAndHoldInterval
    var MOVE_SQ = 144;          // ~12px — movement beyond this is a drag, not a hold

    var timer = null;
    var armed = false;          // hold threshold reached; fire on release
    var fired = false;          // sentinel already sent for this press
    var pressX = 0, pressY = 0;

    function disarm() {
        if (timer !== null) { clearTimeout(timer); timer = null; }
        armed = false;
    }

    document.addEventListener('pointerdown', function (ev) {
        if (!ev.isPrimary) return;
        disarm();
        fired = false;
        pressX = ev.clientX;
        pressY = ev.clientY;
        timer = setTimeout(function () {
            timer = null;
            armed = true;
        }, HOLD_MS);
    }, true);

    document.addEventListener('pointermove', function (ev) {
        if (!ev.isPrimary) return;
        if (timer === null && !armed) return;
        var dx = ev.clientX - pressX;
        var dy = ev.clientY - pressY;
        if (dx * dx + dy * dy > MOVE_SQ)
            disarm();
    }, true);

    document.addEventListener('pointerup', function (ev) {
        if (!ev.isPrimary) return;
        var fire = armed && !fired;
        disarm();
        if (fire) {
            fired = true;
            // Sentinel navigation: WebWidgetHost.onNavigationRequested ignores
            // the request and emits longPressed() — no navigation happens.
            window.location.href = 'prodigy://host/longpress';
        }
    }, true);

    document.addEventListener('pointercancel', function (ev) {
        if (!ev.isPrimary) return;
        disarm();
    }, true);
})();
