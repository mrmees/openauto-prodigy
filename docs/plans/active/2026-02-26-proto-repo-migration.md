# Proto Repository Migration Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Extract 164 Android Auto protocol buffer definitions from `libs/open-androidauto/proto/` into the standalone `open-android-auto` GitHub repo, organized by channel with proper packaging.

**Architecture:** Create new repo with protos organized into 13 subdirectories under `proto/oaa/`. Update `import` paths only — **preserve existing package namespaces** (`oaa.proto.messages`, `oaa.proto.data`, `oaa.proto.enums`, `oaa.proto.ids`) to avoid breaking all C++ `oaa::proto::*` references. Consume as a git submodule in openauto-prodigy. Update C++ `#include` paths in 14 source files.

**CRITICAL:** File paths and protobuf package names are independent. Files move into `oaa/<category>/` directories for human organization, but their `package` declarations stay as-is to maintain generated C++ API compatibility (`oaa::proto::messages::*`, `oaa::proto::enums::*`, etc.).

**Tech Stack:** Protocol Buffers (proto3), CMake, Git submodules

**Design doc:** `docs/plans/active/2026-02-26-proto-repo-migration-design.md`

---

### Task 1: Create the open-android-auto repository

**Step 1: Create repo on GitHub**

Run:
```bash
cd /home/matt/claude/personal/openautopro
mkdir open-android-auto && cd open-android-auto
git init
```

**Step 2: Create directory structure**

Run:
```bash
mkdir -p proto/oaa/{common,control,av,video,audio,input,sensor,bluetooth,wifi,navigation,phone,media,notification}
mkdir -p docs
```

**Step 3: Create LICENSE file**

Write `LICENSE` with GPLv3 text.

**Step 4: Commit skeleton**

```bash
git add -A
git commit -m "chore: initial repository skeleton"
```

---

### Task 2: Copy and organize proto files into categories

Source directory: `/home/matt/claude/personal/openautopro/openauto-prodigy/libs/open-androidauto/proto/`
Target directory: `/home/matt/claude/personal/openautopro/open-android-auto/proto/oaa/`

**Step 1: Copy files into category directories**

The complete file-to-category mapping (164 files):

**common/ (11 files):**
```
StatusEnum.proto
ChannelTypeEnum.proto
ChannelErrorCodeEnum.proto
ConnectionStateEnum.proto
DisconnectReasonEnum.proto
SessionErrorEnum.proto
SessionInfoData.proto
FeatureFlagsData.proto
PingConfigurationData.proto
DiagnosticsData.proto
DriverPositionEnum.proto
```

**control/ (20 files):**
```
ControlMessageIdsEnum.proto
ServiceDiscoveryRequestMessage.proto
ServiceDiscoveryResponseMessage.proto
ChannelDescriptorData.proto
ChannelOpenRequestMessage.proto
ChannelOpenResponseMessage.proto
ConnectionConfigurationData.proto
PingRequestMessage.proto
PingResponseMessage.proto
ShutdownReasonEnum.proto
ShutdownRequestMessage.proto
ShutdownResponseMessage.proto
AuthCompleteIndicationMessage.proto
BindingRequestMessage.proto
BindingResponseMessage.proto
VersionResponseStatusEnum.proto
HeadUnitInfoData.proto
PhoneCapabilitiesData.proto
RadioChannelData.proto
VendorExtensionChannelData.proto
```

**av/ (15 files):**
```
AVChannelMessageIdsEnum.proto
AVChannelData.proto
AVChannelSetupRequestMessage.proto
AVChannelSetupResponseMessage.proto
AVChannelSetupStatusEnum.proto
AVChannelStartIndicationMessage.proto
AVChannelStopIndicationMessage.proto
AVChannelMediaConfigMessage.proto
AVChannelMediaStatsMessage.proto
AVInputChannelData.proto
AVInputOpenRequestMessage.proto
AVInputOpenResponseMessage.proto
AVMediaAckIndicationMessage.proto
AVStreamTypeEnum.proto
MediaCodecTypeEnum.proto
```

**video/ (9 files):**
```
VideoConfigData.proto
VideoResolutionEnum.proto
VideoFPSEnum.proto
VideoFocusRequestMessage.proto
VideoFocusIndicationMessage.proto
VideoFocusModeEnum.proto
VideoFocusReasonEnum.proto
DisplayTypeEnum.proto
AdditionalVideoConfigData.proto
```

