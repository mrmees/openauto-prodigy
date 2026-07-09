# Proto Validation Report

Generated: 2026-02-25

## Summary

| Status | Count |
|--------|-------|
| Verified | 133 |
| Partial (diffs) | 10 |
| Unmatched | 61 |
| Error | 0 |
| **Total** | **204** |

## Verified (all fields match)

- **AVChannel** (AVChannelData.proto) -> `vys` [manual]
- **AVChannelMediaConfig** (AVChannelMediaConfigMessage.proto) -> `vyo` [manual]
- **AVChannelMediaStats** (AVChannelMediaStatsMessage.proto) -> `vyu` [manual]
- **AVChannelSetupRequest** (AVChannelSetupRequestMessage.proto) -> `wcc` [manual]
- **AVChannelSetupResponse** (AVChannelSetupResponseMessage.proto) -> `vxb` [manual]
- **AVChannelStartIndication** (AVChannelStartIndicationMessage.proto) -> `wce` [manual]
- **AVInputChannel** (AVInputChannelData.proto) -> `vyt` [manual]
- **AVInputOpenRequest** (AVInputOpenRequestMessage.proto) -> `vyx` [manual]
- **AVInputOpenResponse** (AVInputOpenResponseMessage.proto) -> `vyy` [manual]
- **AVMediaAckIndication** (AVMediaAckIndicationMessage.proto) -> `vvk` [manual]
- **AbsoluteInputEvent** (AbsoluteInputEventData.proto) -> `wci` [manual]
- **AbsoluteInputEvents** (AbsoluteInputEventsData.proto) -> `wcj` [manual]
- **Accel** (AccelData.proto) -> `vvj` [manual]
- **AdditionalVideoConfig** (AdditionalVideoConfigData.proto) -> `wcm` [manual]
- **AssistantFeatureFlags** (FeatureFlagsData.proto) -> `nlq` [manual]
- **AudioConfig** (AudioConfigData.proto) -> `vvp` [manual]
- **AudioFocusChannel** (AudioFocusChannelData.proto) -> `vyr` [manual]
- **AudioFocusRequest** (AudioFocusRequestMessage.proto) -> `vvr` [manual]
- **AudioFocusResponse** (AudioFocusResponseMessage.proto) -> `vvq` [manual]
- **AuthCompleteIndication** (AuthCompleteIndicationMessage.proto) -> `vvv` [manual]
- **BindingRequest** (BindingRequestMessage.proto) -> `wcg` [auto]
- **BindingResponse** (BindingResponseMessage.proto) -> `vwb` [manual]
- **BluetoothChannel** (BluetoothChannelData.proto) -> `vwc` [manual]
- **BluetoothChannelConfig** (BluetoothChannelConfigData.proto) -> `vwc` [manual]
- **BluetoothPairingRequest** (BluetoothPairingRequestMessage.proto) -> `kay` [manual]
- **BluetoothPairingResponse** (BluetoothPairingResponseMessage.proto) -> `xgq` [manual]
- **ButtonEvent** (ButtonEventData.proto) -> `vyi` [manual]
- **ButtonEvents** (ButtonEventsData.proto) -> `vyj` [manual]
- **CallAvailabilityStatus** (CallAvailabilityMessage.proto) -> `vwh` [manual]
- **ChannelDescriptor** (ChannelDescriptorData.proto) -> `wbw` [manual]
- **ChannelOpenRequest** (ChannelOpenRequestMessage.proto) -> `vwx` [manual]
- **ChannelOpenResponse** (ChannelOpenResponseMessage.proto) -> `vwy` [manual]
- **ChargingStationDetails** (NavigationStepMessage.proto) -> `vwz` [manual]
- **Compass** (CompassData.proto) -> `vxa` [manual]
- **ConnectionConfiguration** (ConnectionConfigurationData.proto) -> `aajk` [manual]
- **ConnectionFeatureFlags** (ConnectionConfigurationData.proto) -> `aaja` [manual]
- **ConnectionReservedConfig** (ConnectionConfigurationData.proto) -> `aaji` [manual]
- **ConnectionSecurityConfig** (ConnectionConfigurationData.proto) -> `aajj` [manual]
- **ConnectionTransportConfig** (ConnectionConfigurationData.proto) -> `aajh` [manual]
- **ConnectionTuningConfig** (ConnectionConfigurationData.proto) -> `aajg` [manual]
- **DeviceInfo** (PhoneCapabilitiesData.proto) -> `vve` [manual]
- **Diagnostics** (DiagnosticsData.proto) -> `vxf` [manual]
- **DistanceLabel** (NavigationDistanceDisplayData.proto) -> `xnc` [manual]
- **Door** (DoorData.proto) -> `vxh` [manual]
- **DrivingStatus** (DrivingStatusEnum.proto) -> `vxj` [manual]
- **Environment** (EnvironmentData.proto) -> `vxk` [manual]
- **FuelLevel** (FuelLevelData.proto) -> `vxn` [manual]
- **GPSLocation** (GPSLocationData.proto) -> `vyl` [manual]
- **Gear** (GearEnum.proto) -> `vxp` [manual]
- **Gyro** (GyroData.proto) -> `vxt` [manual]
- **HVAC** (HVACData.proto) -> `vxv` [manual]
- **HeadUnitInfo** (HeadUnitInfoData.proto) -> `aacd` [manual]
- **InputChannel** (InputChannelData.proto) -> `vya` [manual]
- **InputChannelConfig** (InputChannelConfigData.proto) -> `vya` [manual]
- **InputEventIndication** (InputEventIndicationMessage.proto) -> `vxx` [manual]
- **KeyEvent** (KeyEventData.proto) -> `vyi` [manual]
- **Light** (LightData.proto) -> `vyk` [manual]
- **MediaInfoChannel** (MediaChannelData.proto) -> `vym` [manual]
- **MediaPlaybackMetadata** (MediaPlaybackMetadataMessage.proto) -> `nmi` [manual]
- **MediaPlaybackStatus** (MediaPlaybackStatusMessage.proto) -> `ahdz` [manual]
- **NavigationChannel** (NavigationChannelData.proto) -> `vzr` [manual]
- **NavigationChannelConfig** (NavigationChannelConfigData.proto) -> `vzr` [manual]
- **NavigationDestination** (NavigationStepMessage.proto) -> `vze` [manual]
- **NavigationDistance** (NavigationDistanceMessage.proto) -> `xnb` [manual]
- **NavigationDistanceInfo** (NavigationDistanceMessage.proto) -> `xng` [manual]
- **NavigationDistanceOneof** (NavigationDistanceMessage.proto) -> `xne` [manual]
- **NavigationDistanceValue** (NavigationDistanceDisplayData.proto) -> `xmw` [manual]
- **NavigationFocusRequest** (NavigationFocusRequestMessage.proto) -> `vza` [manual]
- **NavigationFocusResponse** (NavigationFocusResponseMessage.proto) -> `vyz` [manual]
- **NavigationImageDimensions** (NavigationChannelConfigData.proto) -> `vzq` [manual]
- **NavigationImageOptions** (NavigationImageOptionsData.proto) -> `vzq` [manual]
- **NavigationLane** (NavigationStepMessage.proto) -> `vzj` [manual]
- **NavigationLaneDirection** (NavigationStepMessage.proto) -> `vzi` [manual]
- **NavigationManeuver** (NavigationStepMessage.proto) -> `vzk` [manual]
- **NavigationNotification** (NavigationStepMessage.proto) -> `vzo` [manual]
- **NavigationRoadInfo** (NavigationStepMessage.proto) -> `vzc` [manual]
- **NavigationState** (NavigationStateMessage.proto) -> `vzp` [manual]
- **NavigationStep** (NavigationStepMessage.proto) -> `vzu` [manual]
- **NavigationText** (NavigationStepMessage.proto) -> `vzn` [manual]
- **NightMode** (NightModeData.proto) -> `vzw` [manual]
- **NotificationChannel** (NotificationChannelData.proto) -> `wah` [manual]
- **Odometer** (OdometerData.proto) -> `vzx` [manual]
- **ParkingBrake** (ParkingBrakeData.proto) -> `wab` [manual]
- **Passenger** (PassengerData.proto) -> `wac` [manual]
- **PhoneCall** (PhoneStatusMessage.proto) -> `wad` [manual]
- **PhoneConnectionConfig** (PhoneCapabilitiesData.proto) -> `wdm` [manual]
- **PhoneStatusChannel** (PhoneStatusChannelData.proto) -> `vyr` [manual]
- **PhoneStatusUpdate** (PhoneStatusMessage.proto) -> `waf` [manual]
- **PingConfiguration** (PingConfigurationData.proto) -> `zyd` [manual]
- **PingRequest** (PingRequestMessage.proto) -> `wdo` [manual]
- **PingResponse** (PingResponseMessage.proto) -> `wdp` [manual]
- **RPM** (RPMData.proto) -> `wbn` [manual]
- **RadioBand** (RadioChannelData.proto) -> `wbe` [manual]
- **RadioChannel** (RadioChannelData.proto) -> `way` [manual]
- **RelativeInputEvent** (RelativeInputEventData.proto) -> `wbl` [manual]
- **RelativeInputEvents** (RelativeInputEventsData.proto) -> `wbm` [manual]
- **Sensor** (SensorData.proto) -> `wbt` [manual]
- **SensorChannel** (SensorChannelData.proto) -> `wbu` [manual]
- **SensorChannelConfig** (SensorChannelConfigData.proto) -> `wbu` [manual]
- **SensorEventIndication** (SensorEventIndicationMessage.proto) -> `wbo` [manual]
- **SensorStartRequestMessage** (SensorStartRequestMessage.proto) -> `war` [manual]
- **SensorStartResponseMessage** (SensorStartResponseMessage.proto) -> `wbs` [manual]
- **ServiceDiscoveryRequest** (ServiceDiscoveryRequestMessage.proto) -> `wbx` [manual]
- **ServiceDiscoveryResponse** (ServiceDiscoveryResponseMessage.proto) -> `wby` [manual]
- **SessionInfo** (SessionInfoData.proto) -> `vvf` [manual]
- **ShutdownRequest** (ShutdownRequestMessage.proto) -> `vwf` [manual]
- **ShutdownResponse** (ShutdownResponseMessage.proto) -> `vwg` [manual]
- **Speed** (SpeedData.proto) -> `wcd` [manual]
- **StatsEntry** (AVChannelMediaStatsMessage.proto) -> `vvo` [manual]
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
- **VideoConfig** (VideoConfigData.proto) -> `wcz` [manual]
- **VideoFocusIndication** (VideoFocusIndicationMessage.proto) -> `wdb` [manual]
- **VideoFocusRequest** (VideoFocusRequestMessage.proto) -> `wdd` [manual]
- **VideoInsets** (AdditionalVideoConfigData.proto) -> `vyb` [manual]
- **VoiceSessionRequest** (VoiceSessionRequestMessage.proto) -> `wde` [manual]
- **WiFiProjectionChannel** (WiFiProjectionChannelData.proto) -> `wdh` [manual]
- **WifiChannel** (WifiChannelData.proto) -> `wdh` [manual]
- **WifiConnectionRejection** (WifiConnectionRejectionMessage.proto) -> `wdk` [manual]
- **WifiDirectConfig** (WifiDirectConfigData.proto) -> `wbb` [manual]
- **WifiInfoRequest** (WifiInfoRequestMessage.proto) -> `wdl` [manual]
- **WifiInfoResponse** (WifiInfoResponseMessage.proto) -> `wdm` [manual]
- **WifiPingResponse** (WifiPingMessage.proto) -> `wdp` [manual]
- **WifiSecurityResponse** (WifiSecurityResponseMessage.proto) -> `wan` [manual]
- **WifiVersionResponse** (WifiVersionResponseMessage.proto) -> `wdw` [manual]

