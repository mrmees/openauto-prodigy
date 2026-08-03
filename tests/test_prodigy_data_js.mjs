import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import vm from 'node:vm';

const ROOT = new URL('../', import.meta.url);
const PROTOBUF = readFileSync(new URL('resources/web/protobuf.min.js', ROOT), 'utf8');
const GENERATED = readFileSync(new URL('resources/web/prodigy-proto.js', ROOT), 'utf8');
const SHIM = readFileSync(new URL('resources/web/prodigy.js', ROOT), 'utf8');

function harness() {
    const sockets = [];
    const reconnects = [];
    let monotonic = 0;

    class FakeWebSocket {
        constructor(url) {
            this.url = url;
            this.readyState = 0;
            this.sent = [];
            sockets.push(this);
        }
        send(bytes) { this.sent.push(Uint8Array.from(bytes)); }
        open() {
            this.readyState = 1;
            if (this.onopen) this.onopen();
        }
        close() {
            this.readyState = 3;
            if (this.onclose) this.onclose();
        }
    }

    const sandbox = {
        __prodigyBootstrap: {
            apiUrl: 'ws://127.0.0.1:9811',
            context: { widgetId: 'test.widget' },
            themeTokens: {},
        },
        location: { protocol: 'prodigy:' },
        document: {
            documentElement: { style: { setProperty() {} } },
        },
        MutationObserver: class {
            observe() {}
            disconnect() {}
        },
        WebSocket: FakeWebSocket,
        performance: { now: () => monotonic },
        setTimeout: (callback) => { reconnects.push(callback); return reconnects.length; },
        clearTimeout() {},
        console,
        Uint8Array,
        ArrayBuffer,
        BigInt,
        Promise,
        Error,
        Object,
        Math,
        Number,
        String,
        Boolean,
        JSON,
    };
    sandbox.window = sandbox;
    vm.createContext(sandbox);
    vm.runInContext(PROTOBUF, sandbox, { filename: 'protobuf.min.js' });
    vm.runInContext(GENERATED, sandbox, { filename: 'prodigy-proto.js' });
    vm.runInContext(SHIM, sandbox, { filename: 'prodigy.js' });

    const pb = sandbox.protobuf.roots['prodigy-api'].prodigy.api.v1;
    function encode(fields) {
        return pb.ApiMessage.encode(pb.ApiMessage.create(fields)).finish();
    }
    function decode(bytes) { return pb.ApiMessage.decode(bytes); }
    function receive(socket, fields) {
        socket.onmessage({ data: Uint8Array.from(encode(fields)).buffer });
    }
    async function connect(dataCapability = true) {
        const socket = sockets.at(-1);
        socket.open();
        receive(socket, {
            requestId: 1,
            serverHello: {
                apiVersionMajor: 1,
                apiVersionMinor: 2,
                capabilities: dataCapability ? { dataProviderBridge: true } : {},
            },
        });
        await sandbox.prodigy.ready;
        await Promise.resolve();
        return socket;
    }

    return {
        sandbox, sockets, reconnects, pb, encode, decode, receive, connect,
        setMonotonic(value) { monotonic = value; },
    };
}

function dataRef(channelName) {
    return { providerNamespace: 'com.example.vehicle', channelName };
}

test('data API is capability-gated and lists the catalog', async () => {
    const absent = harness();
    await absent.connect(false);
    assert.equal(absent.sandbox.prodigy.data, undefined);

    const h = harness();
    const socket = await h.connect(true);
    assert.equal(typeof h.sandbox.prodigy.data.listCatalog, 'function');
    assert.equal(typeof h.sandbox.prodigy.data.subscribe, 'function');

    const pendingCatalog = h.sandbox.prodigy.data.listCatalog();
    await Promise.resolve();
    const request = h.decode(socket.sent.at(-1));
    assert.ok(request.listDataCatalogRequest);
    h.receive(socket, {
        requestId: request.requestId,
        listDataCatalogResponse: {
            catalog: { catalogRevision: 3, providers: [] },
        },
    });
    const catalog = await pendingCatalog;
    assert.equal(Number(catalog.catalogRevision), 3);
});

