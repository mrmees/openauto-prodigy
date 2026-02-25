# Proto Validation Report

Generated: 2026-02-25

## Summary

| Status | Count |
|--------|-------|
| Verified | 45 |
| Partial (diffs) | 54 |
| Unmatched | 88 |
| Error | 0 |
| **Total** | **187** |

## Verified (all fields match)

- **AVChannelSetupResponse** (AVChannelSetupResponseMessage.proto) -> `vxb` [auto]
- **AVInputChannel** (AVInputChannelData.proto) -> `vyt` [manual]
- **AVInputOpenRequest** (AVInputOpenRequestMessage.proto) -> `vyx` [auto]
- **AudioConfig** (AudioConfigData.proto) -> `vvp` [manual]
- **BindingRequest** (BindingRequestMessage.proto) -> `wcg` [auto]
- **BluetoothChannel** (BluetoothChannelData.proto) -> `vwc` [manual]
- **BluetoothPairingRequest** (BluetoothPairingRequestMessage.proto) -> `kay` [auto]
- **ButtonEvent** (ButtonEventData.proto) -> `vyi` [auto]
- **CallAvailabilityStatus** (CallAvailabilityMessage.proto) -> `poe` [auto]
- **ChargingStationDetails** (NavigationStepMessage.proto) -> `vwz` [manual]
- **ConnectionFeatureFlags** (ConnectionConfigurationData.proto) -> `aaja` [manual]
- **Door** (DoorData.proto) -> `vxh` [auto]
- **FuelLevel** (FuelLevelData.proto) -> `vxn` [auto]
- **HeadUnitInfo** (HeadUnitInfoData.proto) -> `aacd` [auto]
- **InputChannel** (InputChannelData.proto) -> `vya` [manual]
- **Light** (LightData.proto) -> `vyk` [auto]
- **MediaPlaybackMetadata** (MediaPlaybackMetadataMessage.proto) -> `nmi` [auto]
- **NavigationChannel** (NavigationChannelData.proto) -> `vzr` [manual]
- **NavigationDestination** (NavigationStepMessage.proto) -> `vze` [manual]
- **NavigationLane** (NavigationStepMessage.proto) -> `vzj` [manual]
- **NavigationLaneDirection** (NavigationStepMessage.proto) -> `vzi` [manual]
- **NavigationManeuver** (NavigationStepMessage.proto) -> `vzk` [manual]
- **NavigationNotification** (NavigationStepMessage.proto) -> `vzo` [manual]
- **NavigationRoadInfo** (NavigationStepMessage.proto) -> `vzc` [manual]
- **NavigationStep** (NavigationStepMessage.proto) -> `vzu` [manual]
- **NavigationText** (NavigationStepMessage.proto) -> `vzn` [manual]
- **NightMode** (NightModeData.proto) -> `poe` [auto]
- **ParkingBrake** (ParkingBrakeData.proto) -> `poe` [auto]
- **Passenger** (PassengerData.proto) -> `poe` [auto]
- **PhoneCall** (PhoneStatusMessage.proto) -> `wad` [manual]
- **PhoneStatusChannel** (PhoneStatusChannelData.proto) -> `vyr` [manual]
- **PhoneStatusUpdate** (PhoneStatusMessage.proto) -> `waf` [manual]
- **PingRequest** (PingRequestMessage.proto) -> `zrz` [auto]
- **PingResponse** (PingResponseMessage.proto) -> `zrz` [auto]
- **SensorChannel** (SensorChannelData.proto) -> `wbu` [manual]
- **SensorStartRequestMessage** (SensorStartRequestMessage.proto) -> `war` [auto]
- **ServiceDiscoveryRequest** (ServiceDiscoveryRequestMessage.proto) -> `wbx` [manual]
- **ServiceDiscoveryResponse** (ServiceDiscoveryResponseMessage.proto) -> `wby` [manual]
- **SessionInfo** (SessionInfoData.proto) -> `vvf` [manual]
- **TirePressure** (TirePressureData.proto) -> `vzy` [auto]
- **TouchEvent** (TouchEventData.proto) -> `wcj` [auto]
- **VendorExtensionChannel** (VendorExtensionChannelData.proto) -> `wcv` [auto]
- **VideoFocusIndication** (VideoFocusIndicationMessage.proto) -> `wdb` [auto]
- **WifiChannel** (WifiChannelData.proto) -> `wdh` [manual]
- **WifiInfoRequest** (WifiInfoRequestMessage.proto) -> `zui` [auto]

