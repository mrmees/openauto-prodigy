# External Data Provider API Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:executing-plans` to implement this plan task-by-task. Repository
> policy keeps one inline implementation owner by default; use
> `superpowers:subagent-driven-development` only if the user separately requests
> bounded delegation. Steps use checkbox (`- [ ]`) syntax for tracking.

**Status:** ACTIVE

**Design:**
`docs/plans/2026-08-02-external-data-provider-api-design.md`

**Grounded on:** `42e8277`

**Goal:** Implement Prodigy's source-agnostic typed-scalar provider registry,
additive External API messages, exact-channel delivery, and public
`prodigy.data` web-widget shim.

**Architecture:** A main-thread `DataRegistry` owns only live provider/channel
definitions and latest samples. `ApiDataBridge` translates the additive
protobuf domain, owns per-session provider/subscription/watch state, and fans
filtered events through the existing bounded `ApiSession` write path. The
public JavaScript shim exposes the same API to widgets and stamps samples with
a local monotonic receipt time; it receives no private QML or EventBus access.

**Tech Stack:** C++17, Qt 6.8 Core/Network/WebSockets, protobuf 3,
protobuf.js 7, plain browser JavaScript, Qt Test, CMake/CTest, Raspberry Pi 4.

## Global Constraints

- Work on `dev`; do not create a worktree unless the user asks for one.
- `proto/api/` is frozen additive-only: never reuse field numbers, rename
  messages, or silently change existing semantics.
- Do not edit `libs/prodigy-oaa-protocol/proto/`; this feature is unrelated to
  the Android Auto protocol submodule.
- Use capability presence (`Capabilities.data_provider_bridge`), never API
  minor comparison, for feature detection.
- Do not add `TOPIC_DATA`; exact channel subscriptions remain a separate API
  domain.
- Keep all registry and API state on the Qt main thread.
- Providers choose publication cadence. Do not throttle, coalesce, resample,
  request a rate, or feed subscription interest back to providers.
- Retain one latest sample per active channel only; add no history or
  persistence.
- Preserve provider timestamps as provenance/display data. Staleness uses
  local monotonic time since widget receipt, never wall-clock comparison.
- Treat authenticated providers as trusted. Existing frame and outbound queue
  caps remain the only performance protection rails.
- Keep every new web-widget capability available through the public protobuf
  API; add no QWebChannel, EventBus, D-Bus, or widget-only shortcut.
- Implement Prodigy only in this plan. Gauge Studio integration gets its own
  plan after this public surface is green. The empty `obd-plugin` directory is
  not initialized and no backend language is selected here.
- Execute tasks in order with one coherent commit per task. Do not push during
  execution.

---

## File and Interface Map

### Additive wire contract

- Create `proto/api/data.proto` with the exact enums/messages from design
  Sections 7–10 and 11.3.
- Modify `proto/api/api.proto` to import `api/data.proto`, allocate envelope
  fields 80–94, split the existing 80–99 reservation, and add optional
  `Capabilities.data_provider_bridge = 3`.
- Regenerate `resources/web/prodigy-proto.js` with
  `bash tools/gen-proto-js.sh`; do not hand-edit generated JavaScript.
- Modify `tests/test_api_proto_roundtrip.cpp` to pin scalar presence, exact
  64-bit values, optional field presence, capability presence, and every new
  envelope payload case.

The field allocation is fixed:

```text
80 register_data_provider_request       88 subscribe_data_channels_request
81 register_data_provider_response      89 subscribe_data_channels_response
82 declare_data_channels_request        90 unsubscribe_data_channels_request
83 declare_data_channels_response       91 data_values_event
84 remove_data_channels_request         92 watch_data_catalog_request
85 publish_data_values                  93 data_catalog_event
86 list_data_catalog_request            94 data_channel_availability_event
87 list_data_catalog_response
```

### Pure live registry

- Create `src/core/services/DataRegistry.hpp` and
  `src/core/services/DataRegistry.cpp`.
- Create `tests/test_data_registry.cpp`.
- Modify `src/CMakeLists.txt` and `tests/CMakeLists.txt` to compile both.

`DataRegistry.hpp` defines protobuf-independent value types in
`oap::data`. Keep fields named after the design so bridge conversion remains
mechanical:

