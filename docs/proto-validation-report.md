# Proto Validation Report

Generated: 2026-02-25

## Summary

| Status | Count |
|--------|-------|
| Verified | 93 |
| Partial (diffs) | 32 |
| Unmatched | 58 |
| Error | 0 |
| **Total** | **183** |

## Verified (all fields match)

- **AVChannelSetupResponse** (AVChannelSetupResponseMessage.proto) -> `vxb` [manual]
- **AVInputChannel** (AVInputChannelData.proto) -> `vyt` [manual]
- **AVInputOpenRequest** (AVInputOpenRequestMessage.proto) -> `vyx` [manual]
- **AVInputOpenResponse** (AVInputOpenResponseMessage.proto) -> `vyy` [manual]
- **AVMediaAckIndication** (AVMediaAckIndicationMessage.proto) -> `vvk` [manual]
- **AbsoluteInputEvent** (AbsoluteInputEventData.proto) -> `wci` [manual]
- **Accel** (AccelData.proto) -> `vvj` [manual]
- **AssistantFeatureFlags** (FeatureFlagsData.proto) -> `nlq` [manual]
- **AudioConfig** (AudioConfigData.proto) -> `vvp` [manual]
- **AudioFocusChannel** (AudioFocusChannelData.proto) -> `vyr` [manual]
- **AudioFocusRequest** (AudioFocusRequestMessage.proto) -> `vvr` [manual]
- **AudioFocusResponse** (AudioFocusResponseMessage.proto) -> `vvq` [manual]
- **BindingRequest** (BindingRequestMessage.proto) -> `wcg` [auto]
- **BindingResponse** (BindingResponseMessage.proto) -> `vwb` [manual]
- **BluetoothChannel** (BluetoothChannelData.proto) -> `vwc` [manual]
- **BluetoothPairingRequest** (BluetoothPairingRequestMessage.proto) -> `kay` [manual]
- **ButtonEvent** (ButtonEventData.proto) -> `vyi` [manual]
- **ButtonEvents** (ButtonEventsData.proto) -> `vyj` [manual]
- **CallAvailabilityStatus** (CallAvailabilityMessage.proto) -> `vwh` [manual]
- **ChannelOpenRequest** (ChannelOpenRequestMessage.proto) -> `vwx` [manual]
- **ChannelOpenResponse** (ChannelOpenResponseMessage.proto) -> `vwy` [manual]
- **ChargingStationDetails** (NavigationStepMessage.proto) -> `vwz` [manual]
- **Compass** (CompassData.proto) -> `vxa` [manual]
- **ConnectionFeatureFlags** (ConnectionConfigurationData.proto) -> `aaja` [manual]
- **ConnectionReservedConfig** (ConnectionConfigurationData.proto) -> `aaji` [manual]
- **ConnectionTuningConfig** (ConnectionConfigurationData.proto) -> `aajg` [manual]
- **DeviceInfo** (PhoneCapabilitiesData.proto) -> `vve` [manual]
- **Diagnostics** (DiagnosticsData.proto) -> `vxf` [manual]
- **Door** (DoorData.proto) -> `vxh` [manual]
- **Environment** (EnvironmentData.proto) -> `vxk` [manual]
- **FuelLevel** (FuelLevelData.proto) -> `vxn` [manual]
- **GPSLocation** (GPSLocationData.proto) -> `vyl` [manual]
- **Gyro** (GyroData.proto) -> `vxt` [manual]
- **HVAC** (HVACData.proto) -> `vxv` [manual]
- **HeadUnitInfo** (HeadUnitInfoData.proto) -> `aacd` [manual]
- **InputChannel** (InputChannelData.proto) -> `vya` [manual]
- **InputEventIndication** (InputEventIndicationMessage.proto) -> `vxx` [manual]
- **KeyEvent** (KeyEventData.proto) -> `vyi` [manual]
- **Light** (LightData.proto) -> `vyk` [manual]
- **MediaInfoChannel** (MediaChannelData.proto) -> `vym` [manual]
- **MediaPlaybackMetadata** (MediaPlaybackMetadataMessage.proto) -> `nmi` [manual]
- **MediaPlaybackStatus** (MediaPlaybackStatusMessage.proto) -> `ahdz` [manual]
- **NavigationChannel** (NavigationChannelData.proto) -> `vzr` [manual]
- **NavigationDestination** (NavigationStepMessage.proto) -> `vze` [manual]
- **NavigationDistanceValue** (NavigationDistanceMessage.proto) -> `xmw` [manual]
- **NavigationImageOptions** (NavigationImageOptionsData.proto) -> `vzq` [manual]
- **NavigationLane** (NavigationStepMessage.proto) -> `vzj` [manual]
- **NavigationLaneDirection** (NavigationStepMessage.proto) -> `vzi` [manual]
- **NavigationManeuver** (NavigationStepMessage.proto) -> `vzk` [manual]
- **NavigationNotification** (NavigationStepMessage.proto) -> `vzo` [manual]
- **NavigationRoadInfo** (NavigationStepMessage.proto) -> `vzc` [manual]
- **NavigationStep** (NavigationStepMessage.proto) -> `vzu` [manual]
- **NavigationText** (NavigationStepMessage.proto) -> `vzn` [manual]
- **NightMode** (NightModeData.proto) -> `vzw` [manual]
- **NotificationChannel** (NotificationChannelData.proto) -> `wah` [manual]
- **Odometer** (OdometerData.proto) -> `vzx` [manual]
- **ParkingBrake** (ParkingBrakeData.proto) -> `wab` [manual]
- **Passenger** (PassengerData.proto) -> `wac` [manual]
- **PhoneCall** (PhoneStatusMessage.proto) -> `wad` [manual]
- **PhoneStatusChannel** (PhoneStatusChannelData.proto) -> `vyr` [manual]
- **PhoneStatusUpdate** (PhoneStatusMessage.proto) -> `waf` [manual]
- **PingRequest** (PingRequestMessage.proto) -> `wdo` [manual]
- **PingResponse** (PingResponseMessage.proto) -> `wdp` [manual]
- **RPM** (RPMData.proto) -> `wbn` [manual]
- **RelativeInputEvent** (RelativeInputEventData.proto) -> `wbl` [manual]
- **RelativeInputEvents** (RelativeInputEventsData.proto) -> `wbm` [manual]
- **Sensor** (SensorData.proto) -> `wbt` [manual]
- **SensorChannel** (SensorChannelData.proto) -> `wbu` [manual]
- **SensorStartRequestMessage** (SensorStartRequestMessage.proto) -> `war` [manual]
- **SensorStartResponseMessage** (SensorStartResponseMessage.proto) -> `wbs` [manual]
- **ServiceDiscoveryRequest** (ServiceDiscoveryRequestMessage.proto) -> `wbx` [manual]
- **ServiceDiscoveryResponse** (ServiceDiscoveryResponseMessage.proto) -> `wby` [manual]
- **SessionInfo** (SessionInfoData.proto) -> `vvf` [manual]
- **ShutdownRequest** (ShutdownRequestMessage.proto) -> `vwf` [manual]
- **ShutdownResponse** (ShutdownResponseMessage.proto) -> `vwg` [manual]
- **Speed** (SpeedData.proto) -> `wcd` [manual]
- **SteeringWheel** (SteeringWheelData.proto) -> `vxe` [manual]
- **TirePressure** (TirePressureData.proto) -> `wcg` [manual]
- **TouchConfig** (TouchConfigData.proto) -> `vxn` [manual]
- **TouchCoordinate** (TouchCoordinateData.proto) -> `wci` [manual]
- **TouchEvent** (TouchEventData.proto) -> `wcj` [manual]
- **TouchLocation** (TouchLocationData.proto) -> `wci` [manual]
- **TouchPadConfig** (TouchPadConfigData.proto) -> `vxy` [manual]
- **TouchScreenConfig** (TouchScreenConfigData.proto) -> `vxz` [manual]
- **VendorExtensionChannel** (VendorExtensionChannelData.proto) -> `wcv` [manual]
- **VersionFeatureFlags** (FeatureFlagsData.proto) -> `wdq` [manual]
- **VideoFocusIndication** (VideoFocusIndicationMessage.proto) -> `wdb` [manual]
- **VideoFocusRequest** (VideoFocusRequestMessage.proto) -> `wdd` [manual]
- **VideoInsets** (AdditionalVideoConfigData.proto) -> `vyb` [manual]
- **WiFiProjectionChannel** (WiFiProjectionChannelData.proto) -> `wdh` [manual]
- **WifiChannel** (WifiChannelData.proto) -> `wdh` [manual]
- **WifiInfoRequest** (WifiInfoRequestMessage.proto) -> `wdl` [manual]
- **WifiSecurityResponse** (WifiSecurityResponseMessage.proto) -> `wan` [manual]

