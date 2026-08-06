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

    // protobuf.js falls back to Number for 64-bit fields when long.js is not
    // present. The bundled browser runtime intentionally has no extra long.js
    // dependency, so install the small Long-compatible surface its reader
    // needs, backed by native BigInt, before the first frame is decoded.
    function installBigIntLong() {
        if (!window.protobuf || !protobuf.util || typeof BigInt !== 'function') return;
        function BigLong(low, high, unsigned) {
            this.low = low | 0;
            this.high = high | 0;
            this.unsigned = !!unsigned;
        }
        function fromBigInt(value, unsigned) {
            var bits = BigInt(value);
            var width = 1n << 64n;
            if (bits < 0) bits += width;
            bits &= width - 1n;
            return new BigLong(Number(bits & 0xffffffffn) | 0,
                               Number((bits >> 32n) & 0xffffffffn) | 0,
                               !!unsigned);
        }
        BigLong.fromBits = function (low, high, unsigned) {
            return new BigLong(low, high, unsigned);
        };
        BigLong.fromString = function (value, unsigned) {
            return fromBigInt(BigInt(value), unsigned);
        };
        BigLong.fromValue = function (value) {
            if (value instanceof BigLong) return value;
            if (value && typeof value === 'object' &&
                    value.low !== undefined && value.high !== undefined) {
                return new BigLong(value.low, value.high, value.unsigned);
            }
            return fromBigInt(BigInt(value || 0), false);
        };
        BigLong.isLong = function (value) { return value instanceof BigLong; };
        BigLong.prototype.toBigInt = function () {
            var bits = (BigInt(this.high >>> 0) << 32n) | BigInt(this.low >>> 0);
            if (!this.unsigned && (this.high & 0x80000000)) bits -= 1n << 64n;
            return bits;
        };
        BigLong.prototype.toString = function () { return this.toBigInt().toString(); };
        BigLong.prototype.toNumber = function () { return Number(this.toBigInt()); };
        protobuf.util.Long = BigLong;
        if (protobuf.configure) protobuf.configure();
    }
    installBigIntLong();

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
    var dataBindings = {};             // provider\0channel -> binding
    var dataCapable = false;
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

    // Before the data bridge, protobuf.js decoded int64 fields as Number.
    // Keep that established shape for legacy topic callbacks even though the
    // data domain installs an exact Long implementation for typed scalars.
    function normalizeLegacyLongs(value) {
        if (!value || typeof value !== 'object') return value;
        if (protobuf.util.Long && protobuf.util.Long.isLong(value))
            return value.toNumber();
        if (Array.isArray(value)) {
            for (var i = 0; i < value.length; ++i)
                value[i] = normalizeLegacyLongs(value[i]);
            return value;
        }
        Object.keys(value).forEach(function (key) {
            value[key] = normalizeLegacyLongs(value[key]);
        });
        return value;
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

    function dataKey(ref) {
        return String(ref.providerNamespace) + '\u0000' + String(ref.channelName);
    }

    function normalizedRef(ref) {
        if (!ref || !ref.providerNamespace || !ref.channelName)
            throw new Error('prodigy: data subscription requires providerNamespace and channelName');
        return {
            providerNamespace: String(ref.providerNamespace),
            channelName: String(ref.channelName)
        };
    }

    function longToBigInt(value) {
        if (typeof value === 'bigint') return value;
        if (value && value.toString) return BigInt(value.toString());
        return BigInt(value || 0);
    }

    var QUALITY = ['unknown', 'good', 'degraded', 'stale', 'invalid', 'unavailable'];
    var UNAVAILABLE_REASON = [
        'unspecified', 'provider_absent', 'channel_absent',
        'provider_disconnected', 'channel_removed'
    ];

    function scalarFromProto(scalar, definition) {
        scalar = scalar || {};
        var own = Object.prototype.hasOwnProperty;
        var result = { value: undefined, scalarType: 'unspecified' };
        if (own.call(scalar, 'doubleValue')) {
            result.value = scalar.doubleValue;
            result.scalarType = 'double';
        } else if (own.call(scalar, 'signedIntegerValue')) {
            result.value = longToBigInt(scalar.signedIntegerValue);
            result.scalarType = 'signed_integer';
        } else if (own.call(scalar, 'unsignedIntegerValue')) {
            result.value = longToBigInt(scalar.unsignedIntegerValue);
            result.scalarType = 'unsigned_integer';
        } else if (own.call(scalar, 'booleanValue')) {
            result.value = scalar.booleanValue;
            result.scalarType = 'boolean';
        } else if (own.call(scalar, 'stringValue')) {
            result.value = scalar.stringValue;
            result.scalarType = 'string';
        } else if (own.call(scalar, 'enumValue')) {
            result.value = longToBigInt(scalar.enumValue);
            result.scalarType = 'enum';
            (definition && definition.enumOptions || []).some(function (option) {
                if (longToBigInt(option.value) !== result.value) return false;
                result.enumLabel = option.label;
                return true;
            });
        }
        return result;
    }

    function notifyBinding(binding, event) {
        binding.callbacks.slice().forEach(function (cb) {
            try { cb(event); }
            catch (e) { console.error('prodigy: data subscriber error', e); }
        });
    }

    function availabilityEvent(binding, available, reason, definition) {
        return {
            providerNamespace: binding.ref.providerNamespace,
            channelName: binding.ref.channelName,
            available: available,
            unavailableReason: available ? null : reason,
            definition: available ? definition : null,
            sample: null
        };
    }

    function handleDataStream(msg) {
        if (msg.dataChannelAvailabilityEvent) {
            var availability = msg.dataChannelAvailabilityEvent;
            var ref = availability.channel || {};
            var binding = dataBindings[dataKey(ref)];
            if (!binding) return true;
            var available = availability.availability === 1;
            binding.definition = available ? availability.definition : null;
            binding.available = available;
            notifyBinding(binding, availabilityEvent(
                binding, available,
                UNAVAILABLE_REASON[availability.unavailableReason] || 'unspecified',
                binding.definition));
            return true;
        }
        if (msg.dataValuesEvent) {
            var values = msg.dataValuesEvent;
            (values.samples || []).forEach(function (wireSample) {
                var ref = {
                    providerNamespace: values.providerNamespace,
                    channelName: wireSample.channelName
                };
                var binding = dataBindings[dataKey(ref)];
                if (!binding) return;
                var scalar = scalarFromProto(wireSample.value, binding.definition);
                var sample = {
                    value: scalar.value,
                    scalarType: scalar.scalarType,
                    timestampMs: wireSample.observedAtUnixMs == null
                        ? undefined : Number(wireSample.observedAtUnixMs.toString()),
                    receivedAtMonotonicMs: performance.now(),
                    quality: QUALITY[wireSample.quality] || 'unknown'
                };
                if (scalar.enumLabel !== undefined) sample.enumLabel = scalar.enumLabel;
                notifyBinding(binding, {
                    providerNamespace: binding.ref.providerNamespace,
                    channelName: binding.ref.channelName,
                    available: true,
                    unavailableReason: null,
                    definition: binding.definition,
                    sample: sample
                });
            });
            return true;
        }
        return false;
    }

    function sendDataSubscriptions() {
        var channels = Object.keys(dataBindings).map(function (key) {
            return dataBindings[key].ref;
        }).filter(function (ref) {
            return dataBindings[dataKey(ref)].callbacks.length > 0;
        });
        if (!dataCapable || !channels.length || !ws || ws.readyState !== 1) return;
        ws.send(encode({ requestId: nextRequestId++,
                         subscribeDataChannelsRequest: { channels: channels } }));
    }

    function markDataDisconnected() {
        Object.keys(dataBindings).forEach(function (key) {
            var binding = dataBindings[key];
            binding.available = false;
            binding.definition = null;
            notifyBinding(binding, availabilityEvent(
                binding, false, 'link_lost', null));
        });
    }

    var dataApi = {
        listCatalog: function () {
            return request({ listDataCatalogRequest: {} }).then(function (msg) {
                if (!msg.listDataCatalogResponse)
                    throw new Error('prodigy: missing catalog response');
                return msg.listDataCatalogResponse.catalog;
            });
        },
        subscribe: function (ref, cb) {
            if (typeof cb !== 'function')
                throw new Error('prodigy: data subscription callback required');
            ref = normalizedRef(ref);
            var key = dataKey(ref);
            var binding = dataBindings[key];
            if (!binding) {
                binding = dataBindings[key] = {
                    ref: ref, callbacks: [], definition: null, available: false
                };
            }
            var first = binding.callbacks.length === 0;
            binding.callbacks.push(cb);
            if (first) {
                request({ subscribeDataChannelsRequest: { channels: [ref] } })
                    .catch(function () { /* reconnect restores active bindings */ });
            }
            var removed = false;
            return function unsubscribeData() {
                if (removed) return;
                removed = true;
                var index = binding.callbacks.indexOf(cb);
                if (index >= 0) binding.callbacks.splice(index, 1);
                if (binding.callbacks.length) return;
                delete dataBindings[key];
                request({ unsubscribeDataChannelsRequest: { channels: [ref] } })
                    .catch(function () { /* already locally removed */ });
            };
        }
    };

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
            var status = normalizeLegacyLongs(msg[field]);
            if (topic === 'system' && msg[field].themeTokens) {
                applyTokens(status.themeTokens);
                emit('themechange', status.themeTokens);
            }
            (subs[topic] || []).forEach(function (cb) {
                try { cb(status); } catch (e) { console.error('prodigy: subscriber error', e); }
            });
        });
    }

    function onFrame(ev) {
        var msg;
        try { msg = pb.ApiMessage.decode(new Uint8Array(ev.data)); }
        catch (e) { console.error('prodigy: undecodable frame', e); return; }

        if (msg.serverHello) {         // (re)connected
            backoffMs = 1000;
            var capabilities = msg.serverHello.capabilities || {};
            dataCapable = Object.prototype.hasOwnProperty.call(
                capabilities, 'dataProviderBridge') &&
                capabilities.dataProviderBridge === true;
            if (dataCapable) window.prodigy.data = dataApi;
            else delete window.prodigy.data;
            ws.send(encode({ requestId: nextRequestId++,
                             subscribeRequest: { topics: activeTopics() } }));
            sendDataSubscriptions();
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
        if (!handleDataStream(msg))
            handleStream(msg);         // request_id 0 = stream event
    }

    function connect() {
        ws = new WebSocket(boot.apiUrl);
        ws.binaryType = 'arraybuffer';
        ws.onopen = function () {
            ws.send(encode({
                requestId: nextRequestId++,
                clientHello: {
                    requestedApiVersionMajor: 1,
                    requestedApiVersionMinor: 2,
                    clientName: (boot.context && boot.context.widgetId) || 'web-widget',
                    clientKind: pb.ClientKind.CLIENT_KIND_WEB_WIDGET
                }
            }));
        };
        ws.onmessage = onFrame;
        ws.onclose = function () {
            markDataDisconnected();
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
