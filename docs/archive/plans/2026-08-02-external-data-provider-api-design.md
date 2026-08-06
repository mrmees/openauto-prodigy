# External Data Provider API Design

Date: 2026-08-02
Status: COMPLETED 2026-08-02
Grounded against: `5a7a3b559b6e336272e01211c5a74eed7320cb11`
Independent reviews: Opus 4.6, `APPROVE WITH CHANGES` — BLOCKER 0,
MAJOR 2, MINOR 6; all eight findings confirmed and incorporated. A subsequent
user-supplied Fable review produced five contract tightenings, all incorporated;
its namespace-squatting observation was already explicitly dispositioned.

## 1. Purpose

Define one implementation-ready, source-agnostic contract through which an
external backend can announce typed scalar data and publish live values to
OpenAuto Prodigy consumers, including exported Gauge Studio web widgets.

The contract serves three independently developed repositories:

```text
external backend -> Prodigy External API -> Gauge Studio widget
```

The backend decides what its data means and how often it publishes. Prodigy
provides authenticated transport, live discovery, exact-channel subscription,
latest-value snapshots, lifecycle signaling, and bounded socket queues. It
does not understand or arbitrate OBD-II, CAN, MQTT, GPIO, GNSS, vehicle, home,
or other source semantics.

This is a generic external data-provider bridge. Calling it an OBD or vehicle
telemetry API would incorrectly narrow its contract.

## 2. Scope

### 2.1 In scope

- Session-owned external provider registration.
- A backend-supplied provider namespace and descriptive metadata.
- Incremental declaration and removal of named typed-scalar channels.
- Live catalog discovery and optional catalog watching.
- Fire-and-forget publication of scalar value batches at backend-selected
  cadence.
- Exact `{provider_namespace, channel_name}` subscriptions.
- Subscriptions that remain waiting while a provider or channel is absent.
- Immediate latest-value delivery for active channels.
- Explicit channel availability, removal, disconnect, and recovery events.
- Data quality, measurement timestamp, nominal interval, and freshness
  metadata.
- Public WebSocket shim methods that are convenience wrappers over the same
  protobuf API available to every other client.
- A clean Gauge Studio binding and normalized-sample mapping.
- A backend conformance contract independent of backend language or hardware.

### 2.2 Out of scope

- OBD-II PID selection, polling, parsing, or adapter management.
- CAN interface configuration, frame decoding, or DBC handling.
- Any other source-specific transport or hardware policy.
- Prodigy-owned semantic variable names or a controlled unit vocabulary.
- Cross-provider arbitration, priority, fallback, aliasing, or deduplication.
- Server-side unit conversion, formulas, derived values, range enforcement,
  interpolation, throttling, or resampling.
- Historical storage, replay, logging databases, or delivery across a
  connection loss.
- Provider commands, writable channels, acknowledgements for physical
  actions, or widget-to-provider mutation.
- Widget-to-widget communication.
- Subscription-interest feedback to providers.
- Per-provider roles, ACLs, quotas, or rate limits.
- Android Auto sensor publication. A later AA bridge may consume selected
  registry values through its own separately approved semantic provider.

## 3. Governing Decisions and Invariants

1. **One external surface.** This domain is additive growth of the existing
   External API protobuf over TCP and WebSocket. It adds no listener, port,
   authentication system, QWebChannel surface, or widget-only shortcut.
2. **Generic data, not source semantics.** Provider and channel identities are
   backend-defined. Prodigy never exposes or interprets PIDs, CAN frames,
   D-Bus paths, EventBus topics, or AA protocol objects.
3. **Typed scalars only.** Values are double, signed integer, unsigned integer,
   boolean, string, or enum. Strings are displayable scalar text, not an escape
   hatch for JSON documents or binary blobs.
4. **Provider cadence wins.** Prodigy forwards every valid publication without
   intentional throttling or coalescing. Consumers do not request a rate.
5. **Exact subscriptions.** Consumers subscribe to explicit composite channel
   identities. There are no wildcard, prefix, expression, or topic-wide value
   subscriptions in v1.
6. **Boot order is irrelevant.** A syntactically valid subscription is accepted
   even when its provider or channel is absent. It becomes live automatically
   when that identity appears.
7. **Live registrations only.** Provider and channel registrations are owned by
   the registering API session and are never persisted by Prodigy.
8. **Latest state, not a message log.** Prodigy retains only the newest sample
   for each active channel, solely for snapshot-on-subscribe.
9. **Consumers pin compatibility.** A gauge records the value type and unit it
   was designed for. If a later channel definition disagrees, the gauge shows
   incompatible/unavailable rather than rendering a misleading value.
10. **Trusted-client performance model.** Authenticated providers are trusted
    not to overwhelm the Pi. Existing frame and outbound-queue bounds remain
    crash-protection rails; Prodigy does not attempt to guess an appropriate
    channel count or publication rate.
11. **Frozen-additive evolution.** Existing `proto/api/` messages, fields,
    numbers, and semantics remain unchanged. This design consumes only selected
    reserved fields and adds an explicit capability flag.

## 4. Terminology

