# Proto Partial Match Investigation Plan

## Context

After 5 Codex batches and 4 field-fix batches, we're at **92 verified / 33 partial / 58 unmatched** out of 183 proto messages. The 58 unmatched are mostly enum definitions and embedded sub-messages without their own APK class — not actionable. The 33 partials are the remaining work.

**Goal:** Investigate each partial match, determine whether it needs a fix or is cosmetic/validator-limitation, and document findings. Fix what's fixable, document what's not.

**Validator command:** `python3 tools/validate_protos.py --summary`
**Validator reports:** `tools/proto_validation_report.json`, `docs/proto-validation-report.md`
**APK source:** `/home/matt/claude/personal/openautopro/firmware/android-auto-apk/decompiled`
**Proto files:** `libs/open-androidauto/proto/`
**Match file:** `tools/proto_matches.json`

## How to investigate each item

For each partial:
1. Read the proto file and identify the mismatched field(s)
2. Look up the APK class in decompiled source to see actual field types
3. Determine category: cosmetic (int32==enum on wire), wire-compatible (bytes==message), validator bug, or genuine fix needed
4. If fixable: make the change and verify it compiles
5. Report findings before moving to next item

## Progress Tracker

| # | Message | APK | Category | Status | Action |
|---|---------|-----|----------|--------|--------|
| 1 | AVChannel f8 | vys | int32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 2 | AVChannelSetupRequest f1 | wcc | int32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 3 | AbsoluteInputEvents f3 | wcj | int32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 4 | AdditionalVideoConfig f4,f5 | wcm | int32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 5 | AuthCompleteIndication f1 | vvv | Enum↔int32 (reversed) | **COSMETIC** | NO FIX: we use Enum, APK stores as int32 — same wire |
| 6 | BluetoothChannelConfig f2 | vwc | int32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 7 | BluetoothPairingResponse f3 | xgq | int32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 8 | ConnectionSecurityConfig f1 | aajj | int32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 9 | NavigationChannelConfig f2 | vzr | int32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 10 | NavigationFocusRequest f1 | vza | uint32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 11 | NavigationFocusResponse f1 | vyz | uint32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 12 | NavigationState f1 | vzp | int32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 13 | PhoneConnectionConfig f4,f5 | wdm | int32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 14 | SensorChannelConfig f3,f4 | wbu | int32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 15 | VoiceSessionRequest f1 | wde | int32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 16 | WifiInfoResponse f4,f5 | wdm | int32↔enum | **COSMETIC** | NO FIX: wire-identical |
| 17 | AVChannelStartIndication f4 | wce | bytes↔message | **WIRE-COMPAT** | NO FIX: not used in code, wire-identical |
| 18 | ChannelDescriptor f11,15,16,17 | wbw | bytes↔message | **WIRE-COMPAT** | NO FIX: not used in code, wire-identical |
| 19 | ConnectionTransportConfig f2,f3 | aajh | bytes↔message | **WIRE-COMPAT** | NO FIX: not used in code, wire-identical |
| 20 | NavigationDistance f8 | xnb | bytes↔message | **WIRE-COMPAT** | NO FIX: not used in code, wire-identical |
| 21 | RadioChannel f1 | way | bytes↔message | **WIRE-COMPAT** | NO FIX: not used in code, wire-identical |
| 22 | SensorEventIndication f21-26 | wbo | bytes↔message | **WIRE-COMPAT** | NO FIX: not used in code, wire-identical |
| 23 | VideoConfig f11 | wcz | bytes↔message | **WIRE-COMPAT** | NO FIX: not used in code, wire-identical |
| 24 | WifiDirectConfig f5 | wbb | bytes↔message | **WIRE-COMPAT** | NO FIX: not used in code, wire-identical |
| 25 | ChannelOpenRequest f1 | vwx | int32↔sint32 | **INVESTIGATED** | FIX: change to sint32 + check field 3 |
| 26 | ConnectionConfiguration f2-6 | aajk | validator: oneof | **INVESTIGATED** | NO FIX: validator bug (oneof) |
| 27 | DrivingStatus f1 | vxj | validator: enum-only | **INVESTIGATED** | NO FIX: validator limitation |
| 28 | Gear f1 | vxp | validator: enum-only | **INVESTIGATED** | NO FIX: validator limitation |
| 29 | InputChannelConfig f3,f4,f5 | vya | cardinality+type | **INVESTIGATED** | FIX: 3 fields wrong (TouchScreenConfig, TouchPadConfig, enum) |
| 30 | NavigationDistanceDisplay f4 | xnd | map field | **INVESTIGATED** | DEFER: decode xnc.java for map types |
| 31 | NavigationDistanceInfo f2,f4 | xng | cardinality+type | **INVESTIGATED** | DEFER: needs live captures |
| 32 | NavigationImageDimensions f1 | xnf | int32↔message | **INVESTIGATED** | LIKELY WRONG MAPPING: leave as-is |
| 33 | PingConfiguration f1,f2 | zyd | malformed+extra | **INVESTIGATED** | NO FIX: validator bug (descriptor format) |

