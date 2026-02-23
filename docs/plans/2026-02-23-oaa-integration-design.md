# open-androidauto Integration Design

**Date:** 2026-02-23
**Status:** Design
**Branch:** `feature/oaa-integration` (to be created)

## Goal

Replace aasdk (Boost.ASIO-based Android Auto SDK) with open-androidauto (Qt-native, signals/slots) in the OpenAuto Prodigy app. Big-bang swap on a feature branch — remove all aasdk usage at once.

## Why

aasdk is built on Boost.ASIO with strands, shared_ptr-based lifetimes, and custom Promise patterns. open-androidauto uses Qt signals/slots, QObject ownership, and runs on the Qt event loop. Maintaining both is untenable — the impedance mismatch between Boost.ASIO and Qt causes threading bugs, lifecycle complexity, and makes the codebase harder to reason about.

## Current aasdk Integration Surface

13 files in `src/` import from aasdk. The heaviest users:

| File | Role | aasdk Coupling |
|------|------|----------------|
| `AndroidAutoService.cpp` | TCP listener, entity lifecycle, watchdog | Transport, TCP, SSL wrappers, ASIO io_service |
| `AndroidAutoEntity.cpp` | Protocol FSM (version, handshake, discovery) | ControlServiceChannel, ICryptor, IMessenger, all control protos |
| `ServiceFactory.cpp` | 8 channel service stubs (850 lines) | All channel types, all event handler interfaces, SendPromise, strands |
| `VideoService.cpp` | H.264 video reception + decode | VideoServiceChannel, IVideoServiceChannelEventHandler |
| `TouchHandler.hpp` | Touch event sender | InputServiceChannel, SendPromise |
| `IService.hpp` | Service interface | ServiceDiscoveryResponse proto |
| `ProtocolLogger.hpp` | Protocol message logging | ChannelId enum |

## Architecture Changes

### 1. Threading Model

**Before:** 4 Boost.ASIO worker threads + strands for thread safety. Protocol work happens on ASIO threads, UI updates marshalled to Qt main thread via `QMetaObject::invokeMethod`.

**After:** Dedicated `QThread` for AA protocol work. `AASession`, `Messenger`, and all channel handlers live on this thread. Video frames and UI state changes are marshalled to main thread via signals with `Qt::QueuedConnection`.

**Rationale:** Moving everything to main thread risks stalling UI during video decode and audio writes. A dedicated protocol thread preserves the current parallelism without ASIO complexity. Qt's signal/slot mechanism with `QueuedConnection` across threads provides the same safety as ASIO strands.

### 2. Session Management

**Before:** `AndroidAutoService` creates `AndroidAutoEntity` which creates `ControlServiceChannel` and manages the protocol FSM manually (version → handshake → auth → discovery).

**After:** `AndroidAutoService` creates `oaa::AASession` which encapsulates the full FSM (Idle → Connecting → VersionExchange → TLSHandshake → ServiceDiscovery → Active → ShuttingDown → Disconnected). `AndroidAutoService` just listens to `stateChanged` and `disconnected` signals.

**Files deleted:**
- `AndroidAutoEntity.hpp/cpp` — fully replaced by `oaa::AASession`
- `ServiceFactory.hpp/cpp` — replaced by individual channel handler classes
- `IService.hpp` — `fillFeatures()` pattern replaced by channel descriptor contribution

### 3. Channel Handlers

**Before:** 8 service stubs in `ServiceFactory.cpp`, each inheriting from both `IService` and an aasdk event handler interface (e.g., `IAudioServiceChannelEventHandler`). Uses `shared_from_this()`, ASIO strands, `SendPromise` for sends.

**After:** Each channel handler is an `oaa::IChannelHandler` subclass (or `IAVChannelHandler` for AV channels). Handlers:
- Receive raw messages via `onMessage(uint16_t messageId, const QByteArray& payload)`
- Send via `emit sendRequested(channelId, messageId, payload)` signal
- Provide a channel descriptor via virtual method for service discovery
- Use QObject parent-child ownership instead of shared_ptr

**New files in `src/core/aa/`:**