| Term | Meaning |
|---|---|
| Provider | One external API session that has successfully registered a provider namespace. |
| Provider namespace | Backend-chosen, active-session-unique identifier such as `com.example.vehicle`. |
| Channel name | Backend-chosen identifier unique within its provider, such as `engine.rpm`. |
| Channel reference | Composite `{provider_namespace, channel_name}` identity. |
| Channel definition | Live metadata declaring a channel's name, scalar type, unit, labels, cadence hints, and optional display hints. |
| Sample | One channel's typed value, timestamp, and quality. |
| Catalog | Snapshot of all currently connected providers and their declared channels. |
| Waiting subscription | Accepted consumer interest in a channel reference that is currently absent. |

## 5. Architecture

```text
External data backend
  |  ClientHello/auth + provider registration + channel declarations
  |  PublishDataValues at backend-selected cadence
  v
External API session -> DataRegistry <- External API consumer sessions
                         |    |          | exact channel subscriptions
                         |    |          | values + availability events
                         |    + latest sample per active channel
                         + live provider/channel catalog
                                           |
                                           v
                                 public prodigy.data shim
                                           |
                                           v
                               exported Gauge Studio widget
```

`DataRegistry` is a core service and the sole owner of live provider/channel
state. API handlers feed it explicit provider reports and bind consumers to its
typed signals. The External API does not subscribe to EventBus topics or source
internals. The existing `TopicPublisher` full-domain-snapshot path is not used:
dynamic channel values require exact subscriptions, and a generic full
`TOPIC_DATA` snapshot would multiply unrelated high-rate traffic across every
widget connection.

All registry and API work remains on the Qt main thread. No worker, extra event
loop, or cross-thread mutable state is introduced. If later profiling shows
serialization to be material, that work requires its own design; state access
does not move implicitly.

## 6. Session, Authentication, and Provider Ownership

An external backend uses the existing API handshake, pairing, and admission
rules. A typical backend sends `ClientHello` with
`CLIENT_KIND_THIRD_PARTY`. Localhost remains trusted; a remote backend must be
paired and admitted by the existing network policy.

After the session reaches READY, it may send `RegisterDataProviderRequest`.
One API session may own at most one provider namespace. Registering a provider
does not prevent that session from using other ordinary External API consumer
operations.

Provider namespace rules:

- Backend-supplied and stable across reconnects.
- Lowercase ASCII, 1-128 characters.
- Grammar: `^[a-z0-9][a-z0-9._-]{0,127}$`.
- Unique among currently connected provider sessions.
- First active registration owns the namespace until explicit session teardown.
- A competing active registration is rejected with a reason; it does not steal
  or merge the existing provider.

Provider display name, description, and version are descriptive strings for
catalogs and logs. They do not affect identity or capabilities.

Repeating registration of the same namespace from its owning session is
idempotent. If its descriptive provider metadata changed, the live definition
is updated and the catalog revision advances. The same session cannot switch
to a different provider namespace without disconnecting and establishing a new
session.

On API-session teardown, `DataRegistry` atomically:

1. marks all subscribed channel references from that provider unavailable;
2. removes its retained samples and channel definitions;
3. removes the provider from the live catalog and advances its revision; and
4. releases the namespace for a future registration.

The teardown transition advances the catalog revision before emitting public
availability/catalog events; every event from that transition carries or
contains the resulting revision.

When a later session registers the same namespace and re-declares a channel,
waiting consumer subscriptions automatically attach to it.

The existing External API `Ping`/`Pong` is client-initiated in v1; the server
does not probe an otherwise idle provider. Provider teardown therefore occurs
only after the transport reports closure. A dead peer behind a half-open TCP
session can remain catalog-available until the socket eventually fails. This
is an explicit v1 limitation: receipt-monotonic consumer staleness is the UI
defense, and server-initiated provider liveness probing is deferred rather than
guessing a safe timeout for arbitrary publication cadences.

## 7. Channel Identity and Definitions

A channel name follows the same lowercase ASCII grammar and 1-128 character
bound as a provider namespace. It is unique only within its provider.

Examples:

```text
com.example.vehicle / engine.rpm
com.example.vehicle / engine.coolant-temperature
home.camper        / fresh-water.percent
shop.air-system    / compressor.running
```

The proposed definition is:

```protobuf
enum DataValueType {
  DATA_VALUE_TYPE_UNSPECIFIED = 0;
  DATA_VALUE_TYPE_DOUBLE = 1;
  DATA_VALUE_TYPE_SIGNED_INTEGER = 2;
  DATA_VALUE_TYPE_UNSIGNED_INTEGER = 3;
  DATA_VALUE_TYPE_BOOLEAN = 4;
  DATA_VALUE_TYPE_STRING = 5;
  DATA_VALUE_TYPE_ENUM = 6;
}

message DataEnumOption {
  sint64 value = 1;
  string label = 2;
}

message DataChannelDefinition {
  string channel_name = 1;
  string display_name = 2;
  DataValueType value_type = 3;
  optional string unit = 4;
  optional string description = 5;
  optional uint32 nominal_interval_ms = 6;
  optional uint32 stale_after_ms = 7;
  optional double suggested_minimum = 8;
  optional double suggested_maximum = 9;
  repeated DataEnumOption enum_options = 10;
  reserved 11 to 20;
}
```

Metadata contract:

- `value_type` is required and cannot be `UNSPECIFIED`.
- `unit` is backend-defined opaque metadata. Prodigy neither validates a
  vocabulary nor converts it.
- `nominal_interval_ms` describes expected publication cadence; Prodigy does
  not enforce it.
- `stale_after_ms` suggests when an otherwise usable last sample should be
  presented as stale. It is evaluated against monotonic elapsed time since
  local sample receipt, never against `observed_at_unix_ms`; Prodigy does not
  schedule a stale publication.
