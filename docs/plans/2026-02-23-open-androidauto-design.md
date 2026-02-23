# open-androidauto — Protocol Library Design

**Date:** 2026-02-23
**Status:** Approved
**Replaces:** SonOfGib/aasdk (C++14, Boost.ASIO)

## Motivation

OpenAuto Prodigy is a clean-room rebuild on a modern stack — Qt 6, C++17, PipeWire, Wayland. But at its core sits aasdk: a C++14 library with Boost.ASIO strands, a custom Promise pattern, and ~2,000 lines of USB/AOAP code compiled even in wireless-only mode. Every interaction between the AA protocol and the Qt UI requires cross-thread marshaling via `QMetaObject::invokeMethod`. ServiceFactory.cpp alone is 3,600 lines of inline channel stubs implementing 9+ event handler interfaces.

This is architectural debt at the foundation. open-androidauto is a clean replacement: a Qt-native, C++17 Android Auto protocol library that handles both wireless and wired transports, designed for OEM-quality protocol compliance and forward compatibility with future AA versions.

## Goals

1. **Qt-native async** — signals/slots, QThread, Qt event loop. No Boost.ASIO.
2. **OEM-quality protocol compliance** — handle what production head units handle, including unknown channels, unknown messages, and future protocol extensions.
3. **Both transports** — wireless (TCP/SSL) and wired (USB/AOAP).
4. **Forward compatible** — unknown channels get proper protocol responses (not crashes). Unknown messages are logged and emitted for analysis. Version-driven behavior changes are data-driven.
5. **Clean API** — one `IChannelHandler` base interface, no manual message re-arming, no custom Promise type.
6. **Standalone library** — built in-tree at `libs/open-androidauto/` during development, extractable to its own repo once the API stabilizes.

## Non-Goals

- Wire protocol changes (we stay compatible with AA as it exists)
- proto3 migration (proto2 is required for wire compatibility)
- Multi-display support (future work if protocol details emerge)

---

## Architecture

### Layer Stack

```
+--------------------------------------------------+
|  Application (Prodigy)                           |
|  Registers IChannelHandler implementations       |
+--------------------------------------------------+
|  Session Layer (AASession)                       |
|  FSM lifecycle, channel registry, ping, shutdown |
+--------------------------------------------------+
|  Channel Layer (IChannelHandler)                 |
|  Per-channel typed message dispatch              |
+--------------------------------------------------+
|  Messenger Layer (Messenger)                     |
|  Framing, encryption, multiplexing, reassembly   |
+--------------------------------------------------+
|  Transport Layer (ITransport)                    |
|  TCPTransport / USBTransport — pure byte I/O     |
+--------------------------------------------------+
```

### Layer 1: Transport

Pure byte I/O. No protocol knowledge.

```cpp
namespace oaa {

class ITransport : public QObject {
    Q_OBJECT
public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void write(const QByteArray& data) = 0;

signals:
    void dataReceived(const QByteArray& data);
    void connected();
    void disconnected();
    void error(const QString& message);
};

} // namespace oaa
```

**TCPTransport** — wraps `QTcpSocket`. Lives on the Qt event loop. Reads arrive via `readyRead()`. No additional threads needed.

**USBTransport** — wraps libusb. A `QThread` worker performs blocking `libusb_bulk_transfer()` calls and emits `dataReceived()` back to the main thread. Includes the full AOAP identification sequence:
- Send manufacturer/model/version/description/URI/serial strings
- Check for Google accessory VID `0x18D1`, PID `0x2D00`/`0x2D01`
- Handle device re-enumeration after AOAP switch
- Kernel driver detach/claim/release
- STALL/timeout/short-transfer recovery with endpoint reset

**ReplayTransport** — reads captured wire data from files. Used for integration testing without a phone. Supports both directions (HU and phone side) with timing replay.

Both real transports produce the same byte stream. Everything above this layer is transport-agnostic.

### Layer 2: Messenger