```cpp
using OwnerToken = quintptr;

enum class ValueType { Unspecified, Double, SignedInteger, UnsignedInteger,
                       Boolean, String, Enum };
enum class Quality { Unknown, Good, Degraded, Stale, Invalid, Unavailable };
enum class UnavailableReason { None, ProviderAbsent, ChannelAbsent,
                               ProviderDisconnected, ChannelRemoved };

struct EnumScalar { qint64 value = 0; };
using Scalar = std::variant<double, qint64, quint64, bool, QString, EnumScalar>;

struct ChannelRef {
    QString providerNamespace;
    QString channelName;
    friend bool operator==(const ChannelRef& left, const ChannelRef& right) {
        return left.providerNamespace == right.providerNamespace
            && left.channelName == right.channelName;
    }
};

size_t qHash(const ChannelRef& ref, size_t seed = 0);

struct ProviderDefinition {
    QString providerNamespace;
    QString displayName;
    std::optional<QString> description;
    std::optional<QString> providerVersion;
};

struct EnumOption { qint64 value = 0; QString label; };
struct ChannelDefinition {
    QString channelName;
    QString displayName;
    ValueType valueType = ValueType::Unspecified;
    std::optional<QString> unit;
    std::optional<QString> description;
    std::optional<quint32> nominalIntervalMs;
    std::optional<quint32> staleAfterMs;
    std::optional<double> suggestedMinimum;
    std::optional<double> suggestedMaximum;
    QList<EnumOption> enumOptions;
};

struct Sample {
    QString channelName;
    std::optional<Scalar> value;
    std::optional<qint64> observedAtUnixMs;
    Quality quality = Quality::Unknown;
};
```

The registry public API is fixed for this plan:

```cpp
class DataRegistry final : public QObject {
    Q_OBJECT
public:
    RegistrationResult registerProvider(OwnerToken owner,
                                        const ProviderDefinition& definition);
    QList<DeclarationResult> declareChannels(
        OwnerToken owner, const QList<ChannelDefinition>& definitions);
    void removeChannels(OwnerToken owner, const QStringList& channelNames);
    PublishResult publish(OwnerToken owner, const QList<Sample>& samples);
    void removeOwner(OwnerToken owner);

    quint64 catalogRevision() const;
    Catalog catalog() const;
    std::optional<ChannelDefinition> definition(const ChannelRef& ref) const;
    std::optional<Sample> latestSample(const ChannelRef& ref) const;
    bool providerExists(const QString& providerNamespace) const;

signals:
    void catalogChanged(quint64 revision);
    void availabilityChanged(const oap::data::ChannelRef& ref,
                             bool available,
                             oap::data::UnavailableReason reason,
                             quint64 revision);
    void valuesAccepted(const QString& providerNamespace,
                        const QList<oap::data::Sample>& samples);
};
```

`RegistrationResult`, `DeclarationResult`, and `PublishResult` carry explicit
accepted/reason or accepted-samples/diagnostics data; callers never parse log
text. `Catalog` is a deterministic provider/channel snapshot sorted by the two
identifiers. Register all signal value types with Qt's metatype system.

### API bridge and session state

- Create `src/core/api/ApiDataBridge.hpp` and
  `src/core/api/ApiDataBridge.cpp`.
- Create `tests/test_api_data_bridge.cpp`.
- Modify `src/core/api/ApiRequestHandlers.hpp` and `.cpp` to delegate all data
  payloads to the bridge before the existing request switch.

The bridge interface is:

```cpp
class ApiDataBridge final : public QObject {
    Q_OBJECT
public:
    explicit ApiDataBridge(oap::data::DataRegistry* registry,
                           QObject* parent = nullptr);
    bool handleRequest(ApiSession* session, quint64 requestId,
                       const prodigy::api::v1::ApiMessage& message);
    void sessionClosed(ApiSession* session);
};
```

`handleRequest()` returns `true` for every recognized data-domain payload,
including malformed or directionally invalid ones it has already rejected.
It returns `false` only for non-data payloads so existing action,
notification, phone, and companion handling remains unchanged.

For each `ApiSession*`, the bridge stores:

```cpp
struct SessionState {
    QSet<oap::data::ChannelRef> subscriptions;
    bool watchesCatalog = false;
};
```

Provider ownership uses `reinterpret_cast<quintptr>(session)` only as an opaque
process-lifetime token. It is never serialized, logged as an identity, or
reused after `sessionClosed()` calls `DataRegistry::removeOwner()`.