- Suggested bounds are optional authoring hints. They are not clamps or safety
  limits.
- Enum option labels are presentation hints. An enum sample whose numeric
  value is absent from the current table remains a valid sample and may be
  displayed numerically.
- Non-enum channels ignore `enum_options`.

Channel declaration is incremental and idempotent. A provider may declare new
channels after startup. Re-declaring a channel with the same value type updates
its descriptive metadata, advances the catalog revision when the definition
actually changes, and notifies catalog watchers and bound subscribers of the
new definition. Changing an active channel's `value_type` is rejected; the
provider must remove and re-declare that channel, producing an explicit
unavailable/available transition. A type may differ after a full provider
disconnect because no registration survives that boundary; consumers still
protect themselves with pinned compatibility metadata.

Removing a channel deletes its retained sample, advances the catalog revision,
and immediately marks it unavailable to bound consumers. Their subscriptions
remain waiting in case the provider declares it again. Removing an unknown
channel is an idempotent no-op and still receives `Ack`.

## 8. Typed Scalars and Publication

```protobuf
message DataScalar {
  oneof value {
    double double_value = 1;
    sint64 signed_integer_value = 2;
    uint64 unsigned_integer_value = 3;
    bool boolean_value = 4;
    string string_value = 5;
    sint64 enum_value = 6;
  }
  reserved 7 to 15;
}

enum DataQuality {
  DATA_QUALITY_UNSPECIFIED = 0;
  DATA_QUALITY_GOOD = 1;
  DATA_QUALITY_DEGRADED = 2;
  DATA_QUALITY_STALE = 3;
  DATA_QUALITY_INVALID = 4;
  DATA_QUALITY_UNAVAILABLE = 5;
}

message DataSample {
  string channel_name = 1;
  DataScalar value = 2;
  optional int64 observed_at_unix_ms = 3;
  DataQuality quality = 4;
  reserved 5 to 12;
}

message PublishDataValues {
  repeated DataSample samples = 1;
}
```

Publication rules:

- `PublishDataValues` is READY-state, provider-to-server, `request_id = 0`,
  fire-and-forget traffic. It produces no per-batch acknowledgement.
- A provider must register and declare a channel before publishing it.
- Each scalar oneof case must match the channel's declared `value_type`.
- `UNSPECIFIED` quality maps to the consumer-facing `unknown` state; it is not
  silently promoted to `good`.
- `INVALID`, `STALE`, or `UNAVAILABLE` samples may omit `value`. A usable
  quality with no value is invalid and is dropped.
- If `observed_at_unix_ms` is absent, Prodigy fills it with wall-clock receipt
  time before retaining or forwarding the sample. If supplied, the backend is
  responsible for normalizing its source clock to Unix epoch milliseconds.
  This timestamp is provenance/display data only; no consumer may use it to
  decide staleness because either machine's wall clock may be unsynchronized.
- Multiple channels captured together may be batched. Batches must fit the
  existing API frame cap.
- A publication is deduplicated before validation, retention, and fan-out. If
  one channel occurs more than once, only its last occurrence is considered;
  the winners remain ordered by the position of their last occurrence in the
  inbound batch. If that winning occurrence is invalid, the channel is not
  updated even if an earlier duplicate was valid. Other channels remain
  unaffected.
- Undeclared channels, scalar-type mismatches, and malformed samples are
  dropped and logged with provider namespace and channel identity. One bad
  sample does not discard valid siblings or disconnect the provider.
- Accepted, deduplicated samples replace their channel's retained latest value,
  then are forwarded once per channel to subscribed consumers in
  provider-session arrival order.
- Prodigy performs no cadence reduction, timer-based republish, interpolation,
  conversion, formula evaluation, or persistence.

The protobuf schema retains exact signed and unsigned 64-bit integers. A
JavaScript consumer must not silently round a value outside JavaScript's safe
integer range. The public shim preserves such values in an exact representation
and Gauge Studio treats an integer that its numeric renderer cannot represent
exactly as incompatible rather than converting it lossy.

## 9. Catalog Discovery

```protobuf
message DataProviderDefinition {
  string provider_namespace = 1;
  string display_name = 2;
  optional string description = 3;
  optional string provider_version = 4;
  reserved 5 to 12;
}

message DataProviderCatalog {
  DataProviderDefinition provider = 1;
  repeated DataChannelDefinition channels = 2;
}

message DataCatalog {
  uint64 catalog_revision = 1;
  repeated DataProviderCatalog providers = 2;
  reserved 3 to 8;
}

message ListDataCatalogRequest {}
message ListDataCatalogResponse { DataCatalog catalog = 1; }
message WatchDataCatalogRequest {
  bool enabled = 1;
  reserved 2 to 8;
}
message DataCatalogEvent {
  DataCatalog catalog = 1;
  reserved 2 to 8;
}
```

`ListDataCatalogRequest` returns one deterministic full snapshot of currently
connected providers and their active channel definitions. Providers and
channels are sorted by their identifiers so tools produce stable output.

Catalog watching is opt-in. Enabling it immediately sends a full
`DataCatalogEvent`, then another full snapshot whenever the catalog revision
changes. Disabling it is idempotent. Catalog changes are low-frequency control
traffic and deliberately separate from channel-value delivery.

