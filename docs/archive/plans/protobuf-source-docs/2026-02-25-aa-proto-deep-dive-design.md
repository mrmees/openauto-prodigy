# AA Protocol Deep Dive — Design

**Date:** 2026-02-25
**Goal:** Build a complete AA protocol reference by graph-walking all proto dependencies from known roots, then document every message, enum, and field organized by channel/feature.

## Context

We built `proto_decoder.py` which decoded 1,940 of 1,943 protobuf message classes from the AA v16.1 APK. Structural matching identified 29 high-confidence unique matches between APK classes and our proto library. Live capture with Samsung S25 Ultra confirmed the decoder output and revealed new data (PNG phone icons in ServiceDiscoveryRequest, session UUIDs, etc.).

We now have the Rosetta Stone — known APK class ↔ proto name mappings — and the full decoded schema for every proto in the APK. Time to extract everything AA-related before context is lost.

## Approach: Graph Walk + Gearhead Scan

### Phase 1: Graph Walk from Known Roots

Start from the 29 unique-match APK classes (e.g., wby=ServiceDiscoveryResponse, wbw=ChannelDescriptor, vys=AVChannel, wcz=VideoConfig, etc.). For each:

1. Load its decoded fields from `apk_protos.json`
2. For every MESSAGE/REPEATED_MESSAGE field, add the referenced class to the frontier
3. For every ENUM field, add the enum class to the enum set
4. BFS until no new classes are discovered
5. Tag each proto with the channel(s) it's reachable from

**Do NOT seed from ambiguous matches** (vxn, aass, vzy, aagl, poe, ufg — generic wrappers that would pull in half the APK).

### Phase 2: Gearhead Code Scan

Grep the 28 named Java files in `com.google.android.gearhead` for references to any defpackage class name that appears in our decoded proto set. Any hit becomes an additional root (tagged "gearhead-referenced"). Run the same BFS from these new roots.

### Phase 3: Classification & Naming

For each discovered proto:
- Assign a channel tag based on which root it descends from
- Infer field names from types, positions, and enum associations
- Cross-reference with live capture data where available
- Mark confidence level (confirmed by live capture, inferred from structure, unknown)

## Output

### 1. JSON Knowledge Base (`tools/aa_proto_graph.json`)

Machine-readable. Contains:
- All AA-relevant message classes with fields, types, references
- All AA-relevant enums with values
- Channel assignments and dependency graph
- Our proto name mappings where known

### 2. Markdown Reference (`docs/aa-protocol-reference.md`)

Human-readable, organized by channel:
- Control, Video, Audio (×3), Sensor, Input, Bluetooth, WiFi, Navigation, Media Status, Phone Status
- Each section: message IDs, proto fields with names, enum values, live capture notes

### 3. Updated .proto Files

Only for sub-messages of things we already parse (wcm in VideoConfig, wbb in WifiSecurity, utk in NavigationStep, session_info sub-message, etc.). Not bulk-adding all unknowns.

## What's Out of Scope

- Decoding the 50 failed protos (revisit if any are AA-relevant)
- Reverse-engineering Java business logic
- Adding new channel handlers (follow-up work)
- Building a database (JSON is sufficient)

## Success Criteria

1. Every proto reachable from known AA roots is documented
2. Every field has number, type, and best-guess name
3. Every enum has its values listed
4. Reference doc answers "what does the phone send on channel X?"
5. JSON is structured for future tool consumption

## Implementation

Single Python script `tools/aa_proto_graph.py` with phases:
1. Load `apk_protos.json` (already decoded)
2. Define seed set from unique matches
3. BFS graph walk collecting all reachable protos + enums
4. Scan gearhead sources for additional roots, BFS from those
5. Classify by channel, infer names
6. Output JSON + Markdown + identify proto files to update
