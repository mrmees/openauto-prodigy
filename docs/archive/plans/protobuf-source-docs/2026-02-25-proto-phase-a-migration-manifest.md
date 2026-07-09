# AA Proto Migration Manifest (Phase A: Service Discovery Tree)

## Scope
- Profile: `apk_v16_1`
- Goal: atomic migration of capability advertisement subtree only
- Affected flow: `ServiceDiscoveryResponse -> ChannelDescriptor -> nested channel configs`

## Rules
- `FATAL`: field number mismatch for semantic fields, wire-type mismatch, message-vs-bytes mismatch, missing required semantic field
- `HIGH`: packed/unpacked mismatch on repeated numeric fields
- `MEDIUM`: enum vs int32/uint32 where numeric values align
- `LOW`: name/comment differences only

## Status Legend
- `todo`
- `decoded`
- `diffed`
- `patched`
- `verified`

## Phase A Inventory
1. `ServiceDiscoveryResponseMessage` (`wby`)
2. `ChannelDescriptorData` (`wbw`)
3. `SensorChannelData` (`wbu`) and nested sensor wrappers
4. `AVChannelData` (`vys`) and nested:
   - `AudioConfigData` (`vvp`)
   - `VideoConfigData` (`wcz`)
5. `InputChannelDescriptorData` (`vya`) and nested:
   - `InputChannelConfigData` (`zpn`)
   - `TouchConfigData` (`vxn`)
   - touch config wrappers (`vxz`, `vxy`, etc.)
6. `AVInputChannelData` (`vyt`)
7. `BluetoothChannelConfigData` (`vwc`)
8. `NavigationChannelData` (`vzr`)
9. `NotificationChannelData` (`wdh`)

## Prefilled Rows (Known Mismatches)

| Message | APK Class | Field # | Field Name (ours) | APK Type/Cardinality | Our Type/Cardinality | Severity | Action | Status | Evidence |
|---|---|---:|---|---|---|---|---|---|---|
| ChannelDescriptorData | wbw | 1..17 | mixed subtree fields | 17 fields in APK; includes fields 16 and 17 | 15 fields in ours; 16 and 17 missing | FATAL | Retag all top-level fields to APK map; add 16/17 placeholders or real messages | diffed | `libs/open-androidauto/proto/ChannelDescriptorData.proto`, `tools/proto_decode_output/matched/ChannelDescriptorData__wbw.proto` |
| ChannelDescriptorData | wbw | 7,9..15 | mixed subtree fields | APK ordering differs from aasdk mapping | ours shuffled relative to APK | FATAL | Atomic retag with full nested subtree migration (no partial release) | diffed | `libs/open-androidauto/proto/ChannelDescriptorData.proto` comments + phone behavior notes |
| AVChannelData | vys | 8 | `keycode` | enum in APK (`vyh`) | `uint32` in ours | MEDIUM | keep number, consider enum type alignment once enum map confirmed | diffed | `tools/proto_decode_output/matched/AVChannelData__vys.proto`, `libs/open-androidauto/proto/AVChannelData.proto` |
| AVChannelData | vys | 5 | `available_while_in_call` | absent in APK v16.1 | present in ours | MEDIUM | keep reserved/commented compatibility field; verify encoder does not emit unexpectedly | diffed | `tools/proto_decode_output/matched/AVChannelData__vys.proto` |
| InputChannelData (mapping requires confirmation) | zpl/vya | 1 | `supported_keycodes` | repeated numeric encoded packed (reported from APK trace) | repeated `uint32` (proto2; unpacked behavior in current implementation) | HIGH | confirm exact class mapping and wire mode from descriptor; switch to packed repeated numeric if confirmed | todo | `libs/open-androidauto/proto/InputChannelData.proto`, `tools/descriptor_decoder.py --class zpl vya zpn vxn` |
| TouchConfigData | vxn | 3 | missing in ours | `bool` field exists in APK | ours has only width/height (2 fields) | HIGH | add field 3 with APK type and default semantics | diffed | `tools/proto_decode_output/matched/TouchConfigData__vxn.proto`, `libs/open-androidauto/proto/TouchConfigData.proto` |

## Work Plan (Atomic)
1. Freeze legacy behavior under `legacy_aasdk` profile.
2. Build machine-readable diff for all Phase A messages from `descriptor_decoder.py` output.
3. Patch all Phase A `.proto` files in one branch (top-level + nested).
4. Regenerate protobuf code and compile.
5. Run handshake tests on device:
   - BT discovery
   - ServiceDiscoveryResponse accepted
   - channel opens proceed (not ping/pong-only stall)
6. Only then flip `apk_v16_1` profile on for integration testing.

## Exit Criteria
- No open `FATAL`/`HIGH` rows in Phase A.
- `apk_v16_1` profile opens channels successfully on phone.
- Regression check: `legacy_aasdk` profile still works.

## Commands
```bash
# decode relevant descriptors
python3 tools/descriptor_decoder.py --relevant --json > tools/proto_decode_output/relevant.json

# inspect critical classes
python3 tools/descriptor_decoder.py --class wbw wby vys vya zpl zpn vxn vvp wcz vyt vwc vzr wdh
```