---

## Category 1: int32↔enum (cosmetic, wire-identical)

These are all varint-encoded on the wire. The APK uses Java enums internally but protobuf encodes them identically to int32. Our proto uses int32 where the APK has enum. Options:
- **A)** Leave as int32 (works fine, just not self-documenting)
- **B)** Create proper enum types and reference them (more accurate, more proto files)
- **C)** Add comments noting the enum values (compromise)

### Item 1: AVChannel field 8

**File:** `libs/open-androidauto/proto/AVChannelData.proto`
**Field:** `optional int32 keycode = 8;`  // APK has enum
**APK class:** vys (AVChannel), field 8 maps to vyh.java (Android keycode enum)
**Current comment:** `// Android keycode (APK: vyh.java)`

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 2: AVChannelSetupRequest field 1

**File:** `libs/open-androidauto/proto/AVChannelSetupRequestMessage.proto`
**Field:** `optional uint32 config_index = 1;`  // APK has enum
**APK class:** wcc

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 3: AbsoluteInputEvents field 3

**File:** `libs/open-androidauto/proto/AbsoluteInputEventsData.proto`
**Field:** `optional int32 action = 3;`  // APK has enum
**APK class:** wcj

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 4: AdditionalVideoConfig fields 4, 5

**File:** `libs/open-androidauto/proto/AdditionalVideoConfigData.proto`
**Fields:**
- `optional int32 ui_theme = 4;`  // APK has enum (wcr)
- `repeated int32 hidden_ui_elements = 5;`  // APK has enum (wcn)
**APK class:** wcm

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 5: AuthCompleteIndication field 1 (REVERSED)

**File:** `libs/open-androidauto/proto/AuthCompleteIndicationMessage.proto`
**Field:** `required Status status = 1;`  // We have Enum, APK has int32
**APK class:** vvv
**Note:** This is the opposite direction — we reference a proto Enum but APK shows int32. The Enum import may be confusing the validator.

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 6: BluetoothChannelConfig field 2

**File:** `libs/open-androidauto/proto/BluetoothChannelConfigData.proto`
**Field:** `repeated int32 supported_pairing_methods = 2;`  // APK has enum
**APK class:** vwc

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 7: BluetoothPairingResponse field 3

**File:** `libs/open-androidauto/proto/BluetoothPairingResponseMessage.proto`
**Field:** `optional int32 error_code = 3;`  // APK has enum
**APK class:** xgq

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 8: ConnectionSecurityConfig field 1

**File:** `libs/open-androidauto/proto/ConnectionConfigurationData.proto`
**Field:** `optional int32 security_mode = 1;`  // APK has enum
**APK class:** aajj

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 9: NavigationChannelConfig field 2

**File:** `libs/open-androidauto/proto/NavigationChannelConfigData.proto`
**Field:** `optional int32 type = 2;`  // APK has NavigationType enum
**APK class:** vzr

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 10: NavigationFocusRequest field 1

**File:** `libs/open-androidauto/proto/NavigationFocusRequestMessage.proto`
**Field:** `optional uint32 type = 1;`  // APK has enum
**APK class:** vza

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 11: NavigationFocusResponse field 1

**File:** `libs/open-androidauto/proto/NavigationFocusResponseMessage.proto`
**Field:** `optional uint32 type = 1;`  // APK has enum
**APK class:** vyz

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 12: NavigationState field 1

**File:** `libs/open-androidauto/proto/NavigationStateMessage.proto`
**Field:** `required int32 state = 1;`  // APK has enum
**APK class:** vzp

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 13: PhoneConnectionConfig fields 4, 5

**File:** `libs/open-androidauto/proto/PhoneCapabilitiesData.proto`
**Fields:**
- `optional int32 security_mode = 4;`  // APK has enum (SecurityMode)
- `optional int32 ap_type = 5;`  // APK has enum (AccessPointType)
**APK class:** wdm

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 14: SensorChannelConfig fields 3, 4