`WatchDataCatalogRequest` requires a nonzero request ID and receives the
existing `Ack` for both enable and disable. When enabling, the first full
`DataCatalogEvent` uses `request_id = 0` and is written after that `Ack`, exactly
as ordinary topic subscription snapshots are ordered after their response. An
empty request has `enabled = false` under proto3 and therefore disables the
watch.

An exported gauge does not need to watch the catalog. Gauge Studio and
diagnostic clients use discovery to present available variables without making
users type composite identities manually.

## 10. Exact-Channel Subscription

```protobuf
message DataChannelRef {
  string provider_namespace = 1;
  string channel_name = 2;
  reserved 3 to 8;
}

message SubscribeDataChannelsRequest {
  repeated DataChannelRef channels = 1;
}

message DataChannelSubscriptionResult {
  DataChannelRef channel = 1;
  bool accepted = 2;
  string reason = 3;
}

message SubscribeDataChannelsResponse {
  repeated DataChannelSubscriptionResult results = 1;
}

message UnsubscribeDataChannelsRequest {
  repeated DataChannelRef channels = 1;
}
```

Subscription rules:

- A syntactically valid composite reference is accepted even when the provider
  or channel does not exist. It becomes a waiting subscription.
- Repeating a subscription is idempotent and re-sends the current availability
  and latest-value snapshot.
- `SubscribeDataChannelsResponse` is written before any corresponding snapshot
  events on that connection.
- Unsubscribe is exact and idempotent, responds with the existing `Ack`, and
  does not affect any provider or other consumer.
- There are no wildcards, provider-wide value subscriptions, predicates,
  formulas, requested cadences, or queue policies in the request.
- Consumer-session teardown simply discards that session's subscription set.
  A reconnected consumer must resubscribe, matching the existing External API
  model and web-widget reconnect behavior.

### 10.1 Availability events

Value traffic does not resend static definitions at high frequency. A separate
event establishes the lifecycle and compatibility boundary:

```protobuf
enum DataChannelAvailability {
  DATA_CHANNEL_AVAILABILITY_UNSPECIFIED = 0;
  DATA_CHANNEL_AVAILABILITY_AVAILABLE = 1;
  DATA_CHANNEL_AVAILABILITY_UNAVAILABLE = 2;
}

enum DataUnavailableReason {
  DATA_UNAVAILABLE_REASON_UNSPECIFIED = 0;
  DATA_UNAVAILABLE_REASON_PROVIDER_ABSENT = 1;
  DATA_UNAVAILABLE_REASON_CHANNEL_ABSENT = 2;
  DATA_UNAVAILABLE_REASON_PROVIDER_DISCONNECTED = 3;
  DATA_UNAVAILABLE_REASON_CHANNEL_REMOVED = 4;
}

message DataChannelAvailabilityEvent {
  DataChannelRef channel = 1;
  DataChannelAvailability availability = 2;
  DataChannelDefinition definition = 3;
  DataUnavailableReason unavailable_reason = 4;
  uint64 catalog_revision = 5;
  reserved 6 to 12;
}
```

For `AVAILABLE`, `definition` is present and `unavailable_reason` is
`UNSPECIFIED`. For `UNAVAILABLE`, `definition` is absent and the reason explains
the current boundary. Reasons are diagnostic; consumers must not rely on their
wording or use them to implement retry loops.

After a successful subscription:

1. Prodigy sends the current availability event.
2. If available and a retained sample exists, Prodigy sends that sample as a
   normal `DataValuesEvent`.
3. If available with no sample, the consumer presents `unknown`/waiting state.
4. If unavailable, the subscription stays dormant until a matching definition
   appears.

Provider disconnect or explicit channel removal sends `UNAVAILABLE`
immediately. Re-registration sends `AVAILABLE` with the current definition
before any new value event. This ordering lets a gauge validate type and unit
before rendering a sample.

Any live channel-definition change, including metadata-only re-declaration,
sends `AVAILABLE` with the new definition before the next value event for that
channel. Consumers re-evaluate their pinned type and unit on every `AVAILABLE`
event.

If a waiting subscription names an absent provider, its initial reason is
`PROVIDER_ABSENT`. When that provider registers but has not yet declared the
channel, Prodigy sends a new unavailable event with `CHANNEL_ABSENT`. Declaring
the channel then sends `AVAILABLE`. These diagnostic state transitions never
require the consumer to retry its subscription.

### 10.2 Value events

```protobuf
message DataValuesEvent {
  string provider_namespace = 1;
  repeated DataSample samples = 2;
  reserved 3 to 10;
}
```

One event contains only samples from one provider publication and only channels
subscribed by the destination session. If a publication contains no channels
for a consumer, Prodigy sends nothing to that consumer. Prodigy does not merge
separate provider publications or delay them to form larger batches.
Duplicate channel entries have already been reduced to the single last
occurrence defined in Section 8; superseded occurrences are never forwarded.

## 11. Additive External API Allocation

The exact implementation must re-open the current schema immediately before
editing and confirm that these reserved values remain unused. Grounded at the
commit in this document header, the proposed allocation is:

### 11.1 Capability

Add to `Capabilities`:

```protobuf
optional bool data_provider_bridge = 3;
```

Absent means unsupported. Present `true` means the server supports the full
registration, discovery, subscription, and value contract in this design.
The server's API minor version advances from 1 to 2, but feature detection uses
the capability flag rather than version comparison.

No `TOPIC_DATA` is added to `Topic`. This domain intentionally has its own
exact-channel subscription contract.

### 11.2 Envelope fields