## Partial (field diffs)

### AVChannel (AVChannelData.proto) -> `vys` [manual]

Matched: 7/8 our fields, APK has 8 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 8 | type mismatch | int32 | enum |

### AVChannelSetupRequest (AVChannelSetupRequestMessage.proto) -> `aagl` [auto]

Matched: 1/1 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | enum |
| 3 | missing field (exists in APK) | None | enum |
| 4 | missing field (exists in APK) | None | enum |

### AVChannelStartIndication (AVChannelStartIndicationMessage.proto) -> `vxn` [auto]

Matched: 2/2 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | missing field (exists in APK) | None | bool |

### AVInputOpenResponse (AVInputOpenResponseMessage.proto) -> `vxn` [auto]

Matched: 2/2 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | missing field (exists in APK) | None | bool |

### AVMediaAckIndication (AVMediaAckIndicationMessage.proto) -> `vxn` [auto]

Matched: 2/2 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | missing field (exists in APK) | None | bool |

### AbsoluteInputEvent (AbsoluteInputEventData.proto) -> `vxn` [auto]

Matched: 2/2 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | missing field (exists in APK) | None | bool |

### AbsoluteInputEvents (AbsoluteInputEventsData.proto) -> `ufg` [auto]

Matched: 1/1 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | message |
| 3 | missing field (exists in APK) | None | enum |

### Accel (AccelData.proto) -> `vzy` [auto]

Matched: 3/3 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 4 | missing field (exists in APK) | None | int32 |

### AudioFocusRequest (AudioFocusRequestMessage.proto) -> `aass` [auto]

Matched: 1/1 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | message |
| 3 | missing field (exists in APK) | None | string |
| 4 | missing field (exists in APK) | None | string |

### AudioFocusResponse (AudioFocusResponseMessage.proto) -> `aass` [auto]

Matched: 1/1 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | message |
| 3 | missing field (exists in APK) | None | string |
| 4 | missing field (exists in APK) | None | string |

### AuthCompleteIndication (AuthCompleteIndicationMessage.proto) -> `aass` [auto]

Matched: 1/1 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | message |
| 3 | missing field (exists in APK) | None | string |
| 4 | missing field (exists in APK) | None | string |

### BindingResponse (BindingResponseMessage.proto) -> `aass` [auto]

Matched: 1/1 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | message |
| 3 | missing field (exists in APK) | None | string |
| 4 | missing field (exists in APK) | None | string |

### BluetoothPairingResponse (BluetoothPairingResponseMessage.proto) -> `xgq` [auto]

Matched: 2/3 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | type mismatch | string | enum |

### ButtonEvents (ButtonEventsData.proto) -> `ufg` [auto]

Matched: 1/1 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | message |
| 3 | missing field (exists in APK) | None | enum |

### ChannelDescriptor (ChannelDescriptorData.proto) -> `wbw` [manual]

Matched: 13/17 our fields, APK has 17 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 11 | type mismatch | bytes | message |
| 15 | type mismatch | bytes | message |
| 16 | type mismatch | bytes | message |
| 17 | type mismatch | bytes | message |

### ChannelOpenRequest (ChannelOpenRequestMessage.proto) -> `vxn` [auto]

Matched: 2/2 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | missing field (exists in APK) | None | bool |

### ChannelOpenResponse (ChannelOpenResponseMessage.proto) -> `aass` [auto]

Matched: 1/1 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | message |
| 3 | missing field (exists in APK) | None | string |
| 4 | missing field (exists in APK) | None | string |

### Compass (CompassData.proto) -> `vzy` [auto]

Matched: 3/3 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 4 | missing field (exists in APK) | None | int32 |

### ConnectionConfiguration (ConnectionConfigurationData.proto) -> `aajk` [manual]

Matched: 1/1 our fields, APK has 6 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | message |
| 3 | missing field (exists in APK) | None | message |
| 4 | missing field (exists in APK) | None | message |
| 5 | missing field (exists in APK) | None | message |
| 6 | missing field (exists in APK) | None | message |

### ConnectionSecurityConfig (ConnectionConfigurationData.proto) -> `aajj` [manual]

Matched: 0/1 our fields, APK has 1 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | type mismatch | int32 | enum |

### ConnectionTuningConfig (ConnectionConfigurationData.proto) -> `aajg` [manual]