The protocol engine. Handles framing, encryption, channel multiplexing, and message reassembly.

```cpp
class Messenger : public QObject {
    Q_OBJECT
public:
    Messenger(ITransport* transport, QObject* parent = nullptr);

    void start();
    void stop();

    // Queued send — single-writer queue, strict frame ordering
    void sendMessage(uint8_t channelId, uint16_t messageId,
                     const QByteArray& payload);

    // SSL handshake management
    void startHandshake();
    bool isEncrypted() const;

signals:
    void messageReceived(uint8_t channelId, uint16_t messageId,
                         const QByteArray& payload);
    void handshakeComplete();
    void transportError(const QString& message);

private:
    ITransport* transport_;
    Cryptor cryptor_;
    FrameAssembler assembler_;
    EncryptionPolicy encryptionPolicy_;

    // Single-writer send queue (processed on messenger's thread)
    QQueue<SendItem> sendQueue_;
    void processSendQueue();
};
```

**Key design decisions:**

- **`messageReceived` uses raw `uint8_t` channel IDs**, not an enum. Channels 9-13 and any future channels work without code changes.

- **Single-writer send queue** replaces aasdk's ASIO strand serialization. All sends are queued and processed sequentially on the messenger's thread. No QMutex needed — the queue is only touched from one thread. Cross-thread sends post via `QMetaObject::invokeMethod(Qt::QueuedConnection)`.

- **Table-driven encryption policy.** `EncryptionPolicy` maps `(channelId, messageId, sslActive)` to PLAIN or ENCRYPTED. Default rules match the AA spec:
  - Channel 0 messages 0x0001-0x0004 (version/handshake): always PLAIN
  - Channel 0 messages 0x000b-0x000c (ping): always PLAIN
  - Everything else after SSL: ENCRYPTED
  - Everything before SSL: PLAIN

- **Frame reassembly** (`FrameAssembler`) handles interleaved multi-frame messages. Keyed by `(channelId, messageType)` — not just channelId — with one in-flight fragmented message per key. Invalid frame sequences (MIDDLE without FIRST, duplicate FIRST) are handled gracefully: discard partial, log, continue.

- **Fragmentation** (`FrameSerializer`) splits outgoing messages at the 16,384-byte threshold into FIRST/MIDDLE/LAST frames, or sends as BULK for smaller messages.

#### Cryptor

Application-layer TLS 1.2 via OpenSSL memory BIOs, matching the AA protocol spec exactly:

- `SSL_CTX` with `TLS_client_method()` (no version downgrades — AA 12.7+ rejects them)
- Memory BIOs for read/write (not socket-level TLS)
- Hardcoded X.509 certificate + RSA 2048-bit private key
- `SSL_set_connect_state()` — HU acts as TLS client
- `SSL_VERIFY_NONE` — no server certificate verification
- Observed cipher: AES256-GCM-SHA384

The handshake is driven by `AASession` via `startHandshake()` / `handshakeComplete()` signal.

#### Wire Format (implemented by Messenger internals)

Frame header: 2 bytes
- Byte 0: channel ID (uint8_t)
- Byte 1: flags — FrameType (bits 1:0), MessageType (bit 2), EncryptionType (bit 3)

Frame types:
- BULK (0x03): complete message in one frame, 2-byte size
- FIRST (0x01): first fragment, 6-byte size (2-byte frame + 4-byte total)
- MIDDLE (0x00): intermediate fragment, 2-byte size
- LAST (0x02): final fragment, 2-byte size

Message format (after reassembly): uint16 BE message ID + protobuf payload.

### Layer 3: Channel Handlers

One clean interface replaces aasdk's 9+ `IXxxServiceChannelEventHandler` interfaces:

