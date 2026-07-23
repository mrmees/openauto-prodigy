# open-androidauto

Reusable C++/Qt library for the Android Auto wireless protocol. Provides everything needed to build an AA head unit application: transport, framing, encryption, session management, and protocol channel handlers.

## Modules

| Module | Namespace | Purpose |
|--------|-----------|---------|
| **Transport** | `oaa` | TCP transport + replay transport for testing |
| **Messenger** | `oaa` | Frame serialization/parsing, encryption, protocol logging |
| **Session** | `oaa` | AA session state machine, service discovery, channel registration |
| **Channel** | `oaa` | Base channel handler interfaces (`IChannelHandler`, `IAVChannelHandler`) |
| **HU/Handlers** | `oaa::hu` | Head unit channel handler implementations (Video, Audio, Input, Sensor, Bluetooth, WiFi, AVInput) |

## HU Handlers (`oaa::hu`)

Ready-to-use protocol handlers for all 7 AA channel types. Each handler parses incoming protobuf messages, builds responses, manages protocol state, and emits Qt signals for the host application to consume.

- `VideoChannelHandler` — H.264 video stream + focus negotiation
- `AudioChannelHandler` — Generic AV handler for media/speech/system audio (parameterized by channel ID)
- `AVInputChannelHandler` — Microphone upstream to phone
- `InputChannelHandler` — Touch events + key binding requests
- `SensorChannelHandler` — Night mode + driving status sensor events
- `BluetoothChannelHandler` — BT pairing requests
- `WiFiChannelHandler` — WiFi credential exchange

## Usage

```cpp
#include <oaa/HU/Handlers/VideoChannelHandler.hpp>
#include <oaa/HU/Handlers/SensorChannelHandler.hpp>
#include <oaa/Session/AASession.hpp>

oaa::hu::VideoChannelHandler videoHandler;
oaa::hu::SensorChannelHandler sensorHandler;

session->registerChannel(oaa::ChannelId::Video, &videoHandler);
session->registerChannel(oaa::ChannelId::Sensor, &sensorHandler);

connect(&videoHandler, &oaa::hu::VideoChannelHandler::videoFrameData,
        decoder, &VideoDecoder::decodeFrame);
```

The host owns registered handlers. Before destroying or replacing a session,
call `AASession::finalize()` while those handlers and the transport are still
alive. `stop()` is the graceful phone-visible shutdown; `finalize()` is the
idempotent local ownership boundary and performs no protocol write. A session
that reaches `Disconnected` may be started again with the same registrations;
its Messenger restarts with empty framing, assembly, and TLS state.

TLS handshakes distinguish retryable WANT-I/O from fatal OpenSSL results.
`Messenger::handshakeFailed` reports a bounded diagnostic immediately, and
`AASession` closes with `DisconnectReason::HandshakeError` instead of waiting
for the generic negotiation timeout. TLS initialization is transactional and
checks the embedded certificate/key pair before publishing an active object.
Established-session SSL reads and writes are also checked: an incomplete,
closed, or fatal encrypted AA frame is never forwarded as an empty payload, and
the session closes with `DisconnectReason::TlsError`. Fragmented messages retain
FIRST's declared total, are limited to 16 MiB each and 32 MiB in aggregate, and
must reach that total exactly on LAST with consistent flags. A malformed
sequence releases all partial state and closes with
`DisconnectReason::ProtocolError`. Channel-open responses are
always sent on the requested service channel. Registered service handlers
remain detached from the Messenger until their channel is opened, and
`Messenger::stop()` cancels any re-entrant or multi-frame send that has not
reached the transport.

## Dependencies

- Qt 6 (Core, Network)
- OpenSSL
- Protocol Buffers
