/* prodigy.js — the bootstrap shim injected into every web widget (design
 * 2026-07-06-js-runtime §6). This is the ONLY privileged surface a widget
 * gets; everything rides the public External API over WebSocket (rail R3).
 * Injection order (WebWidgetHost.qml): bootstrap -> protobuf.min.js ->
 * prodigy-proto.js -> this file.
 */
(function () {
    'use strict';
    if (window.prodigy) return;
    if (window.location.protocol === 'about:') return;   // initial about:blank gets
    // the DocumentCreation scripts too — never theme/connect there (leaked WS + renderer)

    var boot = window.__prodigyBootstrap || {};
    var root = (window.protobuf && protobuf.roots && protobuf.roots['prodigy-api']) || null;
    var pb = root && root.prodigy && root.prodigy.api ? root.prodigy.api.v1 : null;

    // ---- theme tokens -> CSS custom properties (--prodigy-<token>) ------
    function setVars(el, tokens) {
        Object.keys(tokens).forEach(function (k) {
            el.style.setProperty('--prodigy-' + k, tokens[k]);
        });
    }
    function applyTokens(tokens) {
        if (!tokens) return;
        if (document.documentElement) {
            setVars(document.documentElement, tokens);
            return;
        }
        // This script runs at WebEngineScript.DocumentCreation, which fires
        // before <html> exists — document.documentElement is still null here.
        // Apply as soon as it appears (well before body/CSS content, so first
        // paint is still themed per D6) instead of throwing and aborting the
        // rest of this IIFE (which left window.prodigy undefined).
        var mo = new MutationObserver(function () {
            if (document.documentElement) {
                mo.disconnect();
                setVars(document.documentElement, tokens);
            }
        });
        mo.observe(document, { childList: true });
    }
    applyTokens(boot.themeTokens);   // first paint is already themed (D6)

    // ---- events ----------------------------------------------------------
    var listeners = {};
    function emit(name, arg) {
        (listeners[name] || []).forEach(function (cb) {
            try { cb(arg); } catch (e) { console.error('prodigy: listener error', e); }
        });
    }

    var TOPIC = { media: 1, navigation: 2, projection: 3, phone: 4, system: 5 };
    var STATUS_FIELD = {
        mediaStatus: 'media', navigationStatus: 'navigation',
        projectionStatus: 'projection', phoneStatus: 'phone', systemStatus: 'system'
    };

    var ws = null;
    var nextRequestId = 1;
    var pending = {};                  // request_id -> {resolve, reject}
    var subs = {};                     // topic name -> [callback]
    var backoffMs = 1000;
    var readyResolve;
    var readyPromise = new Promise(function (res) { readyResolve = res; });

    function encode(fields) {
        return pb.ApiMessage.encode(pb.ApiMessage.create(fields)).finish();
    }
    function reqId(msg) {
        return msg.requestId && msg.requestId.toNumber
            ? msg.requestId.toNumber() : Number(msg.requestId || 0);
    }

    function request(fields) {
        return readyPromise.then(function () {
            return new Promise(function (resolve, reject) {
                if (!ws || ws.readyState !== 1) {   // 1 = OPEN; send() on CLOSED silently drops -> black-holed pending
                    reject(new Error('prodigy: not connected'));
                    return;
                }
                var id = nextRequestId++;
                fields.requestId = id;
                pending[id] = { resolve: resolve, reject: reject };
                try { ws.send(encode(fields)); }
                catch (e) { delete pending[id]; reject(e); }
            });
        });
    }

    function activeTopics() {
        var t = [TOPIC.system];        // theme updates always flow
        Object.keys(subs).forEach(function (name) {
            if (subs[name].length && TOPIC[name] !== TOPIC.system)
                t.push(TOPIC[name]);
        });
        return t;
    }

    function handleStream(msg) {
        Object.keys(STATUS_FIELD).forEach(function (field) {
            if (!msg[field]) return;
            var topic = STATUS_FIELD[field];
            if (topic === 'system' && msg[field].themeTokens) {
                applyTokens(msg[field].themeTokens);
                emit('themechange', msg[field].themeTokens);
            }
            (subs[topic] || []).forEach(function (cb) {
                try { cb(msg[field]); } catch (e) { console.error('prodigy: subscriber error', e); }
            });
        });
    }

    function onFrame(ev) {
        var msg;
        try { msg = pb.ApiMessage.decode(new Uint8Array(ev.data)); }
        catch (e) { console.error('prodigy: undecodable frame', e); return; }

        if (msg.serverHello) {         // (re)connected
            backoffMs = 1000;
            ws.send(encode({ requestId: nextRequestId++,
                             subscribeRequest: { topics: activeTopics() } }));
            readyResolve();
            return;
        }
        var id = reqId(msg);
        if (id && pending[id]) {
            var p = pending[id];
            delete pending[id];
            if (msg.error)
                p.reject(new Error('api error ' + msg.error.code +
                                   (msg.error.message ? ': ' + msg.error.message : '')));
            else
                p.resolve(msg);
            return;
        }
        handleStream(msg);             // request_id 0 = stream event
    }

    function connect() {
        ws = new WebSocket(boot.apiUrl);
        ws.binaryType = 'arraybuffer';
        ws.onopen = function () {
            ws.send(encode({
                requestId: nextRequestId++,
                clientHello: {
                    requestedApiVersionMajor: 1,
                    requestedApiVersionMinor: 1,
                    clientName: (boot.context && boot.context.widgetId) || 'web-widget',
                    clientKind: pb.ClientKind.CLIENT_KIND_WEB_WIDGET
                }
            }));
        };
        ws.onmessage = onFrame;
        ws.onclose = function () {
            Object.keys(pending).forEach(function (id) {
                pending[id].reject(new Error('prodigy: connection closed'));
                delete pending[id];
            });
            setTimeout(connect, backoffMs);
            backoffMs = Math.min(backoffMs * 2, 30000);   // capped backoff (design §6)
        };
        ws.onerror = function () { /* onclose fires next */ };
    }

    window.prodigy = {
        get ready() { return readyPromise; },
        context: boot.context || {},
        apiUrl: boot.apiUrl,

        subscribe: function (topic, cb) {
            if (!Object.prototype.hasOwnProperty.call(TOPIC, topic))
                throw new Error('prodigy: unknown topic ' + topic);
            (subs[topic] = subs[topic] || []).push(cb);
            readyPromise.then(function () {
                try { ws.send(encode({ requestId: nextRequestId++,
                                       subscribeRequest: { topics: activeTopics() } })); }
                catch (e) { /* re-subscribe happens on reconnect */ }
            });
            return function unsubscribe() {
                var arr = subs[topic] || [];
                var i = arr.indexOf(cb);
                if (i >= 0) arr.splice(i, 1);
            };
        },

        dispatch: function (actionId, payload) {
            var req = { id: String(actionId) };
            if (payload !== undefined) req.payloadJson = JSON.stringify(payload);
            return request({ dispatchActionRequest: req }).then(function (msg) {
                return !!(msg.dispatchActionResponse && msg.dispatchActionResponse.dispatched);
            });
        },

        notify: function (message, opts) {
            opts = opts || {};
            var req = { kind: 1 /* TOAST */, message: String(message),
                        ttlMs: opts.ttlMs || 0 };
            if (opts.priority !== undefined) req.priority = opts.priority;
            return request({ postNotificationRequest: req }).then(function (msg) {
                return msg.postNotificationResponse
                    ? msg.postNotificationResponse.notificationId : '';
            });
        },

        request: request,              // low-level escape hatch (design §6)

        on: function (name, cb) {
            (listeners[name] = listeners[name] || []).push(cb);
        },

        // host-internal: WebWidgetHost pushes span changes (design §6 bootstrap)
        _updateContext: function (ctx) {
            window.prodigy.context = ctx;
            emit('contextchange', ctx);
        }
    };

    if (pb) connect();
    else console.error('prodigy: proto module missing — API bridge disabled');
})();