| File | Replaces | Channel IDs |
|------|----------|-------------|
| `AudioChannelHandler.hpp/cpp` | AudioServiceStub (template) | 3 (media), 4 (speech), 5 (system) |
| `InputChannelHandler.hpp/cpp` | InputServiceStub | 6 |
| `SensorChannelHandler.hpp/cpp` | SensorServiceStub | 7 |
| `BluetoothChannelHandler.hpp/cpp` | BluetoothServiceStub | 10 |
| `WifiChannelHandler.hpp/cpp` | WIFIServiceStub | 8 |
| `AVInputChannelHandler.hpp/cpp` | AVInputServiceStub | 11 |
| `VideoChannelHandler.hpp/cpp` | VideoService | 1 |

**Message ID sourcing:** Use generated proto enums (`AVChannelMessageIdsEnum`, `InputChannelMessageIdsEnum`, etc.) instead of scattered magic numbers. Each handler includes its relevant message ID enum header.

### 4. Service Discovery

**Before:** Each `IService` implements `fillFeatures(ServiceDiscoveryResponse&)` which mutates the response protobuf directly. `AndroidAutoEntity::onServiceDiscoveryRequest()` iterates services and calls `fillFeatures`.

**After:** Each `IChannelHandler` provides a `channelDescriptor()` method that returns a populated `ChannelDescriptorData` protobuf. `AASession`'s `buildServiceDiscoveryResponse()` collects descriptors from all registered handlers and assembles the full response.

This requires a new virtual method on `IChannelHandler`:
```cpp
virtual oaa::proto::data::ChannelDescriptor channelDescriptor() const = 0;
```

The descriptor includes the full nested config (AV channel data, audio configs, touch config, sensor types, BT adapter, WiFi SSID, etc.) — same richness as the current `fillFeatures()` output.

### 5. Control-Plane Parity

`AASession` must handle all control channel signals that `AndroidAutoEntity` currently handles:

| Signal | Current Behavior | Integration Point |
|--------|-----------------|-------------------|
| `audioFocusRequested` | Grants requested focus type | `AndroidAutoService` or `AASession` handler |
| `navigationFocusRequested` | Detects exit-to-car (type != 1) | `AndroidAutoService` emits `projectionFocusLost` |
| `voiceSessionRequested` | Logged, no action | Log only |
| `shutdownRequested` | Sends response, disconnects | Already handled by `AASession` |
| `pingReceived` | Auto-responds | Already handled by `ControlChannel` |

Two options:
- **A) Wire in AASession** — add slots for audio/nav/voice focus in AASession, emit signals for app-level handling
- **B) Expose ControlChannel** — let AndroidAutoService connect directly to ControlChannel signals

**Recommendation:** Option A. Keep the control channel as an internal detail of AASession. Add `audioFocusRequested`/`navigationFocusRequested`/`voiceSessionRequested` signals on AASession that AndroidAutoService connects to.

### 6. TCP Listener

**Before:** Boost.ASIO `tcp::acceptor` with async_accept, `aasdk::tcp::TCPWrapper`, `aasdk::transport::TCPTransport`.

**After:** `QTcpServer` for accepting connections. On `newConnection()`, take the `QTcpSocket`, wrap in `oaa::TCPTransport`, create `AASession`.

### 7. Connection Watchdog

**Before:** `QTimer` polls `tcp_info` via `getsockopt(IPPROTO_TCP, TCP_INFO)` on the raw socket FD. Checks `tcpi_backoff >= 3` for dead peer detection.

**After:** Same approach, but need native FD access. Add to `oaa::TCPTransport`:
```cpp
qintptr nativeSocketDescriptor() const;
```

This delegates to `QTcpSocket::socketDescriptor()`. The watchdog in `AndroidAutoService` continues using the same `tcp_info` polling logic.

### 8. Video Pipeline

**Before:** `VideoService` implements `IVideoServiceChannelEventHandler`, receives `onAVMediaWithTimestampIndication` and `onAVMediaIndication` callbacks with `DataConstBuffer`.