All catalog, availability, and value fan-out snapshots destination sessions
before the first write, uses `QPointer<ApiSession>` for lifetime observation,
and immediately revalidates READY state plus watch/subscription membership
before `sendMessage(0, event)`. A synchronous slow-consumer teardown may mutate
session state and trigger nested provider cleanup without invalidating an
iterator.

### Composition and public web surface

- Modify `src/main.cpp` to create one app-lifetime `DataRegistry` before
  `ApiServer` and place it in `ApiServiceRefs::dataRegistry`.
- Modify `src/core/api/ApiServer.hpp` and `.cpp` to pass the registry into
  `ApiRequestHandlers` and advertise the capability when non-null.
- Modify `src/core/api/ApiSession.cpp` to report API minor 2.
- Modify `resources/web/prodigy.js` to expose `prodigy.data`.
- Create `tests/test_prodigy_data_js.mjs` as a dependency-free Node `vm`
  harness for the injected shim.
- Modify API server/session/loopback tests for capability and transport-level
  behavior.

The public JavaScript shape is:

```javascript
const catalog = await prodigy.data.listCatalog();
const unsubscribe = prodigy.data.subscribe(
  { providerNamespace: 'com.example.vehicle', channelName: 'engine.rpm' },
  callback,
);
```

Each sample callback preserves `timestampMs` and adds
`receivedAtMonotonicMs = performance.now()` before user callbacks execute.
Signed, unsigned, and enum 64-bit scalar values remain `bigint`; no implicit
`Number(...)` conversion is allowed. Active local bindings survive reconnect
and are re-subscribed after the next `ServerHello`; one-shot calls reject while
disconnected.

---

### Task 1: Add and freeze the protobuf surface

**Files:**

- Create: `proto/api/data.proto`
- Modify: `proto/api/api.proto`
- Modify: `tests/test_api_proto_roundtrip.cpp`
- Regenerate: `resources/web/prodigy-proto.js`
- Regenerate only if the generator changes it:
  `resources/web/protobuf.min.js`

**Consumes:** approved design Sections 7–11 and currently reserved capability
field 3/envelope fields 80–99.

**Produces:** compilable C++ and JavaScript bindings for the complete data
domain, capability field 3, and envelope payloads 80–94.

- [ ] **Step 1: Add failing protobuf contract assertions**

  Extend `tests/test_api_proto_roundtrip.cpp` with slots that instantiate all
  six scalar cases, preserve `INT64_MIN`, `INT64_MAX`, and `UINT64_MAX`,
  distinguish absent optional metadata from present empty strings/zeroes, and
  round-trip each new envelope field. Assert that an empty `DataScalar` has
  `VALUE_NOT_SET` and that a present empty `DataScalar` is still no value.

  ```cpp
  pb::DataScalar scalar;
  scalar.set_unsigned_integer_value(std::numeric_limits<quint64>::max());
  QVERIFY(scalar.SerializeToString(&bytes));
  pb::DataScalar parsed;
  QVERIFY(parsed.ParseFromString(bytes));
  QCOMPARE(parsed.unsigned_integer_value(),
           std::numeric_limits<quint64>::max());
  ```

- [ ] **Step 2: Run the proto test and confirm compile failure**

  Run:

  ```bash
  cmake --build ~/builds/openauto-prodigy --target test_api_proto_roundtrip -j$(nproc)
  ```

  Expected: compilation fails because `DataScalar`, the capability accessor,
  and new envelope accessors do not exist.

- [ ] **Step 3: Add `data.proto` and allocate the envelope fields**

  Copy the approved enum/message shapes exactly from design Sections 7–10 and
  11.3. Add `optional bool data_provider_bridge = 3` to `Capabilities`. Import
  `api/data.proto`, assign fields 80–94 exactly as listed in this plan, and
  change the final reservation to `reserved 95 to 99`; leave every pre-existing
  number and declaration untouched.

- [ ] **Step 4: Reconfigure, regenerate JavaScript, and run the focused test**

  Run:

  ```bash
  cmake -S . -B ~/builds/openauto-prodigy
  bash tools/gen-proto-js.sh
  cmake --build ~/builds/openauto-prodigy --target test_api_proto_roundtrip -j$(nproc)
  ~/builds/openauto-prodigy/tests/test_api_proto_roundtrip
  ```

  Expected: configure discovers `data.proto`; generation succeeds; the focused
  test exits 0.