Matched: 1/1 our fields, APK has 5 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 60 | missing field (exists in APK) | None | double |

### Diagnostics (DiagnosticsData.proto) -> `xss` [auto]

Matched: 2/3 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | cardinality mismatch | optional | repeated |

### DrivingStatus (DrivingStatusEnum.proto) -> `aagl` [auto]

Matched: 0/0 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | missing field (exists in APK) | None | int32 |
| 2 | missing field (exists in APK) | None | enum |
| 3 | missing field (exists in APK) | None | enum |
| 4 | missing field (exists in APK) | None | enum |

### Environment (EnvironmentData.proto) -> `vzy` [auto]

Matched: 3/3 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 4 | missing field (exists in APK) | None | int32 |

### GPSLocation (GPSLocationData.proto) -> `ahdz` [auto]

Matched: 17/18 our fields, APK has 22 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | type mismatch | uint64 | int32 |
| 12 | missing field (exists in APK) | None | int32 |
| 13 | missing field (exists in APK) | None | int32 |
| 23 | missing field (exists in APK) | None | int64 |
| 24 | missing field (exists in APK) | None | int64 |

### Gear (GearEnum.proto) -> `aass` [auto]

Matched: 0/0 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | missing field (exists in APK) | None | enum |
| 2 | missing field (exists in APK) | None | message |
| 3 | missing field (exists in APK) | None | string |
| 4 | missing field (exists in APK) | None | string |

### Gyro (GyroData.proto) -> `vzy` [auto]

Matched: 3/3 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 4 | missing field (exists in APK) | None | int32 |

### HVAC (HVACData.proto) -> `vxn` [auto]

Matched: 2/2 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | missing field (exists in APK) | None | bool |

### InputEventIndication (InputEventIndicationMessage.proto) -> `vxx` [auto]

Matched: 6/7 our fields, APK has 6 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | extra field in our proto (not in APK) | disp_channel | None |

### MediaPlaybackStatus (MediaPlaybackStatusMessage.proto) -> `ahdz` [auto]

Matched: 17/18 our fields, APK has 22 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | type mismatch | string | int32 |
| 12 | missing field (exists in APK) | None | int32 |
| 13 | missing field (exists in APK) | None | int32 |
| 23 | missing field (exists in APK) | None | int64 |
| 24 | missing field (exists in APK) | None | int64 |

### NavigationDistance (NavigationDistanceMessage.proto) -> `xnb` [auto]

Matched: 1/1 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | string |
| 3 | missing field (exists in APK) | None | int32 |
| 8 | missing field (exists in APK) | None | message |

### NavigationFocusRequest (NavigationFocusRequestMessage.proto) -> `aagl` [auto]

Matched: 1/1 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | enum |
| 3 | missing field (exists in APK) | None | enum |
| 4 | missing field (exists in APK) | None | enum |

### NavigationFocusResponse (NavigationFocusResponseMessage.proto) -> `aagl` [auto]

Matched: 1/1 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | enum |
| 3 | missing field (exists in APK) | None | enum |
| 4 | missing field (exists in APK) | None | enum |

### NavigationImageOptions (NavigationImageOptionsData.proto) -> `vzy` [auto]

Matched: 3/3 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 4 | missing field (exists in APK) | None | int32 |

### NavigationState (NavigationStateMessage.proto) -> `aagl` [auto]

Matched: 1/1 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | enum |
| 3 | missing field (exists in APK) | None | enum |
| 4 | missing field (exists in APK) | None | enum |

### NotificationChannel (NotificationChannelData.proto) -> `wah` [manual]

Matched: 0/1 our fields, APK has 0 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | extra field in our proto (not in APK) | notification_filter | None |

### Odometer (OdometerData.proto) -> `vxn` [auto]

Matched: 2/2 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | missing field (exists in APK) | None | bool |

### PingConfiguration (PingConfigurationData.proto) -> `zyd` [manual]

Matched: 1/2 our fields, APK has 2 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| -1 | malformed APK field entry | None | Ran out of descriptor data at pos 13 |
| 2 | extra field in our proto (not in APK) | ping_timeout_ms | None |

### RPM (RPMData.proto) -> `aagl` [auto]

Matched: 1/1 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | enum |
| 3 | missing field (exists in APK) | None | enum |
| 4 | missing field (exists in APK) | None | enum |

### RelativeInputEvent (RelativeInputEventData.proto) -> `vxn` [auto]