Add `import "api/data.proto";` to `api.proto` and allocate selected fields from
the reserved 80-99 future-domain block:

| Field | Payload |
|---:|---|
| 80 | `RegisterDataProviderRequest register_data_provider_request` |
| 81 | `RegisterDataProviderResponse register_data_provider_response` |
| 82 | `DeclareDataChannelsRequest declare_data_channels_request` |
| 83 | `DeclareDataChannelsResponse declare_data_channels_response` |
| 84 | `RemoveDataChannelsRequest remove_data_channels_request` |
| 85 | `PublishDataValues publish_data_values` |
| 86 | `ListDataCatalogRequest list_data_catalog_request` |
| 87 | `ListDataCatalogResponse list_data_catalog_response` |
| 88 | `SubscribeDataChannelsRequest subscribe_data_channels_request` |
| 89 | `SubscribeDataChannelsResponse subscribe_data_channels_response` |
| 90 | `UnsubscribeDataChannelsRequest unsubscribe_data_channels_request` |
| 91 | `DataValuesEvent data_values_event` |
| 92 | `WatchDataCatalogRequest watch_data_catalog_request` |
| 93 | `DataCatalogEvent data_catalog_event` |
| 94 | `DataChannelAvailabilityEvent data_channel_availability_event` |

Fields 95-99 remain reserved. Existing reserved declarations are split only as
needed; no other reserved or assigned field changes.

### 11.3 Registration messages

```protobuf
message RegisterDataProviderRequest {
  DataProviderDefinition provider = 1;
}

message RegisterDataProviderResponse {
  bool accepted = 1;
  string reason = 2;
}

message DeclareDataChannelsRequest {
  repeated DataChannelDefinition channels = 1;
}

message DataChannelDeclarationResult {
  string channel_name = 1;
  bool accepted = 2;
  string reason = 3;
}

message DeclareDataChannelsResponse {
  repeated DataChannelDeclarationResult results = 1;
}

message RemoveDataChannelsRequest {
  repeated string channel_names = 1;
}
```

Registration and declaration requests require nonzero request IDs and receive
their typed response or `Ack`. Partial channel-declaration success is normal.
Publishing before provider registration is a dropped-and-logged protocol
misuse, not a connection-ending fault.

## 12. Server Components and Responsibilities

### 12.1 `DataRegistry`

Composition-root-owned, main-thread core service responsible for:

- active provider namespace ownership;
- provider and channel definitions;
- catalog revision and deterministic snapshots;
- latest validly-formed sample per active channel;
- provider/channel lifecycle signals; and
- exact channel value signals.

It has no socket, protobuf framing, authentication, widget, OBD, CAN, EventBus,
D-Bus, or AA dependency. Its owner parameter is an opaque session identity
supplied by the API bridge so teardown can remove precisely one session's
registrations.

### 12.2 API bridge/session integration

The API layer is responsible for:

- READY-state message routing and request IDs;
- associating provider ownership and subscriptions with `ApiSession`;
- protobuf-to-registry validation and scalar conversion;
- filtering each publication to each consumer's exact subscription set;
- ordering response, availability, and snapshot events;
- translating session teardown into provider cleanup;
- serializing public events; and
- applying the existing transport frame and outbound-queue rules.

Value, availability, and catalog fan-out captures an immutable snapshot of
destination sessions before the first write, then re-validates that each
destination is still READY and still has the relevant subscription or watch
immediately before writing. A write may synchronously tear its session down
when the outbound cap is exceeded; that teardown removes subscriptions and may
also remove a provider and emit nested availability changes. No fan-out walks a
container that such teardown can mutate. Nested provider cleanup is deferred
until the current fan-out completes or is applied only through separately
snapshot-guarded fan-out state.

Data-domain events use the request sink's direct session-send seam with
`request_id = 0`; they do not pass through `ApiSession::deliver()`, whose
current contract deliberately accepts only the five ordinary status-topic
payloads.

This domain does not use `TopicPublisher` and does not alter the semantics of
ordinary `SubscribeRequest` or `ApiSession::deliver()`.

### 12.3 Lifetime

`DataRegistry` must outlive the started `ApiServer` and every `ApiSession` that
references it. Server shutdown tears down sessions before releasing the
registry. Provider cleanup is idempotent so explicit socket close, queue abort,
server stop, and object destruction converge on one result.

The registry pointer enters `ApiServer` through `ApiServiceRefs` at
construction, because the current request handler is constructed in the server
constructor rather than during `start()`.

## 13. Performance and Backpressure Contract

The API must not pretend to know an appropriate rate for arbitrary hardware.
Accordingly:

- providers choose publication cadence;
- consumers choose only identities, not rates;
- each valid provider publication produces one immediate filtered value event
  per interested consumer;
- the server keeps no per-channel history or unbounded application queue;
- the latest retained sample is replaced in place; and
- the existing per-client unsent-byte cap remains authoritative.

If a consumer stops reading and exceeds the outbound cap, its API session is
disconnected immediately. It can reconnect, resubscribe, and recover current
values. A provider that publishes too aggressively may consume excessive main
thread, serialization, socket, and renderer resources; that is an accepted
trusted-configuration risk, not something Prodigy silently reshapes.

Catalog watches deliberately receive a complete snapshot after every real
catalog revision. A provider that repeatedly changes descriptive metadata can
therefore amplify catalog traffic across all watchers. As with aggressive
value publication, this is an accepted trusted-configuration risk in v1:
Prodigy does not rate-limit, coalesce, or reinterpret metadata updates.