**audio/ (7 files):**
```
AudioTypeEnum.proto
AudioConfigData.proto
AudioFocusChannelData.proto
AudioFocusRequestMessage.proto
AudioFocusResponseMessage.proto
AudioFocusStateEnum.proto
AudioFocusTypeEnum.proto
```

**input/ (20 files):**
```
InputChannelMessageIdsEnum.proto
InputChannelData.proto
InputChannelConfigData.proto
InputEventIndicationMessage.proto
TouchScreenConfigData.proto
TouchPadConfigData.proto
TouchConfigData.proto
TouchEventData.proto
TouchLocationData.proto
TouchCoordinateData.proto
TouchActionEnum.proto
AbsoluteInputEventData.proto
AbsoluteInputEventsData.proto
RelativeInputEventData.proto
RelativeInputEventsData.proto
ButtonEventData.proto
ButtonEventsData.proto
ButtonCodeEnum.proto
KeyEventData.proto
HapticFeedbackTypeEnum.proto
```

**sensor/ (33 files):**
```
SensorChannelMessageIdsEnum.proto
SensorChannelData.proto
SensorChannelConfigData.proto
SensorData.proto
SensorTypeEnum.proto
SensorEventIndicationMessage.proto
SensorStartRequestMessage.proto
SensorStartResponseMessage.proto
GPSLocationData.proto
AccelData.proto
GyroData.proto
CompassData.proto
SpeedData.proto
RPMData.proto
FuelLevelData.proto
DrivingStatusData.proto
DrivingStatusEnum.proto
NightModeData.proto
GearData.proto
GearEnum.proto
EnvironmentData.proto
ParkingBrakeData.proto
OdometerData.proto
TirePressureData.proto
SteeringWheelData.proto
DoorData.proto
LightData.proto
HeadlightStatusEnum.proto
IndicatorStatusEnum.proto
HVACData.proto
PassengerData.proto
EVConnectorTypeEnum.proto
FuelTypeEnum.proto
```

**bluetooth/ (7 files):**
```
BluetoothChannelMessageIdsEnum.proto
BluetoothChannelData.proto
BluetoothChannelConfigData.proto
BluetoothPairingMethodEnum.proto
BluetoothPairingRequestMessage.proto
BluetoothPairingResponseMessage.proto
BluetoothPairingStatusEnum.proto
```

**wifi/ (18 files):**
```
WifiChannelMessageIdsEnum.proto
WifiChannelData.proto
WifiProjectionChannelData.proto
WifiDirectConfigData.proto
WifiInfoRequestMessage.proto
WifiInfoResponseMessage.proto
WifiSecurityRequestMessage.proto
WifiSecurityResponseMessage.proto
WifiConnectionRejectionMessage.proto
WifiConnectStatusMessage.proto
WifiPingMessage.proto
WifiSetupInfoMessage.proto
WifiSetupMessageEnum.proto
WifiStartRequestMessage.proto
WifiStartResponseMessage.proto
WifiVersionRequestMessage.proto
WifiVersionResponseMessage.proto
WifiVersionStatusEnum.proto
```

**navigation/ (14 files):**
```
NavigationChannelData.proto
NavigationChannelConfigData.proto
NavigationImageOptionsData.proto
NavigationTypeEnum.proto
NavigationStateMessage.proto
NavigationNotificationMessage.proto
NavigationTurnEventMessage.proto
NavigationDistanceMessage.proto
NavigationDistanceDisplayData.proto
NavigationFocusRequestMessage.proto
NavigationFocusResponseMessage.proto
ManeuverTypeEnum.proto
LaneShapeEnum.proto
TurnSideEnum.proto
```

**phone/ (5 files):**
```
PhoneStatusChannelData.proto
PhoneStatusMessage.proto
PhoneCallStateEnum.proto
CallAvailabilityMessage.proto
VoiceSessionRequestMessage.proto
```

**media/ (3 files):**
```
MediaChannelData.proto
MediaPlaybackStatusMessage.proto
MediaPlaybackMetadataMessage.proto
```

**notification/ (2 files):**
```
NotificationChannelData.proto
NotificationTypeEnum.proto
```

**Step 2: Verify count**

Run:
```bash
find proto/oaa -name "*.proto" | wc -l
```
Expected: `164`

**Step 3: Commit**

```bash
git add proto/
git commit -m "feat: organize 164 AA protocol buffer definitions into categories"
```

---