```cpp
class IChannelHandler : public QObject {
    Q_OBJECT
public:
    virtual uint8_t channelId() const = 0;

    // Channel lifecycle
    virtual void onChannelOpened() = 0;
    virtual void onChannelClosed() = 0;

    // Message dispatch — handler deserializes protobuf internally
    virtual void onMessage(uint16_t messageId, const QByteArray& payload) = 0;

    // Capability advertisement
    virtual void fillFeatures(ServiceDiscoveryResponse& response) = 0;

signals:
    // Handler emits this to send; session routes to messenger
    void sendRequested(uint8_t channelId, uint16_t messageId,
                       const QByteArray& payload);

    // Emitted for unrecognized message IDs (telemetry/capture)
    void unknownMessage(uint16_t messageId, const QByteArray& payload);
};
```

**For AV channels** (video, audio), an extended interface adds backpressure:

```cpp
class IAVChannelHandler : public IChannelHandler {
public:
    // Media data delivered directly with timestamp
    virtual void onMediaData(const QByteArray& payload,
                             uint64_t timestamp) = 0;

    // Backpressure — if false, ACK is delayed (phone pauses sending)
    virtual bool canAcceptMedia() const = 0;
};
```

The session checks `canAcceptMedia()` before delivering. When the handler is backed up, `AV_MEDIA_ACK_INDICATION` is delayed — this is the protocol's native flow control. The `max_unacked` value set in the AV setup response controls the window size.

**Application implements concrete handlers:**

```cpp
// In Prodigy (not in the library)
class VideoChannel : public oaa::IAVChannelHandler {
    void onMessage(uint16_t messageId, const QByteArray& payload) override {
        switch (messageId) {
            case 0x8000: handleSetupRequest(payload); break;
            case 0x8001: handleStartIndication(payload); break;
            case 0x8007: handleFocusRequest(payload); break;
            default: emit unknownMessage(messageId, payload); break;
        }
    }
    void onMediaData(const QByteArray& payload, uint64_t timestamp) override {
        // H.264 data -> decoder
    }
};
```

Protobuf deserialization happens inside each handler. The library delivers raw payloads. This keeps the library proto-agnostic while giving handlers typed access to their message schemas.

### Layer 4: Session

`AASession` is the top-level object the application creates. It manages the full AA lifecycle as an explicit finite state machine with per-state timeouts.

```cpp
enum class SessionState {
    Idle,              // not started
    Connecting,        // transport connecting
    VersionExchange,   // raw binary version request/response (pre-SSL)
    TLSHandshake,     // SSL negotiation via Cryptor
    ServiceDiscovery,  // awaiting phone's request, building response
    Active,            // channels open, pings flowing, media streaming
    ShuttingDown,      // graceful shutdown (ShutdownRequest sent)
    Disconnected       // terminal
};

class AASession : public QObject {
    Q_OBJECT
public:
    AASession(ITransport* transport, const SessionConfig& config,
              QObject* parent = nullptr);

    void start();
    void stop();

    // Channel registry — register handlers before start()
    void registerChannel(uint8_t channelId, IChannelHandler* handler);

    SessionState state() const;
    Messenger* messenger() const;  // for direct access if needed

signals:
    void stateChanged(SessionState newState);
    void channelOpened(uint8_t channelId);
    void channelOpenRejected(uint8_t channelId);
    void disconnected(DisconnectReason reason);
};
```

**Channel registry:** The application registers handlers for channels it supports. When the phone sends `CHANNEL_OPEN_REQUEST`:
- **Registered channel** → respond `CHANNEL_OPEN_RESPONSE(status=OK)`, route messages to handler
- **Unregistered channel** → respond `CHANNEL_OPEN_RESPONSE(status=FAIL)`, log with full payload for analysis, emit `channelOpenRejected`

This is OEM behavior — production head units don't crash on unknown channels.

**State machine timeouts:**
- VersionExchange: 5s (phone should respond quickly)
- TLSHandshake: 10s (multiple round-trips)
- ServiceDiscovery: 10s (phone sends request after handshake)
- Ping watchdog: configurable via `connection_configuration` from phone's ServiceDiscoveryRequest (fallback: 10s interval, 3 missed = dead)