- [ ] **Step 5: Verify generated bindings and commit**

  Run:

  ```bash
  rg -n "dataProviderBridge|registerDataProviderRequest|dataValuesEvent" \
    resources/web/prodigy-proto.js
  git diff --check
  git add proto/api/data.proto proto/api/api.proto \
    resources/web/prodigy-proto.js resources/web/protobuf.min.js \
    tests/test_api_proto_roundtrip.cpp
  git commit -m "feat(api): define external data provider protocol"
  ```

  Expected: all three generated symbols exist and no whitespace errors appear.

### Task 2: Implement the pure live `DataRegistry`

**Files:**

- Create: `src/core/services/DataRegistry.hpp`
- Create: `src/core/services/DataRegistry.cpp`
- Create: `tests/test_data_registry.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Consumes:** the internal types and public API in the File and Interface Map.

**Produces:** one main-thread live registry with deterministic catalog,
session-owned cleanup, last-value retention, and typed publication validation.

- [ ] **Step 1: Write registry lifecycle tests**

  Cover namespace/channel grammar, first-owner wins, same-owner idempotence,
  namespace-switch rejection, incremental declarations, partial declaration
  success, metadata-only revision changes, unchanged re-declaration, active
  type-change rejection, channel removal, and atomic owner cleanup.

  ```cpp
  QCOMPARE(registry.registerProvider(1, provider("com.example.vehicle")).accepted,
           true);
  QCOMPARE(registry.registerProvider(2, provider("com.example.vehicle")).accepted,
           false);
  QCOMPARE(registry.catalogRevision(), quint64(1));
  ```

- [ ] **Step 2: Write typed publication and ordering tests**

  Cover all scalar types, usable quality without value, valueless terminal
  qualities, unknown channels, type mismatch, wall-clock receipt fill through
  an injectable `nowUnixMs` test seam, latest-value replacement, and duplicate
  reduction. For `[a1, b1, a2]`, assert accepted order `[b1, a2]`; if `a2` is
  invalid, assert only `b1` publishes and the old retained `a` remains.

- [ ] **Step 3: Run the registry target and confirm failure**

  Run:

  ```bash
  cmake --build ~/builds/openauto-prodigy --target test_data_registry -j$(nproc)
  ```

  Expected: configure or compile fails because the target and registry do not
  exist.

- [ ] **Step 4: Implement the smallest registry satisfying the tests**

  Use `QHash<QString, ProviderState>` keyed by namespace and a second owner to
  namespace map. Keep `ProviderState.channels` and retained samples private.
  Deduplicate with the index of each channel's last occurrence, sort winners by
  that index, then validate. Increment the catalog revision exactly once per
  public mutation that actually changes catalog state, including multi-channel
  requests and owner teardown.

- [ ] **Step 5: Register build targets and run focused tests**

  Add both source files to `openauto-core`, add `test_data_registry` through
  `oap_add_test`, reconfigure, build, and run:

  ```bash
  cmake -S . -B ~/builds/openauto-prodigy
  cmake --build ~/builds/openauto-prodigy --target test_data_registry -j$(nproc)
  ~/builds/openauto-prodigy/tests/test_data_registry
  ```

  Expected: all registry cases pass.

- [ ] **Step 6: Commit the pure registry slice**

  ```bash
  git add src/core/services/DataRegistry.hpp src/core/services/DataRegistry.cpp \
    src/CMakeLists.txt tests/test_data_registry.cpp tests/CMakeLists.txt
  git commit -m "feat(data): add live typed scalar registry"
  ```

### Task 3: Implement provider commands and catalog discovery

**Files:**

- Create: `src/core/api/ApiDataBridge.hpp`
- Create: `src/core/api/ApiDataBridge.cpp`
- Create: `tests/test_api_data_bridge.cpp`
- Modify: `src/core/api/ApiRequestHandlers.hpp`
- Modify: `src/core/api/ApiRequestHandlers.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Consumes:** `DataRegistry`, generated data protobuf types, public
`ApiSession::sendMessage()`/`closeWithError()`, and existing request sink.

**Produces:** registration, declaration, removal, publication, catalog list,
catalog watch, request-ID validation, and provider cleanup through the existing
session lifecycle.

