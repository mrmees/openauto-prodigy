# Design: open-android-auto Proto Repository Migration

**Date:** 2026-02-26
**Status:** Approved

## Goal

Extract the 164 Android Auto protocol buffer definitions from `libs/open-androidauto/proto/` into a standalone community repository. This becomes the canonical open-source reference for the AA wire protocol — language-agnostic, well-organized, and documented.

## Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Repo name | `open-android-auto` | Matches the library identity |
| License | GPLv3 | Consistent with original aasdk heritage |
| Scope | Proto files + docs only | No generated code; consumers handle codegen |
| Consumption | Git submodule | Version-pinned, familiar, no network dependency at build time |
| Organization | Subdirectories by channel | Self-documenting structure that teaches protocol architecture |
| Package root | `oaa` | Matches existing C++ namespace and proto packages |
| Initial version | v0.1.0 | Confident but inviting community refinement |

## Repository Structure

```
open-android-auto/
├── README.md
├── LICENSE                          # GPLv3
├── CHANGELOG.md
├── CONTRIBUTING.md
├── proto/
│   └── oaa/
│       ├── common/                  # Shared enums & base types
│       │   ├── StatusEnum.proto
│       │   ├── ChannelTypeEnum.proto
│       │   ├── ChannelErrorCodeEnum.proto
│       │   ├── ConnectionStateEnum.proto
│       │   ├── DisconnectReasonEnum.proto
│       │   ├── SessionErrorEnum.proto
│       │   ├── SessionInfoData.proto
│       │   ├── FeatureFlagsData.proto
│       │   ├── PingConfigurationData.proto
│       │   ├── DiagnosticsData.proto
│       │   └── ...
│       ├── control/                 # Control channel — session lifecycle
│       │   ├── ControlMessageIdsEnum.proto
│       │   ├── ServiceDiscoveryRequestMessage.proto
│       │   ├── ServiceDiscoveryResponseMessage.proto
│       │   ├── ChannelDescriptorData.proto
│       │   ├── ChannelOpenRequestMessage.proto
│       │   ├── ChannelOpenResponseMessage.proto
│       │   ├── PingRequestMessage.proto
│       │   ├── PingResponseMessage.proto
│       │   ├── ShutdownRequestMessage.proto
│       │   ├── ShutdownResponseMessage.proto
│       │   ├── ShutdownReasonEnum.proto
│       │   ├── AuthCompleteIndicationMessage.proto
│       │   ├── BindingRequestMessage.proto
│       │   ├── BindingResponseMessage.proto
│       │   ├── VersionResponseStatusEnum.proto
│       │   ├── HeadUnitInfoData.proto
│       │   ├── ConnectionConfigurationData.proto
│       │   └── ...
│       ├── video/                   # Video channel
│       │   ├── VideoConfigData.proto
│       │   ├── VideoResolutionEnum.proto
│       │   ├── VideoFPSEnum.proto
│       │   ├── VideoFocusRequestMessage.proto
│       │   ├── VideoFocusIndicationMessage.proto
│       │   ├── VideoFocusModeEnum.proto
│       │   ├── VideoFocusReasonEnum.proto
│       │   ├── AdditionalVideoConfigData.proto
│       │   ├── DisplayTypeEnum.proto
│       │   └── ...
│       ├── audio/                   # Audio channels (media, nav, phone)
│       │   ├── AudioConfigData.proto
│       │   ├── AudioTypeEnum.proto
│       │   ├── AudioFocusRequestMessage.proto
│       │   ├── AudioFocusResponseMessage.proto
│       │   ├── AudioFocusStateEnum.proto
│       │   ├── AudioFocusTypeEnum.proto
│       │   ├── AudioFocusChannelData.proto
│       │   └── ...
│       ├── av/                      # Shared AV types (used by both video & audio)
│       │   ├── AVChannelData.proto
│       │   ├── AVChannelMessageIdsEnum.proto
│       │   ├── AVChannelSetupRequestMessage.proto
│       │   ├── AVChannelSetupResponseMessage.proto
│       │   ├── AVChannelSetupStatusEnum.proto
│       │   ├── AVChannelStopIndicationMessage.proto
│       │   ├── AVChannelMediaConfigMessage.proto
│       │   ├── AVChannelMediaStatsMessage.proto
│       │   ├── AVInputChannelData.proto
│       │   ├── AVInputOpenRequestMessage.proto
│       │   ├── AVInputOpenResponseMessage.proto
│       │   ├── AVMediaAckIndicationMessage.proto
│       │   ├── AVStreamTypeEnum.proto
│       │   ├── MediaCodecTypeEnum.proto
│       │   └── ...
│       ├── input/                   # Input channel
│       │   ├── InputChannelMessageIdsEnum.proto
│       │   ├── InputChannelData.proto
│       │   ├── InputChannelConfigData.proto
│       │   ├── InputEventIndicationMessage.proto
│       │   ├── TouchScreenConfigData.proto
│       │   ├── TouchEventData.proto
│       │   ├── TouchActionEnum.proto
│       │   ├── AbsoluteInputEventData.proto
│       │   ├── AbsoluteInputEventsData.proto
│       │   ├── RelativeInputEventData.proto
│       │   ├── RelativeInputEventsData.proto
│       │   ├── ButtonEventData.proto
│       │   ├── ButtonEventsData.proto
│       │   ├── ButtonCodeEnum.proto
│       │   ├── HapticFeedbackTypeEnum.proto
│       │   └── ...
│       ├── sensor/                  # Sensor channel
│       │   ├── SensorChannelMessageIdsEnum.proto
│       │   ├── SensorChannelData.proto
│       │   ├── SensorTypeEnum.proto
│       │   ├── SensorData.proto
│       │   ├── SensorEventIndicationMessage.proto
│       │   ├── SensorStartRequestMessage.proto
│       │   ├── SensorStartResponseMessage.proto
│       │   ├── GPSLocationData.proto
│       │   ├── AccelData.proto
│       │   ├── GyroData.proto
│       │   ├── CompassData.proto
│       │   ├── SpeedData.proto
│       │   ├── RPMData.proto
│       │   ├── FuelLevelData.proto
│       │   ├── DrivingStatusData.proto
│       │   ├── DrivingStatusEnum.proto
│       │   ├── NightModeData.proto
│       │   ├── GearData.proto
│       │   ├── GearEnum.proto
│       │   ├── EnvironmentData.proto
│       │   ├── ParkingBrakeData.proto
│       │   ├── OdometerData.proto
│       │   ├── TirePressureData.proto
│       │   ├── SteeringWheelData.proto
│       │   ├── DoorData.proto
│       │   ├── LightData.proto
│       │   ├── HeadlightStatusEnum.proto
│       │   ├── IndicatorStatusEnum.proto
│       │   ├── HVACData.proto
│       │   ├── PassengerData.proto
│       │   ├── EVConnectorTypeEnum.proto
│       │   ├── FuelTypeEnum.proto
│       │   └── ...
│       ├── bluetooth/               # Bluetooth channel
│       │   ├── BluetoothChannelMessageIdsEnum.proto
│       │   ├── BluetoothChannelData.proto
│       │   ├── BluetoothChannelConfigData.proto
│       │   ├── BluetoothPairingRequestMessage.proto
│       │   ├── BluetoothPairingResponseMessage.proto
│       │   ├── BluetoothPairingMethodEnum.proto
│       │   ├── BluetoothPairingStatusEnum.proto
│       │   └── ...
│       ├── wifi/                    # WiFi projection channel
│       │   ├── WifiChannelMessageIdsEnum.proto
│       │   ├── WifiProjectionChannelData.proto
│       │   ├── WifiChannelData.proto
│       │   ├── WifiDirectConfigData.proto
│       │   ├── WifiInfoRequestMessage.proto
│       │   ├── WifiInfoResponseMessage.proto
│       │   ├── WifiSecurityRequestMessage.proto
│       │   ├── WifiSecurityResponseMessage.proto
│       │   ├── WifiSetupInfoMessage.proto
│       │   ├── WifiStartRequestMessage.proto
│       │   ├── WifiStartResponseMessage.proto
│       │   ├── WifiVersionRequestMessage.proto
│       │   ├── WifiVersionResponseMessage.proto
│       │   ├── WifiVersionStatusEnum.proto
│       │   └── ...
│       ├── navigation/              # Navigation status channel
│       │   ├── NavigationChannelData.proto
│       │   ├── NavigationStateMessage.proto
│       │   ├── NavigationNotificationMessage.proto
│       │   ├── NavigationTurnEventMessage.proto
│       │   ├── NavigationDistanceMessage.proto
│       │   ├── NavigationDistanceDisplayData.proto
│       │   ├── NavigationImageOptionsData.proto
│       │   ├── NavigationTypeEnum.proto
│       │   ├── ManeuverTypeEnum.proto
│       │   ├── LaneShapeEnum.proto
│       │   ├── TurnSideEnum.proto
│       │   └── ...
│       ├── phone/                   # Phone status channel
│       │   ├── PhoneStatusChannelData.proto
│       │   ├── PhoneStatusMessage.proto
│       │   ├── PhoneCallStateEnum.proto
│       │   ├── PhoneCapabilitiesData.proto
│       │   ├── CallAvailabilityMessage.proto
│       │   ├── VoiceSessionRequestMessage.proto
│       │   └── ...
│       ├── media/                   # Media status channel
│       │   ├── MediaChannelData.proto
│       │   ├── MediaPlaybackStatusMessage.proto
│       │   ├── MediaPlaybackMetadataMessage.proto
│       │   └── ...
│       └── notification/            # Notification channel
│           ├── NotificationChannelData.proto
│           ├── NotificationTypeEnum.proto
│           └── ...
└── docs/
    ├── protocol-overview.md         # High-level AA architecture
    ├── channel-map.md               # Channel ID → message type mapping
    ├── message-ids.md               # Hex message IDs → proto mapping
    └── field-notes.md               # Hard-won protocol gotchas
```