Each WebEngine widget currently owns its own WebSocket session. Exact filtering
therefore remains load-bearing: a gauge receives only its configured channels,
not a full registry snapshot each time an unrelated value changes.

Gauge rendering may independently keep only the newest received sample until
the next browser animation frame. That is presentation behavior local to the
widget and does not change the public delivery contract.

## 14. Error Handling

| Condition | Result |
|---|---|
| Invalid provider namespace | Registration rejected with reason; session remains READY. |
| Namespace already active | Registration rejected with reason; existing owner unchanged. |
| Owning session tries a different namespace | Registration rejected with `accepted = false` and reason; its existing namespace remains unchanged and the session stays READY. |
| Declaration before provider registration | Every channel result is rejected with `accepted = false` and reason `provider not registered` in `DeclareDataChannelsResponse`; no `Error`, disconnect, or catalog mutation. |
| Invalid channel name/type | That declaration is rejected; valid siblings may succeed. |
| Active type change | That declaration is rejected; existing definition remains active. |
| Metadata-only re-declaration | Definition updated, catalog revision advanced if changed, subscribers receive fresh availability definition. |
| Unknown channel publication | Sample dropped and logged; valid siblings continue. |
| Scalar type mismatch | Sample dropped and logged; previous retained value remains. |
| Usable quality without a value | Sample dropped and logged. |
| Bad provider timestamp | Preserved as provider provenance/display data; backend is trusted to normalize its clock. It never affects receipt-monotonic staleness. |
| Any response-bearing data request with `request_id = 0`, including `WatchDataCatalogRequest` | `Error{INVALID_REQUEST}` with `request_id = 0`, then disconnect under the existing connection-level-fault rule. |
| `PublishDataValues` with nonzero `request_id` | Entire batch dropped and logged; no response and no disconnect. A request ID does not turn fire-and-forget publication into a request. |
| Provider disconnect | All its channels become unavailable, retained values are removed, catalog entry disappears. |
| Consumer disconnect | Its subscriptions disappear; providers and other consumers are unaffected. |
| Slow consumer | Existing outbound cap aborts its session; no provider is blocked. |
| Oversized frame | Existing framing policy rejects/disconnects exactly as for other API traffic. |

Publication errors are not reflected with per-batch acknowledgements because
that would double high-rate traffic and create backpressure in the wrong
direction. Provider developers use local validation and Prodigy's namespaced
logs. Registration and declaration remain acknowledged because they define the
contract under which fire-and-forget samples are interpreted.

For scalar presence, an absent `DataSample.value` and a present `DataScalar`
whose oneof has no selected case both mean "no value" and are treated
identically.

## 15. Public Web-Widget Contract

After the protobuf surface is public, the injected shim adds a `prodigy.data`
namespace. It is sugar over External API messages and gains no private
capability:

```javascript
const catalog = await prodigy.data.listCatalog();

const unsubscribe = prodigy.data.subscribe(
  {
    providerNamespace: "com.example.vehicle",
    channelName: "engine.rpm"
  },
  event => {
    // event.definition changes at availability boundaries
    // event.sample carries typed value, effective timestamp, and quality
    // event.available distinguishes waiting/disconnected state
  }
);
```

The concrete callback object must preserve:

- composite identity;
- availability and unavailable reason;
- current channel definition when available;
- exact scalar type;
- exact unit metadata;
- effective observation timestamp;
- a monotonic local receipt reference;
- quality; and
- exact 64-bit integer representation when JavaScript `Number` would be lossy.

The shim's scalar mapping is fixed rather than implementation-defined:

| Protobuf scalar | JavaScript callback value |
|---|---|
| `double_value` | `number` |
| `signed_integer_value` | `bigint` |
| `unsigned_integer_value` | `bigint` |
| `boolean_value` | `boolean` |
| `string_value` | `string` |
| `enum_value` | `bigint` plus the current option label when defined |

The shim maps quality enums to the lowercase strings `unknown`, `good`,
`degraded`, `stale`, `invalid`, and `unavailable`. It never calls
`Number(...)` on a channel's protobuf integer value implicitly.

`observed_at_unix_ms` is explicitly converted to JavaScript `number` for the
normalized `timestampMs`; Unix epoch milliseconds remain safely inside
`Number.MAX_SAFE_INTEGER`. This timestamp conversion is distinct from channel
scalar conversion.

When the shim decodes each received sample, it also stamps
`receivedAtMonotonicMs` from the browser's monotonic clock before invoking
callbacks. `timestampMs` remains provider provenance/display data. Consumers
compute elapsed freshness only as
`monotonicNow - receivedAtMonotonicMs`; they never compare `timestampMs` with
`Date.now()`. This remains correct when the RTC-less Pi starts with a bad wall
clock, NTP or a companion later steps it, or a provider uses an independent
GNSS-synchronized clock.

The returned unsubscribe function removes local callback delivery immediately
and sends the exact server unsubscribe when no callback in that widget session
still uses the binding. Reconnect re-subscribes all active bindings, matching
the shim's existing subscription behavior.

Connection loss maps active bindings to unavailable before reconnect begins;
the widget must never continue displaying an apparently live old value without
its configured stale/unavailable presentation.

`prodigy.data.listCatalog()` and other one-shot data requests reject while the
widget socket is disconnected, matching the existing `prodigy.request()`
contract; callers may retry. Active data subscriptions are retained locally and
re-sent after the next `ServerHello`.