- [ ] **Step 1: Write provider request tests against real `ApiSession` fakes**

  Assert typed registration/declaration responses echo nonzero IDs; declaration
  before registration returns per-channel `provider not registered`; same-owner
  repeat is idempotent; a namespace switch is rejected; removal returns `Ack`;
  publication with ID 0 sends no response; publication with nonzero ID is
  dropped and logged without disconnect; and teardown removes the provider.

- [ ] **Step 2: Write catalog list/watch and ID-misuse tests**

  Assert deterministic full snapshots, `Ack` before the initial watch event,
  one full event per real revision, idempotent disable, and no event after
  disable. Every response-bearing data request with ID 0 must receive
  `Error{INVALID_REQUEST}` ID 0 and terminate the session.

- [ ] **Step 3: Run the bridge target and confirm failure**

  Run:

  ```bash
  cmake --build ~/builds/openauto-prodigy --target test_api_data_bridge -j$(nproc)
  ```

  Expected: target or compilation failure because `ApiDataBridge` is absent.

- [ ] **Step 4: Implement protobuf/internal conversion and provider routing**

  Keep conversion helpers private to `ApiDataBridge.cpp`. Validate protobuf
  enum values and oneof presence before constructing internal types. Map
  `UNSPECIFIED` quality to `Quality::Unknown`; treat absent `value` and an empty
  `DataScalar` identically. Log dropped samples with provider namespace and
  channel name, but never emit a publication response.

- [ ] **Step 5: Implement catalog watching and safe fan-out**

  Store watch state per session. Send `Ack(requestId)` before the initial
  `DataCatalogEvent(0)`. On `catalogChanged`, snapshot `QPointer<ApiSession>`
  destinations and revalidate each watcher immediately before the write.
  Metadata churn intentionally sends every full revision without throttling.

- [ ] **Step 6: Delegate from the existing request sink and run tests**

  Construct `ApiDataBridge` from `ApiRequestHandlers::Deps::dataRegistry`.
  At the start of `handleRequest`, return when the bridge reports handled. In
  `sessionClosed`, call the bridge before clearing existing action/notification
  ownership. Reconfigure, build, and run:

  ```bash
  cmake -S . -B ~/builds/openauto-prodigy
  cmake --build ~/builds/openauto-prodigy --target test_api_data_bridge \
    test_api_request_handlers -j$(nproc)
  ~/builds/openauto-prodigy/tests/test_api_data_bridge
  QT_QPA_PLATFORM=offscreen \
    ~/builds/openauto-prodigy/tests/test_api_request_handlers
  ```

  Expected: new data tests and all existing request-handler tests pass.

- [ ] **Step 7: Commit the provider/catalog slice**

  ```bash
  git add src/core/api/ApiDataBridge.hpp src/core/api/ApiDataBridge.cpp \
    src/core/api/ApiRequestHandlers.hpp src/core/api/ApiRequestHandlers.cpp \
    src/CMakeLists.txt tests/test_api_data_bridge.cpp tests/CMakeLists.txt
  git commit -m "feat(api): bridge data providers and catalog"
  ```

### Task 4: Add exact-channel subscriptions and lifecycle delivery

**Files:**

- Modify: `src/core/api/ApiDataBridge.hpp`
- Modify: `src/core/api/ApiDataBridge.cpp`
- Modify: `tests/test_api_data_bridge.cpp`

**Consumes:** provider/catalog bridge from Task 3 and registry availability and
value signals.

**Produces:** waiting subscriptions, response-before-snapshot ordering,
metadata compatibility boundaries, exact filtering, retained snapshots,
disconnect/removal/recovery events, and idempotent unsubscribe.

- [ ] **Step 1: Add waiting and snapshot-order tests**

  Subscribe to an absent provider and assert accepted response then
  `PROVIDER_ABSENT`. Register the provider and assert `CHANNEL_ABSENT`; declare
  the channel and assert `AVAILABLE` before values. Subscribe after a retained
  sample and assert response, availability, then exactly one value event.

- [ ] **Step 2: Add exact filtering and batch-normalization tests**

  Use two providers and three channels. Assert each consumer receives only its
  exact references. Publish `[a1, b1, a2]` and assert one event containing the
  subscribed winners in last-occurrence order, never both `a` entries.

