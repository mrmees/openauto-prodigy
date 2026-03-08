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

## Dependencies

- Qt 6 (Core, Network)
- OpenSSL
- Protocol Buffers
