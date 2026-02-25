# Proto Matching Analysis

## Summary

- **Total matches:** 83
- **Unique matches (high confidence):** 29 — one proto uniquely matched to one APK class
- **Shared matches (unreliable):** 54 — spread across 10 APK classes with multiple proto candidates

The 29 unique matches are reliable — they have no ambiguity. The 54 shared matches belong to 10 APK classes that are matched by 2-12 proto candidates each, making them difficult to distinguish without additional context.

---

## Unique Matches (High Confidence)

These 29 protos have unambiguous 1:1 mapping to their APK class counterparts. The lowest score is 0.550 (SensorEventIndicationMessage) but it's still the only match for its class.

| Proto | APK Class | Score | Confidence |
|-------|-----------|-------|------------|
| NavigationDistanceMessage | xnb | 1.000 | Perfect |
| DiagnosticsData | xss | 1.000 | Perfect |
| ServiceDiscoveryRequestMessage | aahi | 1.000 | Perfect |
| VideoFocusIndicationMessage | wdb | 1.000 | Perfect |
| ConnectionConfigurationData | aajk | 1.000 | Perfect |
| VendorExtensionChannelData | wcv | 1.000 | Perfect |
| TouchEventData | wcj | 1.000 | Perfect |
| WifiSecurityResponseMessage | wan | 1.000 | Perfect |
| BindingRequestMessage | wcg | 1.000 | Perfect |
| BluetoothPairingResponseMessage | xgq | 1.000 | Perfect |
| ButtonEventData | vyi | 1.000 | Perfect |
| BluetoothPairingRequestMessage | kay | 1.000 | Perfect |
| HeadUnitInfoData | aacd | 1.000 | Perfect |
| AVInputOpenRequestMessage | vyx | 1.000 | Perfect |
| VideoConfigData | wcz | 1.000 | Perfect |
| AVChannelSetupResponseMessage | vxb | 1.000 | Perfect |
| MediaPlaybackMetadataMessage | nmi | 1.000 | Perfect |
| LightData | vyk | 1.000 | Perfect |
| NavigationChannelData | ahdp | 1.000 | Perfect |
| DoorData | vxh | 1.000 | Perfect |
| SensorStartRequestMessage | war | 1.000 | Perfect |
| ChannelDescriptorData | wbw | 0.857 | High |
| PhoneStatusMessage | wad | 0.833 | High |
| InputEventIndicationMessage | vxx | 0.833 | High |
| AVChannelData | vys | 0.778 | High |
| ServiceDiscoveryResponseMessage | wby | 0.765 | High |
| NavigationStepMessage | utj | 0.667 | Medium-High |
| InputChannelData | zpl | 0.667 | Medium-High |
| SensorEventIndicationMessage | wbo | 0.550 | Medium |

---

## Shared Matches (Unreliable)

These 10 APK classes are matched by multiple protos, making the actual implementation identity ambiguous without additional context (field analysis, runtime behavior, etc.).

### vxn (12 candidates) — HIGHLY AMBIGUOUS

Matched by:
- ChannelOpenRequestMessage (1.000)
- AVMediaAckIndicationMessage (1.000)
- SteeringWheelData (1.000)
- AVChannelStartIndicationMessage (1.000)
- OdometerData (1.000)
- HVACData (1.000)
- AbsoluteInputEventData (1.000)
- FuelLevelData (1.000)
- TouchConfigData (1.000)
- RelativeInputEventData (1.000)
- AVInputOpenResponseMessage (1.000)
- SpeedData (0.667)

**Analysis:** vxn appears to be a generic wrapper or container class. All 12 matches have identical field structure (~2-3 fields, 1-2 diffs). Likely a reusable base class for sensor/config/response messages.

### aass (10 candidates) — HIGHLY AMBIGUOUS

Matched by:
- GearData (1.000)
- ShutdownRequestMessage (1.000)
- SensorStartResponseMessage (1.000)
- ChannelOpenResponseMessage (1.000)
- AuthCompleteIndicationMessage (1.000)
- AudioFocusRequestMessage (1.000)
- BindingResponseMessage (1.000)
- SensorData (1.000)
- AudioFocusResponseMessage (1.000)
- AVInputChannelData (0.667)