**After:** `VideoChannelHandler` implements `IAVChannelHandler`, receives `onMediaData(const QByteArray& data, uint64_t timestamp)`. Routes data to `VideoDecoder` the same way. `AASession` handles the AV channel setup/start/stop protocol messages before routing media data.

**AV protocol handling:** AV channels have a setup phase (SETUP_REQUEST → SETUP_RESPONSE, START_INDICATION) before media flows. Channel handlers need to handle these via `onMessage()` and respond appropriately. Media data (message IDs 0x0000/0x0001) is routed through `IAVChannelHandler::onMediaData()` by `AASession`.

### 9. Touch Input

**Before:** `TouchHandler` holds a `shared_ptr<InputServiceChannel>` and an ASIO strand pointer. Sends `InputEventIndication` via `channel_->sendInputEventIndication()`.

**After:** `TouchHandler` holds a pointer to `InputChannelHandler` (which is an `IChannelHandler`). Sends by calling a method on the handler that serializes the protobuf and emits `sendRequested`. Or simpler: `TouchHandler` gets a direct reference to the `Messenger` and calls `sendMessage(channelId, messageId, payload)`.

**Recommendation:** TouchHandler calls a method on InputChannelHandler like `sendTouchEvent(const InputEventIndication&)`, which serializes and emits `sendRequested`. Keeps the serialization logic in the handler.

### 10. Backpressure

`IAVChannelHandler::canAcceptMedia()` exists but `AASession` doesn't check it. For the initial integration, this is acceptable — the current aasdk code doesn't have backpressure either. But the hook is there for future use (e.g., when VideoDecoder's frame queue is full, stop reading from transport).

### 11. CMake Changes

- Remove `add_subdirectory(libs/aasdk)` from top-level CMakeLists.txt
- Remove `aasdk` and `aasdk_proto` from `target_link_libraries`
- Remove aasdk include directories
- Add `open-androidauto` to `target_link_libraries` (already added as subdirectory)
- Proto includes change from `<aasdk_proto/*.pb.h>` to generated OAA proto headers

### 12. Boost.ASIO Removal

After integration, the AA pipeline has zero Boost.ASIO dependency. Check if any other part of the app uses Boost — if not, remove the Boost dependency entirely from CMake.

## Pre-Integration Library Work

Before starting the integration branch, these gaps in open-androidauto need closing:

1. **Channel descriptor API** — add `virtual ChannelDescriptor channelDescriptor() const` to `IChannelHandler`, update `AASession::buildServiceDiscoveryResponse()` to collect from registered handlers
2. **Control-plane signals** — wire `audioFocusRequested`, `navigationFocusRequested`, `voiceSessionRequested` through `AASession` (connect ControlChannel signals → AASession signals)
3. **Transport native FD** — add `nativeSocketDescriptor()` to `TCPTransport` and `ITransport`
4. **Rich service discovery** — update `buildServiceDiscoveryResponse()` to include `headunit_info`, `connection_configuration`, `ping_configuration`, and all the identity fields currently in `AndroidAutoEntity::onServiceDiscoveryRequest()`

## Implementation Order

1. Close open-androidauto parity gaps (items above)
2. Create feature branch, introduce AASession on dedicated QThread
3. Migrate non-AV channels first (input, sensor, BT, WiFi) — simplest handlers
4. Migrate AV channels (audio, mic) — need AV setup/start/stop protocol
5. Migrate video channel last — most complex (decode pipeline, focus management)
6. Rewrite AndroidAutoService (TCP listener, watchdog, state management)
7. Delete aasdk: remove submodule, CMake refs, unused files
8. Regression test on Pi with real phone connection

## Risk Mitigation

- **Regression risk:** Keep aasdk submodule until final deletion. Can revert to old code if integration fails.
- **Thread safety:** Use `QThread::moveToThread()` for AASession and all handlers. Verify no cross-thread direct calls.
- **Protocol correctness:** Use existing capture files from protocol exploration to validate message format compatibility.
- **Build on both platforms:** Verify compilation on dev VM (Qt 6.4) and Pi (Qt 6.8) at each phase.