## 16. Gauge Studio Contract

Gauge Studio is free to restructure its current draft schema. The preferred
clean live binding is:

```json
{
  "adapter": "prodigy-data",
  "source": {
    "providerNamespace": "com.example.vehicle",
    "channelName": "engine.rpm"
  },
  "expected": {
    "valueType": "double",
    "unit": "rpm"
  }
}
```

The editor uses `listCatalog()` to select a live provider/channel and pins the
selected channel's type and exact optional unit into the exported document.
The exported adapter subscribes by composite identity and maps events into the
existing normalized runtime sample shape:

```javascript
{
  value,
  timestampMs,
  receivedAtMonotonicMs,
  quality,
  staleAfterMs
}
```

The document already owns presentation range, conversion, formatting, alarm,
and stale behavior. Those remain Gauge Studio responsibilities.

Every exported live gauge must resolve a finite positive `staleAfterMs` as
part of that existing presentation policy. Gauge Studio may seed it from the
channel's `stale_after_ms`; when the provider omits that hint, the editor must
still persist a local value before export. This timeout controls presentation
only and never requests or limits provider cadence. The runtime schedules a
deadline from `receivedAtMonotonicMs` and enters its stale presentation when it
expires even if no later sample or socket event arrives. Provider/channel
unavailability still takes effect immediately.

A retained snapshot received by a newly subscribed widget starts its local
monotonic freshness window at that receipt. Without adding server-retained age
to the wire, that snapshot can appear fresh for at most one configured stale
interval even if it was retained earlier. This bounded false-fresh window is an
accepted v1 tradeoff; wall-clock comparison is not an acceptable workaround.

Compatibility policy:

- provider and channel match, type and unit match: accept samples;
- provider or channel absent: show unavailable and wait;
- channel available but type differs: show incompatible/unavailable;
- channel available but exact optional unit differs: show
  incompatible/unavailable;
- signed/unsigned integer cannot be represented exactly by the selected gauge
  runtime: show incompatible rather than round;
- live binding never falls back to animated sample data.

Unit compatibility is exact and case-sensitive, including presence: an absent
unit matches only an absent expected unit. Gauge Studio may offer an explicit
user-authored conversion during gauge construction, but neither the adapter nor
Prodigy guesses that two different unit strings are compatible.

Prodigy forwards the channel definition and values regardless. Compatibility
presentation is intentionally a consumer responsibility, not server policy.

## 17. External Backend Contract

A conforming backend:

1. connects to the existing External API transport;
2. completes `ClientHello` and existing auth/pairing when required;
3. confirms `Capabilities.data_provider_bridge`;
4. registers one stable provider namespace;
5. declares its available channels and checks each declaration result;
6. publishes typed samples at its configured cadence with `request_id = 0`;
7. explicitly removes channels that disappear while the process remains
   connected, when it can distinguish that state;
8. on reconnect, repeats registration and full live channel declaration before
   resuming publication; and
9. validates its own values, types, units, and timestamps before publishing.

A remote backend may use the existing client-initiated `Ping`/`Pong` exchange
to detect a half-open connection to Prodigy. That protects the backend's own
reconnect behavior; it does not make Prodigy actively probe the provider.

The backend receives no subscriber count, requested interval, polling hint, or
flow-control request from Prodigy. It owns source polling and publication
policy completely.

## 18. Security and Trust Model

This design inherits External API v1's trust decision: localhost is trusted;
remote clients must authenticate; once READY, there are no per-capability
ACLs. Any authenticated client may attempt provider registration and any
authenticated consumer may discover and subscribe to live data.

Namespace uniqueness prevents ambiguous active ownership; it is not an
authorization claim. Prodigy does not prove that a backend legitimately owns a
reverse-DNS name. The user controls installed and paired clients.

No credentials, pairing secrets, or network-admission rules are added to data
messages. Strings remain protobuf data under the existing frame cap and are
never interpreted as QML, JavaScript, JSON, filesystem paths, URLs, or action
identifiers by `DataRegistry`.

## 19. Verification Strategy

### 19.1 Protobuf and pure registry tests

- Round-trip every new message and every scalar oneof case.
- Preserve unknown enum values and exact signed/unsigned integer boundaries.
- Validate additive field numbers, capability absence/presence, and old-client
  tolerance.
- Register, reject duplicate active namespaces, declare, idempotently
  re-declare, update metadata, reject active type changes, remove, and clean up
  by owner.
- Verify catalog revision changes only when live catalog state changes and that
  catalog ordering is deterministic.
- Verify latest-value replacement, timestamp fill, mixed-validity batches, and
  no retained state after provider teardown.
- Verify duplicate samples are reduced before validation and forwarding, with
  one winner per channel ordered by each winner's last inbound position.

### 19.2 API session and loopback tests

- Provider registration requires READY and receives typed results.
- Publication before registration and invalid samples are dropped without
  killing the session.
- A consumer can subscribe before provider registration, receives unavailable,
  then availability and values automatically.
- An active subscription receives response, availability definition, and
  current retained value in that order.
- Exact filtering prevents unrelated channels and providers from reaching a
  consumer.
- Multiple accepted publications produce multiple events without server-side
  rate reduction or cross-publication merging.
- Provider channel removal and disconnect produce immediate unavailable;
  reconnect under the same identity resumes the waiting subscription.