## Partial (field diffs)

### NavigationDistanceDisplay (NavigationDistanceDisplayData.proto) -> `xnd` [manual]

Matched: 3/4 our fields, APK has 4 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 4 | type mismatch | sfixed32 | unknown(-1) |
| 4 | cardinality mismatch | optional | map |

### NavigationDistanceEntry (NavigationDistanceMessage.proto) -> `xnf` [manual]

Matched: 2/3 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 3 | type mismatch | uint64 | int32 |

### NavigationTurnEvent (NavigationTurnEventMessage.proto) -> `vzm` [manual]

Matched: 3/6 our fields, APK has 6 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | type mismatch | bytes | string |
| 2 | type mismatch | string | enum |
| 4 | type mismatch | Enum (→enum) | bytes |

### RadioStation (RadioChannelData.proto) -> `wax` [manual]

Matched: 12/14 our fields, APK has 14 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | type mismatch | bytes | enum |
| 4 | type mismatch | bytes | int32 |
| 4 | cardinality mismatch | optional | repeated |

### WifiConnectStatus (WifiConnectStatusMessage.proto) -> `wdj` [manual]

Matched: 1/2 our fields, APK has 2 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 2 | type mismatch | sint32 | string |

### WifiNetworkInfo (WifiSetupInfoMessage.proto) -> `wdi` [manual]