- [ ] **Step 3: Add lifecycle, reentrancy, and slow-consumer tests**

  Cover metadata-only `AVAILABLE` before the next value, explicit removal,
  provider disconnect, same-namespace recovery without consumer retry,
  idempotent exact unsubscribe, consumer teardown, and a fan-out where the
  first destination tears down synchronously while later destinations still
  receive their event.

- [ ] **Step 4: Run focused tests and confirm failure**

  Run:

  ```bash
  cmake --build ~/builds/openauto-prodigy --target test_api_data_bridge -j$(nproc)
  ~/builds/openauto-prodigy/tests/test_api_data_bridge
  ```

  Expected: new subscription assertions fail because delivery is absent.

- [ ] **Step 5: Implement session subscriptions and fan-out**

  Accept every syntactically valid `ChannelRef`, even when absent. Reject only
  invalid grammar per result. On repeat subscribe, resend current availability
  and retained value. Filter each `valuesAccepted` signal into at most one
  `DataValuesEvent` per destination for that provider publication. Use the
  immutable snapshot/revalidation rule from the File and Interface Map for
  availability and value events as well as catalog events.

- [ ] **Step 6: Run focused tests and commit**

  ```bash
  cmake --build ~/builds/openauto-prodigy --target test_api_data_bridge -j$(nproc)
  ~/builds/openauto-prodigy/tests/test_api_data_bridge
  git add src/core/api/ApiDataBridge.hpp src/core/api/ApiDataBridge.cpp \
    tests/test_api_data_bridge.cpp
  git commit -m "feat(api): stream exact external data channels"
  ```

  Expected: all exact-channel and teardown cases pass.

### Task 5: Compose the registry into the live TCP/WebSocket server

**Files:**

- Modify: `src/main.cpp`
- Modify: `src/core/api/ApiServer.hpp`
- Modify: `src/core/api/ApiServer.cpp`
- Modify: `src/core/api/ApiSession.cpp`
- Modify: `tests/test_api_session.cpp`
- Modify: `tests/test_api_server.cpp`
- Modify: `tests/test_api_loopback.cpp`

**Consumes:** registry and bridge implemented in Tasks 2–4.

**Produces:** API minor 2, truthful `data_provider_bridge` capability, correct
app-lifetime ownership, and identical TCP/WebSocket behavior.

- [ ] **Step 1: Add capability and composition tests**

  Update the session expectation from minor 1 to minor 2. In server tests,
  assert capability field 3 is present/true when `ApiServiceRefs.dataRegistry`
  is non-null and absent when null. Assert ordinary topic capability behavior
  is unchanged.

- [ ] **Step 2: Add TCP and WebSocket loopback flows**

  Over TCP, connect a provider and consumer, register/declare, subscribe,
  publish, remove, disconnect, reconnect, and verify exact response/event
  order. Repeat a smaller register/subscribe/publish flow over WebSocket.
  Include an outbound-cap case proving one slow consumer does not stop the
  provider or another consumer.

- [ ] **Step 3: Run the server targets and confirm failure**

  Run:

  ```bash
  cmake --build ~/builds/openauto-prodigy --target test_api_session \
    test_api_server test_api_loopback -j$(nproc)
  QT_QPA_PLATFORM=offscreen ~/builds/openauto-prodigy/tests/test_api_session
  QT_QPA_PLATFORM=offscreen ~/builds/openauto-prodigy/tests/test_api_server
  QT_QPA_PLATFORM=offscreen ~/builds/openauto-prodigy/tests/test_api_loopback
  ```

  Expected: new minor/capability and data flow assertions fail.

- [ ] **Step 4: Wire the composition root and capability**

  Create `DataRegistry` in `main.cpp` immediately before filling
  `ApiServiceRefs`, parent it to `&app`, and assign the pointer. Add the nullable
  ref to `ApiServiceRefs`; pass it into request-handler dependencies during the
  `ApiServer` constructor. Set capability field 3 only for a non-null registry,
  and set `ServerHello.api_version_minor` to 2.