### Task 3: Update import paths (preserve existing packages)

**IMPORTANT: Do NOT change package declarations.** The existing packages (`oaa.proto.messages`, `oaa.proto.data`, `oaa.proto.enums`, `oaa.proto.ids`) are used throughout the C++ codebase as `oaa::proto::messages::*`, `oaa::proto::enums::*`, etc. Changing them would require a massive C++ refactor beyond the scope of this migration.

Only update **import paths** — change from `"Foo.proto"` to `"oaa/<category>/Foo.proto"`.

**Cross-category import reference** (imports that cross directory boundaries):

| File (category) | Imports from |
|---|---|
| ServiceDiscoveryRequestMessage (control) | SessionInfoData (common) |
| ServiceDiscoveryResponseMessage (control) | DriverPositionEnum (common), HeadUnitInfoData (control), ChannelDescriptorData (control) |
| ChannelDescriptorData (control) | SensorChannelData (sensor), AVChannelData (av), InputChannelData (input), AVInputChannelData (av), BluetoothChannelData (bluetooth), NavigationChannelData (navigation), MediaChannelData (media), PhoneStatusChannelData (phone), NotificationChannelData (notification), WifiChannelData (wifi), RadioChannelData (control), VendorExtensionChannelData (control) |
| ChannelOpenResponseMessage (control) | StatusEnum (common) |
| AuthCompleteIndicationMessage (control) | StatusEnum (common) |
| BindingResponseMessage (control) | StatusEnum (common) |
| ConnectionConfigurationData (control) | PingConfigurationData (common) |
| AVChannelData (av) | AudioTypeEnum (audio), AudioConfigData (audio), VideoConfigData (video), DisplayTypeEnum (video), VideoFocusReasonEnum (video), AVStreamTypeEnum (av) |
| AVChannelMediaConfigMessage (av) | PingConfigurationData (common) |
| AVChannelMediaStatsMessage (av) | PingConfigurationData (common) |
| AVInputChannelData (av) | AudioConfigData (audio), AVStreamTypeEnum (av) |
| VideoConfigData (video) | MediaCodecTypeEnum (av), VideoResolutionEnum (video), VideoFPSEnum (video) |
| TouchScreenConfigData (input) | DisplayTypeEnum (video) |
| SensorEventIndicationMessage (sensor) | DiagnosticsData (common) + 19 intra-sensor imports |
| SensorStartResponseMessage (sensor) | StatusEnum (common) |
| WifiSetupInfoMessage (wifi) | FeatureFlagsData (common) |
| WifiVersionRequestMessage (wifi) | PhoneCapabilitiesData (control), FeatureFlagsData (common) |
| WifiVersionResponseMessage (wifi) | SessionInfoData (common) |

**Step 1: Update import paths**

For each `import "Foo.proto"` in every file, replace with `import "oaa/<category>/Foo.proto"` where `<category>` is determined by the category mapping in Task 2.

**Step 2: Preserve license headers, add project header to new files**

- **Files with existing f1x.studio/aasdk headers (103 files):** Keep the existing copyright/license notice intact. Optionally prepend a one-line project comment above it.
- **Files without headers (61 files — our additions):** Add:
```protobuf
// Android Auto Protocol Buffer Definitions
// https://github.com/mrmees/open-android-auto
// SPDX-License-Identifier: GPL-3.0-or-later
```

**Step 3: Verify all protos compile**

Run:
```bash
cd /home/matt/claude/personal/openautopro/open-android-auto
protoc --proto_path=proto --cpp_out=/tmp/proto_test proto/oaa/**/*.proto
```
Expected: no errors

**Step 5: Commit**

```bash
git add -A
git commit -m "feat: update import paths for organized directory layout"
```

---

### Task 4: Write README and documentation

**Step 1: Write README.md**

Content should cover:
- What this is (canonical AA protocol definitions)
- Directory structure with category descriptions
- How to use (protoc command examples for C++, Python, Go, Kotlin)
- Protocol overview (brief)
- Versioning policy
- Contributing guide summary
- License (GPLv3)
- Credit to f1x.studio/aasdk for original definitions

**Step 2: Write docs/protocol-overview.md**

High-level AA protocol explanation:
- Transport layers (BT discovery → WiFi → TCP)
- Framing (frame types, encryption)
- Channel multiplexing
- Service discovery flow
- Channel lifecycle

**Step 3: Write docs/channel-map.md**