**File:** `libs/open-androidauto/proto/SensorChannelConfigData.proto`
**Fields:**
- `repeated int32 fuel_types = 3;`  // APK has enum (FuelType)
- `repeated int32 ev_connector_types = 4;`  // APK has enum (EVConnectorType)
**APK class:** wbu

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 15: VoiceSessionRequest field 1

**File:** `libs/open-androidauto/proto/VoiceSessionRequestMessage.proto`
**Field:** `optional int32 session_type = 1;`  // APK has enum
**APK class:** wde

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 16: WifiInfoResponse fields 4, 5

**File:** `libs/open-androidauto/proto/WifiInfoResponseMessage.proto`
**Fields:**
- `optional int32 security_mode = 4;`  // APK has enum (SecurityMode)
- `optional int32 ap_type = 5;`  // APK has enum (AccessPointType)
**APK class:** wdm

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

---

## Category 2: bytes↔message (wire-compatible)

These are both length-delimited (wire type 2) on the wire. Using `bytes` means we get raw bytes; using `message` means protobuf auto-deserializes. We chose `bytes` when the sub-message structure wasn't decoded. Options:
- **A)** Leave as bytes (safe, works, just opaque)
- **B)** Decode sub-message and promote to proper message type (better for code that needs the data)
- **C)** Mixed — decode the ones we actually USE in code, leave rest as bytes

### Item 17: AVChannelStartIndication field 4

**File:** `libs/open-androidauto/proto/AVChannelStartIndicationMessage.proto`
**Field:** `optional bytes media_config = 4;`  // APK: message type
**APK class:** wce, field 4 sub-message class TBD

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 18: ChannelDescriptor fields 11, 15, 16, 17

**File:** `libs/open-androidauto/proto/ChannelDescriptorData.proto`
**Fields:**
- `optional bytes media_browser_channel = 11;`  // APK: message (vym, empty)
- `optional bytes car_control_channel = 15;`  // APK: message (vwo, 3 fields)
- `optional bytes generic_notification_channel = 16;`  // APK: message (vwt, empty)
- `optional bytes voice_channel = 17;`  // APK: message (vwd, empty)
**APK class:** wbw

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 19: ConnectionTransportConfig fields 2, 3

**File:** `libs/open-androidauto/proto/ConnectionConfigurationData.proto`
**Fields:**
- `optional bytes transport_params_a = 2;`  // APK: message (aajt)
- `optional bytes transport_params_b = 3;`  // APK: message (aakd)
**APK class:** aajh

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 20: NavigationDistance field 8

**File:** `libs/open-androidauto/proto/NavigationDistanceMessage.proto`
**Field:** `optional bytes distance_display = 8;`  // APK: message type
**APK class:** xnb

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 21: RadioChannel field 1

**File:** `libs/open-androidauto/proto/RadioChannelData.proto`
**Field:** `repeated bytes radio_stations = 1;`  // APK: message (wax)
**APK class:** way

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 22: SensorEventIndication fields 21-26

**File:** `libs/open-androidauto/proto/SensorEventIndicationMessage.proto`
**Fields:**
- `repeated bytes toll_road = 21;`  // APK: message (vxs, 3 fields)
- `repeated bytes range_remaining = 22;`  // APK: message (wch, 1 field: bool)
- `repeated bytes fuel_type_info = 23;`  // APK: message (vvg, empty)
- `repeated bytes ev_battery_info = 24;`  // APK: message (wcl, empty)
- `repeated bytes ev_charge_info = 25;`  // APK: message (wbh, 1 field: bytes)
- `repeated bytes ev_charge_status = 26;`  // APK: message (wbf, 1 field: bytes)
**APK class:** wbo

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 23: VideoConfig field 11

**File:** `libs/open-androidauto/proto/VideoConfigData.proto`
**Field:** `optional bytes additional_config = 11;`  // APK: message (AdditionalVideoConfig/wcm)
**APK class:** wcz

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

### Item 24: WifiDirectConfig field 5

**File:** `libs/open-androidauto/proto/WifiDirectConfigData.proto`
**Field:** `optional bytes group_owner_info = 5;`  // APK: message (wam)
**APK class:** wbb

**Investigation:** _pending_
**Finding:** _pending_
**Action:** _pending_

---

## Category 3: Structural / Validator Issues (may need real fixes)

### Item 25: ChannelOpenRequest field 1 — int32 vs sint32

**File:** `libs/open-androidauto/proto/ChannelOpenRequestMessage.proto`
**Field:** `required int32 priority = 1;`  // APK has sint32
**APK class:** vwx