Matched: 2/2 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | missing field (exists in APK) | None | bool |

### RelativeInputEvents (RelativeInputEventsData.proto) -> `ufg` [auto]

Matched: 1/1 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | message |
| 3 | missing field (exists in APK) | None | enum |

### Sensor (SensorData.proto) -> `aass` [auto]

Matched: 1/1 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | message |
| 3 | missing field (exists in APK) | None | string |
| 4 | missing field (exists in APK) | None | string |

### SensorEventIndication (SensorEventIndicationMessage.proto) -> `wbo` [auto]

Matched: 20/26 our fields, APK has 26 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 21 | type mismatch | bytes | message |
| 22 | type mismatch | bytes | message |
| 23 | type mismatch | bytes | message |
| 24 | type mismatch | bytes | message |
| 25 | type mismatch | bytes | message |
| 26 | type mismatch | bytes | message |

### SensorStartResponseMessage (SensorStartResponseMessage.proto) -> `aass` [auto]

Matched: 1/1 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | message |
| 3 | missing field (exists in APK) | None | string |
| 4 | missing field (exists in APK) | None | string |

### ShutdownRequest (ShutdownRequestMessage.proto) -> `aass` [auto]

Matched: 1/1 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | missing field (exists in APK) | None | message |
| 3 | missing field (exists in APK) | None | string |
| 4 | missing field (exists in APK) | None | string |

### Speed (SpeedData.proto) -> `vxn` [auto]

Matched: 2/3 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | type mismatch | bool | int32 |

### SteeringWheel (SteeringWheelData.proto) -> `vxn` [auto]

Matched: 2/2 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | missing field (exists in APK) | None | bool |

### TouchConfig (TouchConfigData.proto) -> `vxn` [manual]

Matched: 2/2 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | missing field (exists in APK) | None | bool |

### TouchLocation (TouchLocationData.proto) -> `vzy` [auto]

Matched: 3/3 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 4 | missing field (exists in APK) | None | int32 |

### VideoConfig (VideoConfigData.proto) -> `wcz` [manual]

Matched: 10/11 our fields, APK has 11 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 11 | type mismatch | bytes | message |

### VideoFocusRequest (VideoFocusRequestMessage.proto) -> `aagl` [auto]

Matched: 3/3 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 4 | missing field (exists in APK) | None | enum |

### WifiDirectConfig (WifiDirectConfigData.proto) -> `wbb` [auto]

Matched: 5/6 our fields, APK has 6 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 5 | type mismatch | bytes | message |

### WifiInfoResponse (WifiInfoResponseMessage.proto) -> `zui` [auto]

Matched: 2/3 our fields, APK has 2 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | extra field in our proto (not in APK) | status | None |

### WifiSecurityResponse (WifiSecurityResponseMessage.proto) -> `wan` [auto]

Matched: 11/11 our fields, APK has 13 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 12 | missing field (exists in APK) | None | string |
| 13 | missing field (exists in APK) | None | bool |

## Unmatched (no APK class found)