## Partial (field diffs)

### AVChannel (AVChannelData.proto) -> `vys` [manual]

Matched: 7/8 our fields, APK has 8 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 8 | type mismatch | int32 | enum |

### AVChannelSetupRequest (AVChannelSetupRequestMessage.proto) -> `wcc` [manual]

Matched: 0/1 our fields, APK has 1 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | type mismatch | uint32 | enum |

### AVChannelStartIndication (AVChannelStartIndicationMessage.proto) -> `wce` [manual]

Matched: 2/4 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | type mismatch | int32 | enum |
| 4 | type mismatch | bytes | message |

### AbsoluteInputEvents (AbsoluteInputEventsData.proto) -> `wcj` [manual]

Matched: 2/3 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | type mismatch | int32 | enum |

### AdditionalVideoConfig (AdditionalVideoConfigData.proto) -> `wcm` [manual]

Matched: 5/7 our fields, APK has 7 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 4 | type mismatch | int32 | enum |
| 5 | type mismatch | int32 | enum |

### AuthCompleteIndication (AuthCompleteIndicationMessage.proto) -> `vvv` [manual]

Matched: 0/1 our fields, APK has 1 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | type mismatch | Enum (→enum) | int32 |

### BluetoothChannelConfig (BluetoothChannelConfigData.proto) -> `vwc` [manual]

