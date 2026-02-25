# Android Auto APK Preprocessing — Design

> **Date:** 2026-02-25
> **Goal:** Create a reproducible indexing pipeline for decompiled Android Auto APK artifacts to accelerate protocol reverse-engineering and gap analysis.

## Objective
Build a local preprocessing pipeline that:
- auto-detects APK `versionName` and `versionCode`
- organizes source/index artifacts under a versioned folder
- generates both SQLite and JSON indexes
- indexes symbols, strings, UUIDs, protocol constants, call edges, and protobuf field usage
- produces summary reports for fast triage

## Scope (v1)
Included:
- Class/method inventory
- String literal extraction
- UUID/Bluetooth constant extraction
- Message/channel/protocol constant extraction
- Lightweight call-graph edges (static reference edges)
- Protobuf field access patterns (`set*`, `get*`, `has*`, builder usage)
- SQLite and JSON outputs

Not included:
- Full control-flow/data-flow analysis
- Deobfuscation renaming heuristics beyond stable IDs
- Dynamic instrumentation

## Canonical Layout
All artifacts live under `openauto-prodigy/analysis/`:

- `analysis/android_auto_<versionName>_<versionCode>/apk-source/`
- `analysis/android_auto_<versionName>_<versionCode>/apk-index/sqlite/`
- `analysis/android_auto_<versionName>_<versionCode>/apk-index/json/`
- `analysis/android_auto_<versionName>_<versionCode>/apk-index/reports/`

## Input Sources
Primary expected source trees (checked in order):
1. Existing decompile root in workspace (current known path)
2. Alternate local decompile paths if configured

Primary APK metadata source (checked in order):
1. Decompiled `AndroidManifest.xml` / decoded manifest metadata
2. `apktool.yml` (if present)
3. AAPT fallback (`aapt dump badging`) if APK is available

## Data Model (SQLite)
Core tables:
- `files(id, rel_path, language, sha256, size_bytes)`
- `classes(id, file_id, class_name, package_name, stable_id)`
- `methods(id, class_id, signature, method_name, stable_id)`
- `strings(id, file_id, value, line_no)`
- `uuids(id, file_id, value, context, line_no)`
- `constants(id, file_id, name, value, kind, line_no)`
- `call_edges(id, caller_method_id, callee_ref, file_id, line_no)`
- `proto_access(id, file_id, message_type, field_name, access_kind, line_no)`
- `meta(key, value)`

Indexes:
- UUID value, constant value, method stable_id, proto message+field, string hash

## JSON Exports
Mirrored exports to support sharing and ad-hoc grep:
- `classes.json`, `methods.json`, `strings.json`, `uuids.json`,
  `constants.json`, `call_edges.json`, `proto_access.json`, `meta.json`

## Report Outputs
`apk-index/reports/summary.md` includes:
- Version and source path
- Top UUIDs and where they appear
- Top protocol constants/messages by frequency
- Top protobuf messages/fields touched
- Hot files by protocol density
- Unresolved/high-obfuscation hotspots

## Safety + Reproducibility
- Non-destructive source relocation:
  - if destination exists, append timestamp suffix to new run
- Deterministic output ordering
- Every run writes run metadata:
  - source hash manifest
  - pipeline version
  - timestamp

## Performance Constraints
- Prefer streaming/text parsing over full AST for v1
- Use `rg` for broad extraction and Python post-processing
- Avoid high-memory whole-repo in-memory graphs
- Target runtime: a few minutes on typical hobby hardware

## Success Criteria
- A versioned analysis folder is created automatically from APK metadata
- Decompiled source is present under `apk-source/`
- SQLite DB and JSON exports are generated
- Summary report is produced
- Queries for UUID/message/protobuf gaps can be answered quickly from index artifacts

## Next Step
Create implementation plan and execute pipeline scripts in small verified steps.
