# Proto Validation Pipeline — Design

**Date:** 2026-02-25
**Status:** Approved

## Problem

We have 150 proto files defining the Android Auto wire protocol. These were originally reverse-engineered by aasdk (2018) and extended by us using APK decompilation. We've already hit cases where comments claimed wrong field numbers (wbw/ChannelDescriptor), wasting debugging time. We need systematic validation that our proto definitions match the actual APK v16.1.

## Goal

A rerunnable Python tool that cross-references our proto files against the decompiled APK source, producing a per-proto confidence report. Two phases:

1. **Automated validation (B-level):** Decoder output + Java source cross-reference for all 150 protos.
2. **Live validation (C-level):** Phone testing for ~20 hot-path protos (manual, separate effort).

## Design

### Tool: `tools/validate_protos.py`

**Step 1: Parse our protos.** Read each `.proto` in `libs/open-androidauto/proto/`, extract field numbers, types, cardinality, and message/enum names. Simple regex parser — proto2 is regular enough.

**Step 2: Match to APK classes.** Use the existing high-confidence matches (29 unique 1:1 pairs from `apk_protos.json`) as seeds. For remaining protos, use structural matching scores (85 total matches). Flag ambiguous matches for manual review.

**Step 3: Decode and compare.** For each matched pair, run the descriptor decoder on the APK class and diff field-by-field:
- Field number match
- Base type match (int32/uint32/string/message/enum/bytes/bool)
- Cardinality match (optional/required/repeated/packed)
- For message-typed fields: recursively verify the referenced class
- For enum-typed fields: verify the enum verifier class

**Step 4: Triage decoder errors.** Before the full run, check whether any of the 50 decoder-error classes match our protos. Fix decoder for those specific cases; skip the rest.

**Step 5: Generate output.**
- `tools/proto_validation_report.json` — per-proto machine-readable results
- `docs/proto-validation-report.md` — human-readable summary

### Confidence Levels

| Level | Meaning |
|-------|---------|
| **verified** | 1:1 APK match, all fields match |
| **partial** | APK match found but field diffs exist |
| **unmatched** | No APK class found |
| **error** | Decoder failed on matched APK class |

### Scope Boundaries

- Report only — does not modify proto files (human decides what to fix)
- No live phone testing (separate C-level effort)
- No APK class discovery for new protos (expansion phase, after validation)
- Rerunnable against future APK versions

### Dependencies

- `tools/descriptor_decoder.py` — existing decoder (works correctly, 50 classes with errors)
- `tools/proto_decode_output/apk_protos.json` — existing match data
- APK decompiled source at `firmware/android-auto-apk/decompiled/`