## Package Convention

```protobuf
syntax = "proto3";
package oaa.control;

import "oaa/common/StatusEnum.proto";
```

Generated namespaces per language:
- **C++:** `oaa::control::ServiceDiscoveryRequest`
- **Python:** `oaa.control`
- **Kotlin/Java:** `oaa.control`
- **Go:** `oaa/control`

## Consumption from openauto-prodigy

Submodule mounted at `libs/open-androidauto/proto/open-android-auto/`:

```cmake
file(GLOB_RECURSE PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/open-android-auto/proto/oaa/*.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Import resolution root
target_include_directories(open-androidauto PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}  # generated headers land here
)
set(Protobuf_IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto/open-android-auto/proto)
```

## C++ Include Path Changes

Generated header paths change:
```cpp
// Before
#include "ServiceDiscoveryRequestMessage.pb.h"

// After
#include "oaa/control/ServiceDiscoveryRequestMessage.pb.h"
```

Mechanical find-and-replace across `libs/open-androidauto/` source files.

## Documentation Content

### `docs/protocol-overview.md`
High-level explanation of the AA protocol: transport layers, framing, encryption handshake, channel multiplexing, service discovery flow.

### `docs/channel-map.md`
Maps AA channel IDs to their proto files and message types. Shows which channel each message belongs to and the direction (HU→phone vs phone→HU).

### `docs/message-ids.md`
Maps hex message ID values to proto message types. Essential for anyone decoding raw AA traffic.

### `docs/field-notes.md`
All the protocol gotchas learned through reverse engineering:
- `touch_screen_config` must be video resolution, not display resolution
- Phone sends `config_index=3` regardless of config list size
- `MessageType::Control` vs `MessageType::Specific` per channel type
- SPS/PPS arrives as `AV_MEDIA_INDICATION` with no timestamp
- BSSID encoding: field 3, length-delimited string
- FFmpeg `thread_count` must be 1 for real-time AA decode
- And more from CLAUDE.md gotchas section

## Migration Sequence (High Level)

1. Create `open-android-auto` repo on GitHub
2. Categorize all 164 protos into subdirectories
3. Update all `package` declarations and `import` paths
4. Write README, docs, LICENSE
5. Push initial commit
6. Add as submodule to `openauto-prodigy`
7. Update `open-androidauto` CMakeLists.txt for new paths
8. Update all C++ `#include` paths for generated headers
9. Verify build (local + cross-compile)
10. Remove old `proto/` directory from `open-androidauto`