test('shared binding preserves exact scalars, metadata, and monotonic receipt', async () => {
    const h = harness();
    const socket = await h.connect(true);
    const eventsA = [];
    const eventsB = [];
    const ref = dataRef('mode');
    const unsubscribeA = h.sandbox.prodigy.data.subscribe(ref, e => eventsA.push(e));
    const unsubscribeB = h.sandbox.prodigy.data.subscribe(ref, e => eventsB.push(e));
    await Promise.resolve();

    const dataRequests = socket.sent.map(h.decode).filter(m => m.subscribeDataChannelsRequest);
    assert.equal(dataRequests.length, 1);
    assert.equal(dataRequests[0].subscribeDataChannelsRequest.channels.length, 1);

    h.receive(socket, {
        dataChannelAvailabilityEvent: {
            channel: ref,
            availability: 1,
            definition: {
                channelName: 'mode', displayName: 'Mode', valueType: 6,
                unit: 'state', enumOptions: [{ value: 7, label: 'Drive' }],
            },
            catalogRevision: 4,
        },
    });
    h.setMonotonic(250);
    h.receive(socket, {
        dataValuesEvent: {
            providerNamespace: ref.providerNamespace,
            samples: [{
                channelName: ref.channelName,
                value: { enumValue: h.sandbox.protobuf.util.Long.fromString('9007199254740993') },
                observedAtUnixMs: h.sandbox.protobuf.util.Long.fromString('1722000000000'),
                quality: 1,
            }],
        },
    });

    for (const events of [eventsA, eventsB]) {
        assert.equal(events.length, 2);
        assert.equal(events[0].available, true);
        assert.equal(events[0].definition.unit, 'state');
        assert.equal(typeof events[1].sample.value, 'bigint');
        assert.equal(events[1].sample.value, 9007199254740993n);
        assert.equal(events[1].sample.enumLabel, undefined);
        assert.equal(events[1].sample.timestampMs, 1722000000000);
        assert.equal(events[1].sample.receivedAtMonotonicMs, 250);
        assert.equal(events[1].sample.quality, 'good');
        assert.equal(events[1].sample.scalarType, 'enum');
    }

    h.setMonotonic(300);
    h.receive(socket, {
        dataValuesEvent: {
            providerNamespace: ref.providerNamespace,
            samples: [{
                channelName: ref.channelName,
                value: { enumValue: 7 },
                observedAtUnixMs: h.sandbox.protobuf.util.Long.fromString('100'),
                quality: 2,
            }],
        },
    });
    assert.equal(eventsA[2].sample.value, 7n);
    assert.equal(eventsA[2].sample.enumLabel, 'Drive');
    assert.equal(eventsA[2].sample.receivedAtMonotonicMs, 300);
    assert.equal(eventsA[2].sample.quality, 'degraded');

    unsubscribeA();
    await Promise.resolve();
    assert.equal(socket.sent.map(h.decode).filter(m => m.unsubscribeDataChannelsRequest).length, 0);
    unsubscribeB();
    await Promise.resolve();
    assert.equal(socket.sent.map(h.decode).filter(m => m.unsubscribeDataChannelsRequest).length, 1);
});

test('double, signed, unsigned, boolean, and string mappings are fixed', async () => {
    const h = harness();
    const socket = await h.connect(true);
    const received = new Map();
    const definitions = [
        ['double', 1], ['signed', 2], ['unsigned', 3],
        ['boolean', 4], ['string', 5],
    ];
    for (const [name] of definitions)
        h.sandbox.prodigy.data.subscribe(dataRef(name), e => {
            if (e.sample) received.set(name, e.sample);
        });
    await Promise.resolve();
    for (const [name, valueType] of definitions) {
        h.receive(socket, {
            dataChannelAvailabilityEvent: {
                channel: dataRef(name), availability: 1,
                definition: { channelName: name, displayName: name, valueType },
                catalogRevision: 2,
            },
        });
    }
    h.receive(socket, {
        dataValuesEvent: {
            providerNamespace: 'com.example.vehicle',
            samples: [
                { channelName: 'double', value: { doubleValue: 1.5 }, quality: 1 },
                { channelName: 'signed', value: { signedIntegerValue: -9 }, quality: 1 },
                { channelName: 'unsigned', value: { unsignedIntegerValue: 10 }, quality: 1 },
                { channelName: 'boolean', value: { booleanValue: true }, quality: 1 },
                { channelName: 'string', value: { stringValue: 'ready' }, quality: 1 },
            ],
        },
    });
    assert.equal(received.get('double').value, 1.5);
    assert.equal(received.get('signed').value, -9n);
    assert.equal(received.get('unsigned').value, 10n);
    assert.equal(received.get('boolean').value, true);
    assert.equal(received.get('string').value, 'ready');
});

test('legacy topic subscribers retain numeric int64 fields', async () => {
    const h = harness();
    const socket = await h.connect(true);
    const media = [];
    const phone = [];
    h.sandbox.prodigy.subscribe('media', status => media.push(status));
    h.sandbox.prodigy.subscribe('phone', status => phone.push(status));
    await Promise.resolve();

    h.receive(socket, {
        mediaStatus: {
            hasMedia: true,
            positionMs: h.sandbox.protobuf.util.Long.fromString('1234'),
            durationMs: h.sandbox.protobuf.util.Long.fromString('5678'),
            hasPosition: true,
        },
    });
    h.receive(socket, {
        phoneStatus: {
            hfpConnected: true,
            calls: [{
                state: 2,
                startedAtUnixMs: h.sandbox.protobuf.util.Long.fromString('1722000000000'),
            }],
        },
    });

    assert.equal(typeof media[0].positionMs, 'number');
    assert.equal(media[0].positionMs, 1234);
    assert.equal(typeof media[0].durationMs, 'number');
    assert.equal(media[0].durationMs, 5678);
    assert.equal(typeof phone[0].calls[0].startedAtUnixMs, 'number');
    assert.equal(phone[0].calls[0].startedAtUnixMs, 1722000000000);
});

test('disconnect marks bindings unavailable and reconnect restores them', async () => {
    const h = harness();
    const first = await h.connect(true);
    const events = [];
    h.sandbox.prodigy.data.subscribe(dataRef('engine.rpm'), event => events.push(event));
    await Promise.resolve();
    first.close();
    assert.equal(events.at(-1).available, false);
    assert.equal(events.at(-1).unavailableReason, 'link_lost');
    await assert.rejects(h.sandbox.prodigy.data.listCatalog(), /not connected/);

    assert.equal(h.reconnects.length, 1);
    h.reconnects.shift()();
    const second = h.sockets.at(-1);
    second.open();
    h.receive(second, {
        serverHello: {
            apiVersionMajor: 1, apiVersionMinor: 2,
            capabilities: { dataProviderBridge: true },
        },
    });
    await Promise.resolve();
    const restored = second.sent.map(h.decode)
        .filter(message => message.subscribeDataChannelsRequest);
    assert.equal(restored.length, 1);
    assert.equal(restored[0].subscribeDataChannelsRequest.channels[0].channelName,
                 'engine.rpm');
});
