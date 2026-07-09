# AA Proto Phase A Patch Checklist

This checklist operationalizes:
- `docs/plans/2026-02-25-proto-phase-a-migration-manifest.md`

Use it as the execution tracker while patching `.proto` files.

## Status Values
- `todo`
- `in_progress`
- `blocked`
- `done`
- `verified`

## Evidence Expectations
Each completed item should include:
- `Proto diff`: file path + short summary of changed field tags/types
- `Build`: protobuf regenerate + compile success
- `Runtime`: phone log evidence for ServiceDiscovery/channel-open behavior

## Global Preconditions
- [ ] Keep `legacy_aasdk` profile available and working.
- [ ] Ensure working branch has clean proto regeneration procedure documented.
- [ ] Snapshot baseline logs for legacy behavior.

## Task Table

| ID | Task | Files | Depends On | Status | Evidence |
|---|---|---|---|---|---|
| A1 | Decode/refresh descriptors for all Phase A classes | `tools/descriptor_decoder.py`, `tools/proto_decode_output/*` | none | todo | |
| A2 | Confirm class mapping for InputChannel path (`vya`, `zpl`, `zpn`, `vxn`) | `tools/proto_decode_output/*`, decompiled classes | A1 | todo | |
| A3 | Patch `ServiceDiscoveryResponseMessage` tags/types to APK mapping | `libs/open-androidauto/proto/ServiceDiscoveryResponseMessage.proto` | A1 | todo | |
| A4 | Patch `ChannelDescriptorData` (`wbw`) to full APK tag map (including fields 16/17 strategy) | `libs/open-androidauto/proto/ChannelDescriptorData.proto` | A1 | todo | |
| A5 | Patch `AVChannelData` (`vys`) tag/type mismatches (incl. enum-vs-int review for field 8) | `libs/open-androidauto/proto/AVChannelData.proto` | A1 | todo | |
| A6 | Patch `InputChannel*` subtree (`vya`/`zpl`/`zpn`) tags/types/cardinality | `libs/open-androidauto/proto/InputChannelDescriptorData.proto`, `libs/open-androidauto/proto/InputChannelData.proto`, `libs/open-androidauto/proto/InputChannelConfigData.proto` | A2 | todo | |
| A7 | Patch `TouchConfigData` (`vxn`) add APK field 3 semantics | `libs/open-androidauto/proto/TouchConfigData.proto` | A2 | todo | |
| A8 | Patch `AVInputChannelData` (`vyt`) to APK mapping | `libs/open-androidauto/proto/AVInputChannelData.proto` | A1 | todo | |
| A9 | Patch remaining descriptor nested configs (`wbu`, `vvp`, `wcz`, `vwc`, `vzr`, `wdh`) | corresponding files under `libs/open-androidauto/proto/` | A1 | todo | |
| A10 | Regenerate protobuf sources and compile | build scripts / proto generation targets | A3-A9 | todo | |
| A11 | Add/adjust runtime profile gate (`legacy_aasdk` vs `apk_v16_1`) | runtime config/code paths | A3-A10 | todo | |
| A12 | On-device handshake test: ensure channels open under `apk_v16_1` | device logs + capture docs | A11 | todo | |
| A13 | Regression test `legacy_aasdk` path still works | device logs + capture docs | A11 | todo | |
| A14 | Update docs with final field map and migration rationale | `docs/*` | A12-A13 | todo | |

## A1 Prefill: Decode/Refresh Descriptors

### Commands

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
mkdir -p tools/proto_decode_output/phase_a

python3 tools/descriptor_decoder.py --class \
  wby wbw wbu vys vya zpl zpn vxn vyt vvp wcz vwc vzr wdh \
  > tools/proto_decode_output/phase_a/phase_a_classes.txt

python3 tools/descriptor_decoder.py --class \
  wby wbw wbu vys vya zpl zpn vxn vyt vvp wcz vwc vzr wdh \
  --json > tools/proto_decode_output/phase_a/phase_a_classes.json
```

### Expected Artifacts
- `tools/proto_decode_output/phase_a/phase_a_classes.txt`
- `tools/proto_decode_output/phase_a/phase_a_classes.json`

### Acceptance Criteria
- Output contains all requested classes.
- Each class has field number, wire type, and cardinality metadata.
- No decoder errors for missing classes.

### Quick Checks

```bash
rg -n "\"apk_class\": \"(wby|wbw|wbu|vys|vya|zpl|zpn|vxn|vyt|vvp|wcz|vwc|vzr|wdh)\"" \
  tools/proto_decode_output/phase_a/phase_a_classes.json
```

## A2 Prefill: Confirm InputChannel Mapping (`vya`, `zpl`, `zpn`, `vxn`)

### Commands

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy

# Class-level decode (human-readable)
python3 tools/descriptor_decoder.py --class vya zpl zpn vxn \
  > tools/proto_decode_output/phase_a/input_path_decode.txt

# Graph/cross-reference hints
python3 tools/aa_proto_graph.py > /tmp/aa_proto_graph_refresh.log 2>&1 || true
rg -n "\"(vya|zpl|zpn|vxn)\"" tools/aa_proto_graph.json

# Decompiled usage trace
rg -n "\\b(vya|zpl|zpn|vxn)\\b" \
  /home/matt/claude/personal/openautopro/firmware/android-auto-apk/decompiled/sources/defpackage \
  > tools/proto_decode_output/phase_a/input_path_usages.txt
```

### Expected Artifacts
- `tools/proto_decode_output/phase_a/input_path_decode.txt`
- `tools/proto_decode_output/phase_a/input_path_usages.txt`

### Acceptance Criteria
- A single mapping decision is recorded:
  - which APK class corresponds to `InputChannelDescriptorData.proto`
  - which corresponds to `InputChannelData.proto`
  - which corresponds to `InputChannelConfigData.proto`
  - confirmation that `TouchConfigData.proto` maps to `vxn`
- Decision is backed by descriptor shape and call-site/context evidence.

### Required Decision Note (append to task evidence)

```text
Mapping decision:
- InputChannelDescriptorData.proto -> <apk class>
- InputChannelData.proto -> <apk class>
- InputChannelConfigData.proto -> <apk class>
- TouchConfigData.proto -> vxn (confirmed/unconfirmed)

Rationale:
- descriptor shape:
- field/cardinality match:
- usage context:
```

## Per-Task Template

Copy this block under each task when working:

```text
Task ID:
Status:
Owner:
Date:

Changes:
- file:
  summary:

Verification:
- command:
  result:

Runtime evidence:
- log/capture path:
  key lines:

Notes/Risks:
```

## Stop Conditions
- Any `FATAL` mismatch unresolved in Phase A subtree.
- Channel-open remains stuck on ping/pong under `apk_v16_1`.
- Loss of legacy functionality without explicit approval.