- AVChannelMessage (AVChannelMessageIdsEnum.proto)
- AVChannelSetupStatus (AVChannelSetupStatusEnum.proto)
- AVChannelStopIndication (AVChannelStopIndicationMessage.proto)
- AVStreamType (AVStreamTypeEnum.proto)
- AdditionalVideoConfig (AdditionalVideoConfigData.proto)
- AssistantFeatureFlags (FeatureFlagsData.proto)
- AudioFocusChannel (AudioFocusChannelData.proto)
- AudioFocusState (AudioFocusStateEnum.proto)
- AudioFocusType (AudioFocusTypeEnum.proto)
- AudioType (AudioTypeEnum.proto)
- BluetoothChannelConfig (BluetoothChannelConfigData.proto)
- BluetoothChannelMessage (BluetoothChannelMessageIdsEnum.proto)
- BluetoothPairingMethod (BluetoothPairingMethodEnum.proto)
- BluetoothPairingStatus (BluetoothPairingStatusEnum.proto)
- ButtonCode (ButtonCodeEnum.proto)
- ButtonConfig (InputChannelDescriptorData.proto)
- CapabilityEntry (PhoneCapabilitiesData.proto)
- CapabilityFlag (PhoneCapabilitiesData.proto)
- CapabilityPair (PhoneCapabilitiesData.proto)
- ChannelErrorCode (ChannelErrorCodeEnum.proto)
- ChannelType (ChannelTypeEnum.proto)
- ConnectionReservedConfig (ConnectionConfigurationData.proto)
- ConnectionState (ConnectionStateEnum.proto)
- ConnectionTransportConfig (ConnectionConfigurationData.proto)
- ControlMessage (ControlMessageIdsEnum.proto)
- DeviceInfo (PhoneCapabilitiesData.proto)
- DisconnectReason (DisconnectReasonEnum.proto)
- DisplayType (DisplayTypeEnum.proto)
- DistanceLabel (NavigationDistanceDisplayData.proto)
- DriverPosition (DriverPositionEnum.proto)
- EVConnectorType (EVConnectorTypeEnum.proto)
- FuelType (FuelTypeEnum.proto)
- HapticFeedbackType (HapticFeedbackTypeEnum.proto)
- HeadlightStatus (HeadlightStatusEnum.proto)
- IndicatorStatus (IndicatorStatusEnum.proto)
- InputChannelConfig (InputChannelConfigData.proto)
- InputChannelDescriptor (InputChannelDescriptorData.proto)
- InputChannelMessage (InputChannelMessageIdsEnum.proto)
- KeyEvent (KeyEventData.proto)
- LaneShape (LaneShapeEnum.proto)
- ManeuverType (ManeuverTypeEnum.proto)
- MediaCodec (MediaCodecEnum.proto)
- MediaCodecType (MediaCodecTypeEnum.proto)
- MediaInfoChannel (MediaChannelData.proto)
- NavigationChannelConfig (NavigationChannelConfigData.proto)
- NavigationDistanceDisplay (NavigationDistanceDisplayData.proto)
- NavigationDistanceInfo (NavigationDistanceMessage.proto)
- NavigationDistanceUnit (NavigationDistanceDisplayData.proto)
- NavigationDistanceValue (NavigationDistanceMessage.proto)
- NavigationImageDimensions (NavigationChannelConfigData.proto)
- NavigationType (NavigationTypeEnum.proto)
- NotificationType (NotificationTypeEnum.proto)
- PhoneCallState (PhoneCallStateEnum.proto)
- PhoneCapabilities (PhoneCapabilitiesData.proto)
- PhoneConnectionConfig (PhoneCapabilitiesData.proto)
- PingConfigEntry (PhoneCapabilitiesData.proto)
- PingConfigPair (PhoneCapabilitiesData.proto)
- RadioChannel (RadioChannelData.proto)
- RadioChannelConfig (RadioChannelData.proto)
- RadioStationData (RadioChannelData.proto)
- SensorChannelConfig (SensorChannelConfigData.proto)
- SensorChannelMessage (SensorChannelMessageIdsEnum.proto)
- SensorType (SensorTypeEnum.proto)
- SensorTypeEntry (SensorChannelConfigData.proto)
- SessionError (SessionErrorEnum.proto)
- ShutdownReason (ShutdownReasonEnum.proto)
- ShutdownResponse (ShutdownResponseMessage.proto)
- Status (StatusEnum.proto)
- TouchAction (TouchActionEnum.proto)
- TouchCoordinate (TouchCoordinateData.proto)
- TouchPadConfig (TouchPadConfigData.proto)
- TouchScreenConfig (TouchScreenConfigData.proto)
- TouchSensitivity (InputChannelConfigData.proto)
- VersionFeatureFlags (FeatureFlagsData.proto)
- VersionResponseStatus (VersionResponseStatusEnum.proto)
- VideoFPS (VideoFPSEnum.proto)
- VideoFocusMode (VideoFocusModeEnum.proto)
- VideoFocusReason (VideoFocusReasonEnum.proto)
- VideoInsets (AdditionalVideoConfigData.proto)
- VideoMarginConfig (AdditionalVideoConfigData.proto)
- VideoResizeAction (AdditionalVideoConfigData.proto)
- VideoResolution (VideoResolutionEnum.proto)
- VideoResolutionRange (AdditionalVideoConfigData.proto)
- VoiceSessionRequest (VoiceSessionRequestMessage.proto)
- WiFiProjectionChannel (WiFiProjectionChannelData.proto)
- WifiChannelMessage (WifiChannelMessageIdsEnum.proto)
- WifiSecurityRequest (WifiSecurityRequestMessage.proto)
- WifiSetupMessage (WifiSetupMessageEnum.proto)