Matched: 4/5 our fields, APK has 5 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 5 | extra field in our proto (not in APK) | extra | None |
| 6 | missing field (exists in APK) | None | int32 |

### WifiPingRequest (WifiPingMessage.proto) -> `wdo` [manual]

Matched: 0/2 our fields, APK has 1 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | type mismatch | int32 | int64 |
| 2 | extra field in our proto (not in APK) | timestamp_ns | None |

### WifiSetupInfo (WifiSetupInfoMessage.proto) -> `wds` [manual]

Matched: 3/5 our fields, APK has 5 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | missing field (exists in APK) | None | int32 |
| 3 | type mismatch | int32 | bytes |
| 6 | extra field in our proto (not in APK) | network_info | None |

### WifiStartResponse (WifiStartResponseMessage.proto) -> `wdu` [manual]

Matched: 1/3 our fields, APK has 3 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 1 | type mismatch | int32 | string |
| 2 | type mismatch | string | int32 |

### WifiVersionRequest (WifiVersionRequestMessage.proto) -> `wdv` [manual]

Matched: 5/6 our fields, APK has 6 fields

| Field | Issue | Ours | APK |
|-------|-------|------|-----|
| 4 | type mismatch | bytes | int32 |
| 4 | cardinality mismatch | optional | repeated |

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
- NavigationType (NavigationTypeEnum.proto)
- NotificationType (NotificationTypeEnum.proto)
- PhoneCallState (PhoneCallStateEnum.proto)
- PhoneCapabilities (PhoneCapabilitiesData.proto)
- PingConfigEntry (PhoneCapabilitiesData.proto)
- PingConfigPair (PhoneCapabilitiesData.proto)
- ResizeActionType (AdditionalVideoConfigData.proto)
- SensorChannelMessage (SensorChannelMessageIdsEnum.proto)
- SensorType (SensorTypeEnum.proto)
- SensorTypeEntry (SensorChannelConfigData.proto)
- SessionError (SessionErrorEnum.proto)
- ShutdownReason (ShutdownReasonEnum.proto)
- Status (StatusEnum.proto)
- TouchAction (TouchActionEnum.proto)
- TurnSide (TurnSideEnum.proto)
- UIElement (AdditionalVideoConfigData.proto)
- UITheme (AdditionalVideoConfigData.proto)
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
- WifiVersionStatus (WifiVersionStatusEnum.proto)