**ControlChannel is built into the library** — version exchange, SSL handshake, service discovery, and ping are protocol fundamentals every consumer needs. All other channels are implemented by the application.

**Ping configuration** uses the phone's `connection_configuration` data (ping interval, timeout) rather than hardcoded values. Production head units do this.

---

## Forward Compatibility Strategy

### Unknown Channels

Phone opens channel 9 (Radio), 10 (Navigation), or 15 (future). Session responds with `CHANNEL_OPEN_RESPONSE(status=FAIL)`. Logged with channel ID and payload for future implementation.

### Unknown Messages

Channel receives message ID 0x9003 it doesn't recognize. Handler's default `onMessage` case logs channel/ID/size and emits `unknownMessage` signal. Optional capture for protocol analysis.

### Version-Driven Behavior

`SessionConfig` includes `protocolVersion` (currently v1.7). Version-specific behavior is gated through the config, not scattered conditionals. When v1.8 or v2.0 arrives, changes are data-driven.

### Replay Testing

`ReplayTransport` reads captured wire data from real phone sessions. Integration tests replay these captures to verify protocol handling without a live phone. Captures include both directions with timing data.

---

## Protobuf Strategy

We port the existing 108 .proto files from aasdk. They are proto2 (required for wire compatibility) and represent years of reverse engineering work. The generated .pb.h headers are built at compile time via CMake's protobuf_generate.

As we discover new message types (channels 9-13, extended vehicle sensors, etc.), we add new .proto files without changing existing ones.

Proto files live at `libs/open-androidauto/proto/` and generate into the build directory.

---

## Threading Model

All protocol state lives on **one Qt thread** — the messenger, session FSM, channel registry, TLS state, and ping timer. This eliminates the cross-thread marshaling that plagues the current aasdk integration.