Table mapping channel IDs → proto files → message directions (HU→phone / phone→HU).

**Step 4: Write docs/message-ids.md**

Table mapping hex message ID values → proto message types per channel.

**Step 5: Write docs/field-notes.md**

Hard-won protocol knowledge from CLAUDE.md gotchas and session memory:
- `touch_screen_config` must match video resolution
- Phone sends unexpected `config_index` values
- `MessageType::Control` vs `MessageType::Specific`
- SPS/PPS encoding quirks
- BSSID protobuf field encoding
- FFmpeg `thread_count` must be 1
- WiFi SSID/password must match hostapd exactly
- `AV_PIX_FMT_YUVJ420P` vs `YUV420P` format differences

**Step 6: Write CONTRIBUTING.md**

How to contribute: adding new messages, field documentation, testing.

**Step 7: Commit**

```bash
git add -A
git commit -m "docs: add README, protocol overview, channel map, and field notes"
```

---

### Task 5: Push to GitHub

**Step 1: Create GitHub repo**

```bash
gh repo create mrmees/open-android-auto --public --description "Android Auto protocol buffer definitions — the canonical open-source reference for the AA wire protocol" --license gpl-3.0
```

**Step 2: Push**

```bash
cd /home/matt/claude/personal/openautopro/open-android-auto
git remote add origin git@github.com:mrmees/open-android-auto.git
git branch -M main
git push -u origin main
```

**Step 3: Tag initial release**

```bash
git tag -a v0.1.0 -m "Initial release: 164 AA protocol buffer definitions"
git push origin v0.1.0
```

---

### Task 6: Add submodule to openauto-prodigy