**Analysis:** Like vxn, aass is likely a generic container. All matches have ~1 field in our proto, but 4 fields in APK class (same 3 extra fields: message g, string d, string e). Possibly a base class for request/response message wrappers.

### vzy (9 candidates) — HIGHLY AMBIGUOUS

Matched by:
- AccelData (1.000)
- GyroData (1.000)
- TirePressureData (1.000)
- AudioConfigData (1.000)
- EnvironmentData (1.000)
- NavigationImageOptionsData (1.000)
- PingConfigurationData (1.000)
- TouchLocationData (1.000)
- CompassData (1.000)

**Analysis:** Another generic container. All 9 matches have 3-4 fields in our proto with only field 4 differing. Likely a config/data container class.

### aagl (7 candidates) — AMBIGUOUS

Matched by:
- NavigationStateMessage (1.000)
- NavigationFocusRequestMessage (1.000)
- DrivingStatusData (1.000)
- AVChannelSetupRequestMessage (1.000)
- RPMData (1.000)
- NavigationFocusResponseMessage (1.000)
- VideoFocusRequestMessage (0.667)

**Analysis:** Navigation/setup request/response class. All have 1 field in our proto but 4 in APK (same extra fields: string d, enum f, enum). Possibly a navigation-specific request base class.

### poe (4 candidates)

Matched by:
- ParkingBrakeData (1.000)
- CallAvailabilityMessage (1.000)
- NightModeData (1.000)
- PassengerData (1.000)

**Analysis:** All have exactly 1 field in both our proto and APK class. Simple enum or single-value wrapper.

### ufg (4 candidates)

Matched by:
- SensorChannelData (1.000)
- AbsoluteInputEventsData (1.000)
- RelativeInputEventsData (1.000)
- ButtonEventsData (1.000)

**Analysis:** Sensor/input event container. All have 1 field in our proto, 3 in APK (same extra fields: message d, enum e).

### zrz (2 candidates)

Matched by:
- PingRequestMessage (1.000)
- PingResponseMessage (1.000)

**Analysis:** Ping request/response pair. Both have exactly 1 field in both protos and APK. Likely a simple wrapper.

### ahel (2 candidates)

Matched by:
- WifiChannelData (1.000)
- BluetoothChannelData (0.500)

**Analysis:** Bluetooth/WiFi channel class. WifiChannelData is confident; BluetoothChannelData has lower score (0.500 — likely false positive).

### zui (2 candidates)

Matched by:
- WifiInfoRequestMessage (1.000)
- WifiInfoResponseMessage (0.667)

**Analysis:** WiFi info request/response. Request has higher score; response is borderline (0.667 — field mismatch).

### ahdz (2 candidates)

Matched by:
- GPSLocationData (0.857)
- MediaPlaybackStatusMessage (0.833)

**Analysis:** Location/playback data. Both have moderate scores with significant field differences (14-15 extra fields in APK). Possible name collision or class hierarchy.

---

## Recommendations for Manual Validation

1. **Start with the 29 unique matches** — these are safe to use as reference implementations.

2. **For the 10 shared classes**, focus on:
   - **vxn, aass, vzy** (largest groups) — these are almost certainly generic base/container classes. Check if they're abstract in the APK source or used as mixins.
   - **aagl** (7 candidates) — likely navigation-specific but with ambiguous semantic meaning.
   - **ahel, zui, ahdz** — small groups with outliers (BluetoothChannelData, WifiInfoResponseMessage, MediaPlaybackStatusMessage). The lower-scoring matches are probably false positives; validate by field type inspection.

3. **Investigate structurally identical groups** (e.g., all vzy candidates have field 4 as a single int32):
   - These may be auto-generated subclasses from a single template.
   - Check if the APK uses inheritance/composition patterns that explain the duplication.

4. **Cross-reference with service channel assignments** — knowing which proto is assigned to which AA service channel can disambiguate ambiguous matches.