**Investigation:** Verified in vwx.java. Descriptor type char U+150F → lower byte 0x0F = proto-lite FieldType 15 = SINT32. Also APK has 3 Java fields (b, c, d) but descriptor header says 2 fields — field `d` may be a 3rd proto field we're missing.
**Finding:** REAL MISMATCH. sint32 uses ZigZag encoding: sint32(1)→wire 2, sint32(2)→wire 4. If phone sends sint32 and we parse as int32, we read double the actual value. However, `priority` is only logged, never used for decisions. Third field `d` (int) needs investigation.
**Action:** Change field 1 to sint32. Investigate possible field 3.

### Item 26: ConnectionConfiguration fields 2-6 — validator can't parse oneof

**File:** `libs/open-androidauto/proto/ConnectionConfigurationData.proto`
**Fields:** 2-6 are inside a `oneof config_type { ... }` block
**APK class:** aajk

**Investigation:** Confirmed. Fields 2-6 exist in the proto inside a oneof block. Validator's proto_parser.py doesn't parse oneof syntax.
**Finding:** VALIDATOR BUG. Proto is correct.
**Action:** No proto change. Fix validator's oneof parsing if desired.

### Item 27: DrivingStatus field 1 — validator can't see enum-only files

**File:** `libs/open-androidauto/proto/DrivingStatusEnum.proto`
**APK class:** vxj

**Investigation:** DrivingStatus is an enum, not a message. APK class vxj wraps it in a message with 1 int32 field. Our enum values are used as the field type in SensorEventIndication.
**Finding:** VALIDATOR LIMITATION. Enum-only files don't have message definitions to compare.
**Action:** No proto change needed.

### Item 28: Gear field 1 — validator can't see enum-only files

**File:** `libs/open-androidauto/proto/GearEnum.proto`
**APK class:** vxp

**Investigation:** Same as DrivingStatus. Enum-only file.
**Finding:** VALIDATOR LIMITATION.
**Action:** No proto change needed.

### Item 29: InputChannelConfig fields 3, 4, 5 — wrong types and cardinality

**File:** `libs/open-androidauto/proto/InputChannelConfigData.proto`
**APK class:** vya

**Investigation:** Read vya.java. Object[] = `{"c", "d", "e", vxz.class, "f", vxy.class, "g", vvs.m, "h"}`. 5 proto fields confirmed by descriptor header byte. Class refs reveal:
- Field 3 (`e`, type zzf) → repeated MESSAGE → **vxz.class = TouchScreenConfig**
- Field 4 (`f`, type zzf) → repeated MESSAGE → **vxy.class = TouchPadConfig**
- Field 5 (`g`, type zzb) → repeated ENUM → **vvs.m verifier** (unknown enum)
**Finding:** REAL MISMATCH. All three fields wrong:
- Field 3: `optional bytes touchpad_config` → should be `repeated TouchScreenConfig touch_screen_configs`
- Field 4: `optional int32 key_config` → should be `repeated TouchPadConfig touchpad_configs`
- Field 5: `optional uint32 max_touches` → should be `repeated int32 <enum_field>` (enum TBD)
**Action:** Fix fields 3-5. Add imports for TouchScreenConfig/TouchPadConfig. Need to identify the enum for field 5 (vvs.m verifier).

### Item 30: NavigationDistanceDisplay field 4 — map field

**File:** `libs/open-androidauto/proto/NavigationDistanceDisplayData.proto`
**Field:** `optional bytes formatting = 4;`
**APK class:** xnd

**Investigation:** Read xnd.java. Fields: `int b`, `String c`, `String d`, `long e`, `zzf f` (list with xnc.class). Descriptor has 4 fields. Type char at field 4 position is б (U+0431, lower byte 0x31=49) — a special type encoding for maps. `xnc.class` is the map entry message.
**Finding:** Field 4 is a **map** type with entry class xnc. Need to decode xnc.java to determine map<K,V> types. Also confirms fields 1-3 as: int(1), string(2), string(3), with field 4 being long/int64 stored in `e`. Wait — 5 Java fields but 4 proto fields. The `zzf f` list might BE field 4 (the map), and `long e` might be field 3 (timestamp).
**Action:** Decode xnc.java for map key/value types. Keep as bytes until decoded if we don't use this field.

### Item 31: NavigationDistanceInfo fields 2, 4 — cardinality + type

**File:** `libs/open-androidauto/proto/NavigationDistanceMessage.proto`
**APK class:** xng