Matched: 1/2 our fields, APK has 2 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | type mismatch | int32 | enum |

### BluetoothPairingResponse (BluetoothPairingResponseMessage.proto) -> `xgq` [manual]

Matched: 2/3 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | type mismatch | int32 | enum |

### ChannelDescriptor (ChannelDescriptorData.proto) -> `wbw` [manual]

Matched: 13/17 our fields, APK has 17 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 11 | type mismatch | bytes | message |
| 15 | type mismatch | bytes | message |
| 16 | type mismatch | bytes | message |
| 17 | type mismatch | bytes | message |

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

### ConnectionTransportConfig (ConnectionConfigurationData.proto) -> `aajh` [manual]

Matched: 0/2 our fields, APK has 2 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | type mismatch | bytes | message |
| 3 | type mismatch | bytes | message |

### DrivingStatus (DrivingStatusEnum.proto) -> `vxj` [manual]

Matched: 0/0 our fields, APK has 1 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | missing field (exists in APK) | None | int32 |

### Gear (GearEnum.proto) -> `vxp` [manual]

Matched: 0/0 our fields, APK has 1 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | missing field (exists in APK) | None | enum |

### InputChannelConfig (InputChannelConfigData.proto) -> `vya` [manual]

Matched: 3/5 our fields, APK has 5 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 4 | type mismatch | TouchPadConfig (→message) | enum |
| 5 | cardinality mismatch | repeated | singular |

### NavigationChannelConfig (NavigationChannelConfigData.proto) -> `vzr` [manual]

Matched: 2/3 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | type mismatch | int32 | enum |

### NavigationDistance (NavigationDistanceMessage.proto) -> `xnb` [manual]

Matched: 3/4 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 8 | type mismatch | bytes | message |

### NavigationDistanceDisplay (NavigationDistanceDisplayData.proto) -> `xnd` [manual]

Matched: 3/4 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 4 | type mismatch | bytes | unknown(-1) |
| 4 | cardinality mismatch | optional | map |

### NavigationDistanceInfo (NavigationDistanceMessage.proto) -> `xng` [manual]

Matched: 1/3 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | type mismatch | bytes | message |
| 2 | cardinality mismatch | optional | repeated |
| 4 | type mismatch | bytes | message |
| 4 | cardinality mismatch | optional | oneof |

### NavigationFocusRequest (NavigationFocusRequestMessage.proto) -> `vza` [manual]

Matched: 0/1 our fields, APK has 1 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | type mismatch | uint32 | enum |

### NavigationFocusResponse (NavigationFocusResponseMessage.proto) -> `vyz` [manual]

Matched: 0/1 our fields, APK has 1 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | type mismatch | uint32 | enum |

### NavigationImageDimensions (NavigationChannelConfigData.proto) -> `xnf` [manual]

Matched: 2/3 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | type mismatch | int32 | message |

### NavigationState (NavigationStateMessage.proto) -> `vzp` [manual]

Matched: 0/1 our fields, APK has 1 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | type mismatch | int32 | enum |

### PhoneConnectionConfig (PhoneCapabilitiesData.proto) -> `wdm` [manual]

Matched: 3/5 our fields, APK has 5 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 4 | type mismatch | int32 | enum |
| 5 | type mismatch | int32 | enum |

### PingConfiguration (PingConfigurationData.proto) -> `zyd` [manual]

Matched: 1/2 our fields, APK has 2 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| -1 | malformed APK field entry | None | Ran out of descriptor data at pos 13 |
| 2 | extra field in our proto (not in APK) | ping_timeout_ms | None |

### RadioChannel (RadioChannelData.proto) -> `way` [manual]

Matched: 0/1 our fields, APK has 1 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | type mismatch | bytes | message |

### SensorChannelConfig (SensorChannelConfigData.proto) -> `wbu` [manual]

Matched: 2/4 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | type mismatch | int32 | enum |
| 4 | type mismatch | int32 | enum |

### SensorEventIndication (SensorEventIndicationMessage.proto) -> `wbo` [manual]