- Catalog list/watch snapshots and revisions follow provider lifecycle.
- Catalog-watch enable with a zero request ID faults and disconnects; a
  publication with a nonzero request ID is dropped without response or
  disconnect.
- Consumer unsubscribe and teardown remove only that consumer's interests.
- A stalled consumer is disconnected by the existing outbound cap while other
  consumers and the provider continue.
- TCP and WebSocket paths exhibit the same semantics.

### 19.3 Web shim tests

- Catalog request/response mapping.
- Subscription sharing and exact unsubscribe behavior within one widget.
- Waiting, available, sample, incompatible, disconnect, and reconnect callback
  sequences.
- Wall-clock jumps and provider/Pi clock disagreement do not affect monotonic
  receipt stamps or stale deadlines.
- Stale presentation fires after the configured monotonic interval even when
  no subsequent sample or disconnect event arrives.
- Exact handling of 64-bit integers outside JavaScript's safe range.
- No capability or data surface exists in the shim unless the public protobuf
  capability is present.

### 19.4 Cross-repository conformance

- A deterministic backend fixture declares representative numeric, boolean,
  string, and enum channels and publishes known batches.
- Gauge Studio consumes the fixture through the public shim and produces its
  normalized sample shape.
- Type and unit mismatch fixtures show incompatible rather than a plausible
  value.
- Backend restart verifies waiting-subscription recovery without widget retry
  logic.
- A half-open provider fixture demonstrates the documented boundary: catalog
  availability remains until transport teardown, while the gauge becomes
  stale from its local monotonic deadline.
- A sustained configurable-rate simulator demonstrates transparent forwarding;
  results are measurements, not a promised universal frequency ceiling.

### 19.5 Required Prodigy final gate when implemented

Because implementation will change C++, protobuf, CMake/code generation, the
embedded web shim, and Pi-facing runtime behavior, the final Prodigy tree must
pass:

- focused API/protobuf/registry/shim tests while iterating;
- native build;
- explicit `openauto-prodigy` app target;
- `ctest --output-on-failure`;
- `./cross-build.sh`; and
- a live Pi test using a simulated external provider and an exported gauge.

The implementation plan must specify the independent verification required in
the backend and Gauge Studio repositories rather than treating a green Prodigy
suite as cross-repository proof.

## 20. Documentation and Compatibility Work

An implementation updates, in the same change set where applicable:

- `proto/api/` comments and the new `data.proto` source of truth;
- External API architecture and developer documentation;
- web-widget authoring documentation for `prodigy.data`;
- generated protobuf JavaScript bundled into Prodigy;
- Gauge Studio's binding and adapter contract documentation; and
- the external backend's provider conformance documentation.

The existing five topic subscriptions and all current API clients remain
wire-compatible. Older clients ignore the capability and new envelope fields.
New clients treat absence of `data_provider_bridge` as unsupported and do not
infer support from the API minor version alone.

## 21. Deferred Extensions

The following require separate evidence and approval rather than being grown
inside this design or its implementation plan:

- writable data channels or a request/acknowledgement command domain;
- provider subscription-interest feedback;
- wildcard or provider-wide subscriptions;
- server-side cadence requests, downsampling, aggregation, or history;
- persistent provider definitions or offline catalog browsing;
- server-initiated provider heartbeat/liveness probing and timeout policy;
- provider ACLs, namespace authorization, or hostile-client quotas;
- semantic aliases and cross-provider arbitration;
- data-derived AA sensors or vehicle-control exposure; and
- a dedicated high-throughput transport if measured scalar traffic proves the
  existing API unsuitable.

## 22. Executor Guidance

Before implementation:

1. Re-open current `proto/api/api.proto` and `common.proto`; verify every
   proposed field and reserve split against the implementation base.
2. Preserve all frozen-additive rules. Never edit the hands-off Android Auto
   protocol submodule for this feature.
3. Keep the API bound to `DataRegistry`; do not shortcut through EventBus,
   backend internals, or a private JS/QML bridge.
4. Keep provider cadence transparent. Do not add a timer, requested interval,
   latest-only delivery optimization, or silent coalescing to make a benchmark
   look better.
5. Keep one authoritative provider teardown path and one consumer teardown
   path; exercise socket close, server stop, and queue-abort cases. Every
   fan-out must iterate an immutable destination snapshot and tolerate nested
   teardown without mutating a live iterator.
6. Treat Gauge Studio and the backend as independent consumers of this written
   wire contract. Do not make either repository depend on Prodigy implementation
   headers or internal Qt types.
7. Stop and update this design before implementing any behavior that conflicts
   with its scope, trust model, or delivery guarantees.
8. Reconfigure CMake after adding `data.proto`; the current proto source list is
   globbed without `CONFIGURE_DEPENDS`, so a build alone will not discover the
   new file.
9. Extend both `ServerHello` capability parsing and stream-event routing in the
   web shim. The current shim recognizes only the five status fields and does
   not consume capability flags.

## 23. Definition of Design Success

This design is ready for implementation planning when:

- its protobuf allocation is confirmed against the live schema;
- Prodigy, Gauge Studio, and backend responsibilities are unambiguous;
- provider/channel lifecycle ordering is explicit;
- publication cadence and backpressure ownership are explicit;
- the user approves the written contract;
- the requested independent Opus 4.6 architecture review is adjudicated;
- the user-supplied Fable follow-up review is adjudicated; and
- no unresolved blocker requires a different public wire shape.