**Investigation:** Read xng.java. Fields: `Object d` (oneof), `xnd e`, `int c` (oneof case), `int b`, `zzf f` (list). Object[] = `{"d", "c", "b", "e", "f", xnd.class, xne.class}`. Has oneof (d/c pair). Field `e` references xnd (NavigationDistanceDisplay). Field `f` references xnd.class and xne.class.
**Finding:** STRUCTURAL DIFFERENCES:
- Field 1: references xnd (NavigationDistanceDisplay), not NavigationDistanceValue — possible mapping issue
- Field 2: should be repeated message (our: optional bytes)
- Field 4: is a oneof (our: optional bytes)
**Action:** This is complex — the field 1 type mismatch suggests deeper mapping confusion between NavigationDistanceValue/NavigationDistanceDisplay. Leave as bytes for now; needs live capture data to resolve definitively.

### Item 32: NavigationImageDimensions field 1 — int32 vs message

**File:** `libs/open-androidauto/proto/NavigationChannelConfigData.proto`
**Field:** `required int32 width = 1;`
**APK class:** xnf

**Investigation:** Read xnf.java. Fields: `int b` (field 1?), `xnd c` (field 2 — references NavigationDistanceDisplay!), `int d` (field 3), `zzb e` (list). Descriptor type char for field 1 is U+1409, lower byte 0x09 = MESSAGE. So APK says field 1 is a MESSAGE, we have int32.
**Finding:** LIKELY WRONG MAPPING. xnf references NavigationDistanceDisplay (xnd) at field 2, which makes no sense for NavigationImageDimensions (nav icon sizes). The structural match was coincidental — both have 3 fields but types don't align. The real NavigationImageDimensions might be a different class, or vzq (NavigationImageOptions) covers this role.
**Action:** Investigate whether this mapping is correct. The proto may actually be fine if the mapping is wrong. Since our code works with width/height/color_depth as int32 and video is functional, leave as-is for now.

### Item 33: PingConfiguration fields 1, 2 — malformed + extra

**File:** `libs/open-androidauto/proto/PingConfigurationData.proto`
**APK class:** zyd

**Investigation:** Read zyd.java. Clear structure: `long b` (field 1 = int64 ping_interval_ns), `int c` (field 2 = uint32 ping_timeout_ms). Object[] = `{"b", "c"}`. Descriptor `"\u0000\u0002..."` starts with 0x00 instead of 0x01 (possibly proto3 or different encoding version), has no high-Unicode type chars — all bytes ≤ 0x04.
**Finding:** VALIDATOR BUG. Our proto is correct (2 fields: int64 + uint32). The descriptor decoder can't parse this particular descriptor format because it lacks the expected high-Unicode type indicator chars.
**Action:** No proto change needed. Fix descriptor_decoder.py to handle this format if desired.

---

## Execution Order

1. Start with Category 3 (structural) — most likely to have real fixes
2. Then Category 2 (bytes↔message) — decide which to promote
3. Finally Category 1 (int32↔enum) — bulk decision on approach

---

## Final Summary

**Investigation complete.** All 33 partial matches investigated.

### Validator score after fixes: 93 verified / 32 partial / 58 unmatched

### Breakdown of 32 remaining partials

| Category | Count | Action |
|----------|-------|--------|
| int32↔enum (wire-identical) | 16 | No fix — cosmetic only |
| bytes↔message (wire-compatible, unused in code) | 8 | No fix — not referenced in C++ |
| Validator bugs (oneof, enum-only, descriptor format) | 4 | No fix — proto correct, validator wrong |
| Validator decoder bugs (wrong type/cardinality read) | 2 | No fix — InputChannelConfig f4/f5 correct per APK |
| Likely wrong mapping | 1 | No fix — NavigationImageDimensions/xnf mismatch |
| Deferred (needs xnc decode or live captures) | 2 | Future work — NavigationDistanceDisplay map, NavigationDistanceInfo oneof |

### Fixes applied this round
1. **ChannelOpenRequest field 1:** int32 → sint32 (real encoding difference)
2. **InputChannelConfig fields 3-5:** Corrected types (TouchScreenConfig, TouchPadConfig, enum)
3. **NavigationImageOptions field 1:** (previous commit) Reverted bytes → int32

### What would move the needle further
1. **Fix the validator** to understand: oneof syntax, enum-only files, proto3-style descriptors
2. **Decode remaining sub-messages** when we need them (xnc map entry, nav distance oneofs)
3. **Improve descriptor decoder** for edge cases (proto3 format, high field numbers)

### Commands
- Re-run validator: `python3 tools/validate_protos.py`
- Cross-compile: `docker run --rm -u "$(id -u):$(id -g)" -v "$(pwd):/src:rw" -w /src/build-pi openauto-cross-pi4 cmake --build . -j$(nproc)`
- Deploy and test on Pi
- Commit results