**Step 1: Add submodule**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
git submodule add git@github.com:mrmees/open-android-auto.git libs/open-androidauto/proto/open-android-auto
```

**Step 2: Verify submodule**

```bash
ls libs/open-androidauto/proto/open-android-auto/proto/oaa/common/StatusEnum.proto
```
Expected: file exists

**Step 3: Commit**

```bash
git add .gitmodules libs/open-androidauto/proto/open-android-auto
git commit -m "chore: add open-android-auto proto submodule"
```

---

### Task 7: Update CMakeLists.txt for new proto paths

**Files:**
- Modify: `libs/open-androidauto/CMakeLists.txt`

**Step 1: Update CMake proto configuration**

Replace:
```cmake
file(GLOB PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})
```

With:
```cmake
file(GLOB_RECURSE PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/proto/open-android-auto/proto/oaa/*.proto)

# Set import root so protoc resolves "oaa/..." imports
set(Protobuf_IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/proto/open-android-auto/proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})
```

**Step 2: Verify the build still finds protobuf headers**

The `target_include_directories` already includes `${CMAKE_CURRENT_BINARY_DIR}` which is where `protobuf_generate_cpp` puts generated headers. The generated headers will now be in subdirectories matching the proto layout (`oaa/common/`, `oaa/control/`, etc.), so includes will be `"oaa/control/ServiceDiscoveryRequestMessage.pb.h"`.

**Step 3: Commit**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
git add libs/open-androidauto/CMakeLists.txt
git commit -m "build: update CMake for reorganized proto submodule paths"
```

---

### Task 8: Checkpoint — verify proto generation works

Before touching C++ code, verify the proto reorganization produces correct generated headers.

**Step 1: Check generated header paths**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build
cmake ..
# Check that generated headers are in expected subdirectory structure
find . -name "*.pb.h" -path "*/oaa/*" | head -10
```
Expected: headers like `./oaa/control/ServiceDiscoveryRequestMessage.pb.h`

**Step 2: Verify namespace preservation**

```bash
grep "namespace oaa" $(find . -name "ServiceDiscoveryRequestMessage.pb.h" -path "*/oaa/*") | head -5
```
Expected: `namespace oaa { namespace proto { namespace messages {` — confirming packages were NOT changed.

**Step 3: Scripted validation — check all .pb.h files referenced in C++ exist**

```bash
# Extract all .pb.h includes from source, verify each has a generated counterpart
cd /home/matt/claude/personal/openautopro/openauto-prodigy
for f in $(grep -rh '#include ".*\.pb\.h"' libs/open-androidauto/src/ libs/open-androidauto/tests/ | sed 's/.*#include "\(.*\)"/\1/' | sort -u); do
    if [ ! -f "build/libs/open-androidauto/$f" ] && [ ! -f "build/libs/open-androidauto/open-androidauto_autogen/$f" ]; then
        echo "MISSING: $f"
    fi
done
```
Expected: no MISSING lines (or only the ones we haven't updated yet in Task 9)

---

### Task 9: Update C++ #include paths for generated protobuf headers

**Files to modify (14 total):**

**`src/Channel/ControlChannel.cpp`** — update includes:
```cpp
#include "oaa/control/PingRequestMessage.pb.h"
#include "oaa/control/PingResponseMessage.pb.h"
#include "oaa/control/ChannelOpenRequestMessage.pb.h"
#include "oaa/control/ChannelOpenResponseMessage.pb.h"
#include "oaa/control/AuthCompleteIndicationMessage.pb.h"
#include "oaa/control/ShutdownRequestMessage.pb.h"
#include "oaa/control/ShutdownResponseMessage.pb.h"
#include "oaa/common/StatusEnum.pb.h"
#include "oaa/control/ShutdownReasonEnum.pb.h"
#include "oaa/phone/CallAvailabilityMessage.pb.h"
#include "oaa/control/ServiceDiscoveryRequestMessage.pb.h"
```

**`src/Session/AASession.cpp`** — update includes:
```cpp
#include "oaa/control/ServiceDiscoveryRequestMessage.pb.h"
#include "oaa/control/ServiceDiscoveryResponseMessage.pb.h"
#include "oaa/control/ChannelDescriptorData.pb.h"
#include "oaa/control/ChannelOpenRequestMessage.pb.h"
#include "oaa/control/ChannelOpenResponseMessage.pb.h"
#include "oaa/common/StatusEnum.pb.h"
#include "oaa/audio/AudioFocusRequestMessage.pb.h"
#include "oaa/audio/AudioFocusResponseMessage.pb.h"
#include "oaa/audio/AudioFocusTypeEnum.pb.h"
#include "oaa/audio/AudioFocusStateEnum.pb.h"
#include "oaa/common/DriverPositionEnum.pb.h"
```

**`src/HU/Handlers/AudioChannelHandler.cpp`** — update includes:
```cpp
#include "oaa/av/AVChannelSetupRequestMessage.pb.h"
#include "oaa/av/AVChannelSetupResponseMessage.pb.h"
#include "oaa/av/AVChannelSetupStatusEnum.pb.h"
#include "oaa/av/AVChannelStartIndicationMessage.pb.h"
#include "oaa/av/AVChannelStopIndicationMessage.pb.h"
#include "oaa/av/AVMediaAckIndicationMessage.pb.h"
```

**`src/HU/Handlers/VideoChannelHandler.cpp`** — update includes:
```cpp
#include "oaa/av/AVChannelSetupRequestMessage.pb.h"
#include "oaa/av/AVChannelSetupResponseMessage.pb.h"
#include "oaa/av/AVChannelSetupStatusEnum.pb.h"
#include "oaa/av/AVChannelStartIndicationMessage.pb.h"
#include "oaa/av/AVChannelStopIndicationMessage.pb.h"
#include "oaa/av/AVMediaAckIndicationMessage.pb.h"
#include "oaa/video/VideoFocusRequestMessage.pb.h"
#include "oaa/video/VideoFocusIndicationMessage.pb.h"
#include "oaa/video/VideoFocusModeEnum.pb.h"
#include "oaa/video/VideoFocusReasonEnum.pb.h"
```

**`src/HU/Handlers/AVInputChannelHandler.cpp`** — update includes:
```cpp
#include "oaa/av/AVInputOpenRequestMessage.pb.h"
#include "oaa/av/AVInputOpenResponseMessage.pb.h"
```

**`src/HU/Handlers/BluetoothChannelHandler.cpp`** — update includes:
```cpp
#include "oaa/bluetooth/BluetoothPairingRequestMessage.pb.h"
#include "oaa/bluetooth/BluetoothPairingResponseMessage.pb.h"
#include "oaa/bluetooth/BluetoothPairingStatusEnum.pb.h"
```

**`src/HU/Handlers/InputChannelHandler.cpp`** — update includes:
```cpp
#include "oaa/input/InputEventIndicationMessage.pb.h"
#include "oaa/input/TouchEventData.pb.h"
#include "oaa/input/TouchLocationData.pb.h"
#include "oaa/input/TouchActionEnum.pb.h"
#include "oaa/control/BindingRequestMessage.pb.h"
#include "oaa/control/BindingResponseMessage.pb.h"
#include "oaa/common/StatusEnum.pb.h"
```

**`src/HU/Handlers/SensorChannelHandler.cpp`** — update includes:
```cpp
#include "oaa/sensor/SensorStartRequestMessage.pb.h"
#include "oaa/sensor/SensorStartResponseMessage.pb.h"
#include "oaa/sensor/SensorEventIndicationMessage.pb.h"
#include "oaa/sensor/NightModeData.pb.h"
#include "oaa/sensor/DrivingStatusData.pb.h"
#include "oaa/sensor/ParkingBrakeData.pb.h"
#include "oaa/common/StatusEnum.pb.h"
#include "oaa/sensor/SensorTypeEnum.pb.h"
```

**`src/HU/Handlers/WiFiChannelHandler.cpp`** — update includes:
```cpp
#include "oaa/wifi/WifiSecurityRequestMessage.pb.h"
#include "oaa/wifi/WifiSecurityResponseMessage.pb.h"
```

**`src/HU/Handlers/NavigationChannelHandler.cpp`** — update includes:
```cpp
#include "oaa/navigation/NavigationStateMessage.pb.h"
#include "oaa/navigation/NavigationNotificationMessage.pb.h"
#include "oaa/navigation/NavigationDistanceMessage.pb.h"
```

**`src/HU/Handlers/MediaStatusChannelHandler.cpp`** — update includes:
```cpp
#include "oaa/media/MediaPlaybackStatusMessage.pb.h"
#include "oaa/media/MediaPlaybackMetadataMessage.pb.h"
```

**`src/HU/Handlers/PhoneStatusChannelHandler.cpp`** — update includes:
```cpp
#include "oaa/phone/PhoneStatusMessage.pb.h"
```

**`tests/test_control_channel.cpp`** — update includes:
```cpp
#include "oaa/control/PingRequestMessage.pb.h"
#include "oaa/control/PingResponseMessage.pb.h"
#include "oaa/control/ChannelOpenRequestMessage.pb.h"
#include "oaa/control/ChannelOpenResponseMessage.pb.h"
#include "oaa/control/ShutdownRequestMessage.pb.h"
#include "oaa/common/StatusEnum.pb.h"
#include "oaa/control/ShutdownReasonEnum.pb.h"
```

**`tests/test_session_fsm.cpp`** — update includes:
```cpp
#include "oaa/control/ChannelOpenRequestMessage.pb.h"
#include "oaa/control/ChannelOpenResponseMessage.pb.h"
#include "oaa/control/ShutdownRequestMessage.pb.h"
#include "oaa/control/ShutdownResponseMessage.pb.h"
#include "oaa/control/ShutdownReasonEnum.pb.h"
#include "oaa/common/StatusEnum.pb.h"
#include "oaa/control/ServiceDiscoveryRequestMessage.pb.h"
#include "oaa/control/ServiceDiscoveryResponseMessage.pb.h"
```

**Step: Commit**

```bash
git add libs/open-androidauto/src/ libs/open-androidauto/tests/
git commit -m "refactor: update C++ includes for reorganized proto paths"
```

---

### Task 10: Build and test

**Step 1: Local build (x86)**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build
cmake .. && make -j$(nproc)
```
Expected: clean build, no errors

**Step 2: Run tests**

```bash
ctest --output-on-failure
```
Expected: all tests pass

**Step 3: Cross-compile for Pi**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build-pi
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-pi4.cmake .. && make -j$(nproc)
```
Expected: clean build

**Step 4: Commit any fixups if needed**

---

### Task 11: Clean up old proto directory

**Step 1: Remove old proto files**

Delete the original flat proto files from `libs/open-androidauto/proto/` (everything except the `open-android-auto/` submodule directory).

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/libs/open-androidauto/proto/
# Remove all .proto files at this level (not in subdirectories)
rm *.proto
```

**Step 2: Verify build still works**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy/build
cmake .. && make -j$(nproc) && ctest --output-on-failure
```

**Step 3: Commit**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
git add libs/open-androidauto/proto/
git commit -m "chore: remove old flat proto files (now in open-android-auto submodule)"
```

---

### Task 12: Final push and cleanup

**Step 1: Push openauto-prodigy changes**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
git push origin main
```

**Step 2: Update session handoffs**

Update `docs/session-handoffs.md` with the migration completion.

**Step 3: Update roadmap**

Update `docs/roadmap-current.md` to reflect proto migration is done.