Matched: 20/26 our fields, APK has 26 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 21 | type mismatch | bytes | message |
| 22 | type mismatch | bytes | message |
| 23 | type mismatch | bytes | message |
| 24 | type mismatch | bytes | message |
| 25 | type mismatch | bytes | message |
| 26 | type mismatch | bytes | message |

### VideoConfig (VideoConfigData.proto) -> `wcz` [manual]

Matched: 10/11 our fields, APK has 11 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 11 | type mismatch | bytes | message |

### VoiceSessionRequest (VoiceSessionRequestMessage.proto) -> `wde` [manual]

Matched: 0/1 our fields, APK has 1 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | type mismatch | int32 | enum |

### WifiDirectConfig (WifiDirectConfigData.proto) -> `wbb` [manual]

Matched: 5/6 our fields, APK has 6 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 5 | type mismatch | bytes | message |

### WifiInfoResponse (WifiInfoResponseMessage.proto) -> `wdm` [manual]

Matched: 3/5 our fields, APK has 5 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 4 | type mismatch | int32 | enum |
| 5 | type mismatch | int32 | enum |

## Unmatched (no APK class found)

- AVChannelMessage (AVChannelMessageIdsEnum.proto)
- AVChannelSetupStatus (AVChannelSetupStatusEnum.proto)
- AVChannelStopIndication (AVChannelStopIndicationMessage.proto)
- AVStreamType (AVStreamTypeEnum.proto)
- AudioFocusState (AudioFocusStateEnum.proto)
- AudioFocusType (AudioFocusTypeEnum.proto)
- AudioType (AudioTypeEnum.proto)
- BluetoothChannelMessage (BluetoothChannelMessageIdsEnum.proto)
- BluetoothPairingMethod (BluetoothPairingMethodEnum.proto)
- BluetoothPairingStatus (BluetoothPairingStatusEnum.proto)
- ButtonCode (ButtonCodeEnum.proto)
- CapabilityEntry (PhoneCapabilitiesData.proto)
- CapabilityFlag (PhoneCapabilitiesData.proto)
- CapabilityPair (PhoneCapabilitiesData.proto)
- ChannelErrorCode (ChannelErrorCodeEnum.proto)
- ChannelType (ChannelTypeEnum.proto)
- ConnectionState (ConnectionStateEnum.proto)
- ControlMessage (ControlMessageIdsEnum.proto)
- DisconnectReason (DisconnectReasonEnum.proto)
- DisplayType (DisplayTypeEnum.proto)
- DriverPosition (DriverPositionEnum.proto)
- EVConnectorType (EVConnectorTypeEnum.proto)
- FuelType (FuelTypeEnum.proto)
- HapticFeedbackType (HapticFeedbackTypeEnum.proto)
- HeadlightStatus (HeadlightStatusEnum.proto)
- IndicatorStatus (IndicatorStatusEnum.proto)
- InputChannelMessage (InputChannelMessageIdsEnum.proto)
- LaneShape (LaneShapeEnum.proto)
- ManeuverType (ManeuverTypeEnum.proto)
- MediaCodec (MediaCodecEnum.proto)
- MediaCodecType (MediaCodecTypeEnum.proto)
- NavigationDistanceUnit (NavigationDistanceDisplayData.proto)
- NavigationType (NavigationTypeEnum.proto)
- NotificationType (NotificationTypeEnum.proto)
- PhoneCallState (PhoneCallStateEnum.proto)
- PhoneCapabilities (PhoneCapabilitiesData.proto)
- PingConfigEntry (PhoneCapabilitiesData.proto)
- PingConfigPair (PhoneCapabilitiesData.proto)
- SensorChannelMessage (SensorChannelMessageIdsEnum.proto)
- SensorType (SensorTypeEnum.proto)
- SensorTypeEntry (SensorChannelConfigData.proto)
- SessionError (SessionErrorEnum.proto)
- ShutdownReason (ShutdownReasonEnum.proto)
- Status (StatusEnum.proto)
- TouchAction (TouchActionEnum.proto)
- TouchSensitivity (InputChannelConfigData.proto)
- VersionResponseStatus (VersionResponseStatusEnum.proto)
- VideoFPS (VideoFPSEnum.proto)
- VideoFocusMode (VideoFocusModeEnum.proto)
- VideoFocusReason (VideoFocusReasonEnum.proto)
- VideoMarginConfig (AdditionalVideoConfigData.proto)
- VideoResizeAction (AdditionalVideoConfigData.proto)
- VideoResolution (VideoResolutionEnum.proto)
- VideoResolutionRange (AdditionalVideoConfigData.proto)
- WifiChannelMessage (WifiChannelMessageIdsEnum.proto)
- WifiSecurityRequest (WifiSecurityRequestMessage.proto)
- WifiSetupMessage (WifiSetupMessageEnum.proto)
- WifiStartRequest (WifiStartRequestMessage.proto)