**Transports** may run on other threads:
- TCPTransport: Qt event loop (same thread as protocol, via QTcpSocket)
- USBTransport: dedicated QThread for blocking libusb calls
- Both deliver data via queued signal connections (thread-safe by Qt's guarantee)

**Application handlers** receive messages on the protocol thread. If they need to do heavy work (e.g., video decode), they dispatch to their own worker threads — same as today, but without the ASIO middleman.

**Ownership:** All QObjects follow Qt's parent-child ownership. `AASession` owns the `Messenger`, which owns the `Cryptor` and frame processors. Cross-thread destruction uses `deleteLater()` on the owning thread. `start()`/`stop()` are idempotent and cancellation-safe.

---

## Comparison with aasdk

| Concern | aasdk | open-androidauto |
|---------|-------|------------------|
| Async model | Boost.ASIO strands + custom Promise | Qt signals/slots + QThread |
| Threading | 4 ASIO threads + marshal to Qt | Single protocol thread + Qt event loop |
| Channel handlers | 9+ IXxxEventHandler interfaces | One IChannelHandler base |
| Message delivery | Manual re-arm (`channel_->receive()`) | Continuous receive, auto-dispatch |
| Unknown channels | Crash or ignore | Respond FAIL, log, emit signal |
| Unknown messages | Ignore | Log + unknownMessage signal |
| Send ordering | ASIO strand serialization | Single-writer queue on protocol thread |
| Media backpressure | None (unbounded) | canAcceptMedia() + ACK delay |
| Encryption | Scattered conditionals | Table-driven EncryptionPolicy |
| USB support | Always compiled, ~2K LOC | Optional ITransport, same interface |
| Proto files | proto2 (108 files) | Same proto2 files, ported |
| C++ standard | C++14 | C++17 |
| Session lifecycle | Implicit state, no timeouts | Explicit FSM with per-state deadlines |
| Dependencies | Boost (asio, log, system), libusb, OpenSSL, protobuf | Qt 6 (Core, Network), OpenSSL, protobuf, libusb (optional) |

---

## Library Structure

```
libs/open-androidauto/
+-- CMakeLists.txt
+-- include/oaa/
|   +-- Transport/
|   |   +-- ITransport.hpp
|   |   +-- TCPTransport.hpp
|   |   +-- USBTransport.hpp
|   |   +-- ReplayTransport.hpp
|   +-- Messenger/
|   |   +-- Messenger.hpp
|   |   +-- Cryptor.hpp
|   |   +-- FrameAssembler.hpp
|   |   +-- FrameSerializer.hpp
|   |   +-- EncryptionPolicy.hpp
|   +-- Channel/
|   |   +-- IChannelHandler.hpp
|   |   +-- IAVChannelHandler.hpp
|   |   +-- ControlChannel.hpp       (built-in)
|   +-- Session/
|   |   +-- AASession.hpp
|   |   +-- SessionConfig.hpp
|   |   +-- SessionState.hpp
|   +-- Proto/                        (generated at build time)
|   +-- Version.hpp
+-- src/
|   +-- Transport/
|   +-- Messenger/
|   +-- Channel/
|   +-- Session/
+-- proto/                            (proto2 .proto source files)
+-- tests/
    +-- test_frame_assembler.cpp
    +-- test_frame_serializer.cpp
    +-- test_encryption_policy.cpp
    +-- test_cryptor.cpp
    +-- test_messenger.cpp
    +-- test_session_state.cpp
    +-- replay/                       (captured wire data)
```

**Dependencies:** Qt 6 (Core, Network), OpenSSL, protobuf, libusb (optional, compile-time flag for USB transport)

**Build:** Static library linked into Prodigy. CMake option `OAA_ENABLE_USB` controls USB transport compilation. Protobuf headers generated at build time.

---

## Development & Testing Environment

**Dev VM (claude-dev):** Ubuntu 24.04 VM, Qt 6.4. Used for iterative development and unit testing. No Bluetooth or real AA hardware.

**Pi target (192.168.1.149):** Raspberry Pi 4, RPi OS Trixie, Qt 6.8. Running wireless AA instance available for integration testing. SSH accessible from dev VM.

**Phone (USB ADB):** Moto G Play 2024 (Android 14) connected via USB ADB to the dev VM (`platform-tools/adb`). Available for:
- Logcat capture during AA sessions (`adb logcat | grep AndroidAuto`)
- AA app version/state inspection
- Triggering AA connections to the Pi
- Protocol-level debugging (phone-side logs correlate with Pi-side captures)

**Samsung S25 Ultra:** Primary test phone (wireless AA confirmed working with current Prodigy).

**Testing strategy:**
- Unit tests for frame assembler, serializer, encryption policy, cryptor, session FSM — run on dev VM, no hardware needed
- Integration tests via ReplayTransport with captured wire data — run on dev VM
- Live protocol tests against real phone — run on Pi with phone connected
- Regression tests: capture working sessions, replay after refactors

---

## Migration Plan (high level)

1. Build open-androidauto as a parallel library in `libs/open-androidauto/`
2. Implement bottom-up: Transport → Messenger → Session
3. Write tests at each layer using ReplayTransport with captured data
4. Implement Prodigy's channel handlers (Video, Audio, Input, Sensor, etc.) against the new API
5. Swap `AndroidAutoService` to use `oaa::AASession` instead of aasdk entities
6. Remove aasdk submodule
7. Extract to separate repo once API is stable

This is a big-bang swap — no adapter layer, no parallel running. The old code stays on `main` as a fallback via git.

---

## Open Questions

- **Certificate management:** aasdk hardcodes a JVC Kenwood certificate. Do we carry this forward, generate our own, or make it configurable? (Current answer: carry forward, it works, revisit if Google tightens cert validation.)
- **Channels 9-13 message format:** We know these services exist (Radio, Navigation, MediaBrowser, PhoneStatus, GenericNotification) from firmware analysis but don't have wire-level message definitions. open-androidauto will reject these channels cleanly until we can implement them.
- **Audio codec negotiation:** aasdk always uses PCM. Production SDKs support AAC. Should open-androidauto support AAC decode? (Deferred — PCM works, AAC is optimization.)