- [ ] **Step 5: Run all API-focused tests and commit**

  ```bash
  cmake --build ~/builds/openauto-prodigy --target test_api_proto_roundtrip \
    test_data_registry test_api_data_bridge test_api_session \
    test_api_request_handlers test_api_server test_api_loopback -j$(nproc)
  ctest --test-dir ~/builds/openauto-prodigy --output-on-failure \
    -R 'test_(api|data_registry)'
  git add src/main.cpp src/core/api/ApiServer.hpp src/core/api/ApiServer.cpp \
    src/core/api/ApiSession.cpp tests/test_api_session.cpp \
    tests/test_api_server.cpp tests/test_api_loopback.cpp
  git commit -m "feat(api): enable live external data providers"
  ```

  Expected: every API/data target passes.

### Task 6: Expose the public `prodigy.data` widget shim

**Files:**

- Modify: `resources/web/prodigy.js`
- Create: `tests/test_prodigy_data_js.mjs`
- Modify: `tests/CMakeLists.txt` only if `node` is found at configure time

**Consumes:** regenerated protobuf JavaScript and live server messages.

**Produces:** capability-gated catalog discovery, shared exact subscriptions,
exact scalar mapping, monotonic receipt timestamps, immediate disconnect
unavailability, and reconnect restoration.

- [ ] **Step 1: Build a deterministic Node `vm` harness**

  Load `resources/web/prodigy.js` into a context with fake `window`,
  `performance.now`, timers, protobuf root, and `WebSocket`. Capture encoded
  plain objects rather than depending on a network. Drive `ServerHello`,
  responses, availability events, value events, close, and reconnect through
  the fake socket.

- [ ] **Step 2: Add failing public-contract tests**

  Assert `prodigy.data` is absent when the capability is absent and exposes
  `listCatalog`/`subscribe` when present. Verify one server subscription for
  multiple local callbacks, exact unsubscribe when the last callback leaves,
  response-before-event handling, waiting/available/value/unavailable callback
  sequences, and reconnect resubscription.

- [ ] **Step 3: Add scalar and clock tests**

  Assert doubles remain `number`; signed, unsigned, and enum values remain
  `bigint`; enum labels resolve from the current definition; and
  `observed_at_unix_ms` becomes numeric `timestampMs`. Set wall time backward
  and forward while advancing fake monotonic time and assert
  `receivedAtMonotonicMs` follows only the latter.

  ```javascript
  assert.equal(typeof sample.value, 'bigint');
  assert.equal(sample.timestampMs, 1_722_000_000_000);
  assert.equal(sample.receivedAtMonotonicMs, 250);
  ```

- [ ] **Step 4: Run the JS test and confirm failure**

  Run:

  ```bash
  node tests/test_prodigy_data_js.mjs
  ```

  Expected: failure because `prodigy.data` is undefined.

- [ ] **Step 5: Implement the shim without changing existing APIs**

  Keep ordinary topic subscriptions intact. Parse capability presence from
  each `ServerHello`; route data stream events before the generic status
  handler. Store data bindings separately from topic subscriptions. Reject
  one-shot requests during a reconnect gap, mark live bindings unavailable on
  close, and reissue exact subscriptions only after the next `ServerHello`.

- [ ] **Step 6: Run shim and widget regression checks**

  ```bash
  node tests/test_prodigy_data_js.mjs
  cmake --build ~/builds/openauto-prodigy --target test_web_widget_scanner \
    test_widget_contract_qml -j$(nproc)
  QT_QPA_PLATFORM=offscreen \
    ~/builds/openauto-prodigy/tests/test_web_widget_scanner
  QT_QPA_PLATFORM=offscreen \
    ~/builds/openauto-prodigy/tests/test_widget_contract_qml
  ```

  Expected: shim and existing widget-host tests pass.

- [ ] **Step 7: Register the optional Node test and commit**

  If `find_program(NODE_EXECUTABLE node)` succeeds, add the test to CTest; do
  not make runtime installation depend on Node. Then run the configured test
  once and commit:

  ```bash
  cmake -S . -B ~/builds/openauto-prodigy
  ctest --test-dir ~/builds/openauto-prodigy --output-on-failure \
    -R test_prodigy_data_js
  git add resources/web/prodigy.js tests/test_prodigy_data_js.mjs \
    tests/CMakeLists.txt
  git commit -m "feat(widgets): expose external data subscriptions"
  ```

### Task 7: Document, verify, review, and hand off cross-repository work

**Files:**

- Modify: `docs/architecture.md`
- Modify: `docs/reference/web-widget-authoring.md`
- Modify: `docs/roadmap-current.md`
- Modify: `docs/INDEX.md`
- Modify: `docs/session-handoffs.md`
- Move on completion:
  `docs/plans/2026-08-02-external-data-provider-api-design.md`
- Move on completion:
  `docs/plans/2026-08-02-external-data-provider-api-implementation-plan.md`

**Consumes:** green implementation from Tasks 1–6.

**Produces:** current public API documentation, complete native/ARM evidence,
one bounded major-work review, and an exact handoff for Gauge Studio and the
future backend repository.

- [ ] **Step 1: Update public and architecture documentation**

  Document provider registration, typed declarations, request/event ordering,
  exact subscriptions, reconnect, 64-bit JavaScript values, and monotonic
  staleness. State the half-open limitation and that the server never requests
  cadence. Keep OBD/CAN examples explicitly illustrative rather than semantic
  API names.

- [ ] **Step 2: Run the complete native and documentation gate**

  Run:

  ```bash
  cmake -S . -B ~/builds/openauto-prodigy
  cmake --build ~/builds/openauto-prodigy -j$(nproc)
  cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j$(nproc)
  ctest --test-dir ~/builds/openauto-prodigy --output-on-failure
  node tests/test_prodigy_data_js.mjs
  python3 scripts/check-doc-links.py --scope tracked-live
  git diff --check
  ```

  Expected: configure/build succeed and every test/check exits 0.

- [ ] **Step 3: Cross-build the Pi artifact**

  Run:

  ```bash
  ./cross-build.sh
  ```

  Expected: the ARM `openauto-prodigy` target succeeds with embedded regenerated
  protobuf/shim resources. Do not deploy until the cross-repository live gauge
  fixture exists or Matthew explicitly requests an API-only deployment.

- [ ] **Step 4: Run the single major-work review gate**

  Use the immutable commit created for this plan as the review base:

  ```bash
  review_base=$(git log -1 --format=%H \
    --grep='^docs: plan external data provider implementation$')
  test -n "$review_base"
  bash scripts/review-gate.sh --author codex --major --base "$review_base"
  ```

  Adjudicate every finding under repository policy. One remediation review is
  the maximum without explicit user authorization. Record confirmed,
  dismissed, and deferred counts in the handoff.

- [ ] **Step 5: Create the Gauge Studio implementation plan**

  In `/mnt/e/claude/personal/openautopro/gauges`, write a repository-local plan
  that consumes only `window.prodigy.data`. It must cover the
  `providerNamespace`/`channelName`/pinned-type/pinned-unit document migration,
  catalog selector, `prodigy-adapter.js`, finite positive stale policy,
  monotonic runtime timer, export bundling, and Node tests. It must not copy
  protobuf transport code or depend on Prodigy C++ headers.

- [ ] **Step 6: Record the backend bootstrap decision as still open**

  In the handoff, record that `/mnt/e/claude/personal/openautopro/obd-plugin`
  was empty at planning time. Its later repository plan must choose language,
  packaging, OBD/CAN libraries, and hardware ownership separately while
  conforming to this public contract. Do not rename the generic API after that
  backend's source semantics.

- [ ] **Step 7: Close and archive this Prodigy plan after green review**

  Set the design and plan status to `COMPLETED 2026-08-02` (or the actual
  completion date), move both to `docs/archive/plans/`, update index/roadmap
  links, append the final evidence to `docs/session-handoffs.md`, and commit the
  coherent documentation closure. Do not push until the user authorizes it.

---

## Completion Criteria

- All additive protobuf fields and capability presence match the approved
  design exactly.
- A provider can register, declare, publish, remove, disconnect, and reconnect
  over both TCP and WebSocket without source-specific semantics.
- A consumer can wait on an absent exact channel, receive compatibility
  metadata before values, recover after provider restart, and unsubscribe
  without affecting anyone else.
- Duplicate publications, request-ID misuse, catalog churn, slow consumers,
  nested teardown, and half-open-provider presentation behavior have direct
  test coverage.
- `prodigy.data` preserves exact 64-bit values and bases freshness only on its
  browser-monotonic receipt stamp.
- Native build, explicit app target, complete CTest, JS shim test, docs checks,
  ARM cross-build, and the bounded major review are green.
- Gauge Studio has a separate executable integration plan; the empty backend
  directory remains uninitialized until its own technology decision.
