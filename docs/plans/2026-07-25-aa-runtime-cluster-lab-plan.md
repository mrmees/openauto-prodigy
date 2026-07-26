# Android Auto Runtime CLUSTER Lab Plan

Status: ACTIVE

Grounded on Prodigy `98038cc` and open-android-auto
`dev/android-auto-17.3-analysis` at `231f932b`.

## Goal

Provide one durable, runtime-controlled laboratory for changing the existing
projected CLUSTER display's advertised carrier resolution, DPI, centered
content geometry, and turn-data capability, then renegotiate wireless Android
Auto without editing files or restarting the Prodigy service.

## Approved design

One typed CLUSTER profile is the source of truth for service discovery,
decoded-carrier validation, and QML crop geometry. A controller validates and
applies complete profile snapshots atomically. When projection is active, an
accepted change uses the existing graceful AA disconnect/retrigger path so the
next service-discovery response contains the new profile; Prodigy itself stays
running.

The lab is reachable from Debug Settings and through built-in `aa.cluster.*`
ActionRegistry actions, which are already dispatchable through External API
v1. No frozen API protobuf changes are needed. Runtime overrides are
application-lifetime only in this phase; the existing YAML values remain the
startup defaults.

## Global constraints

- The feature remains default-off behind `experimental_cluster_display`.
- This phase accepts only `DISPLAY_TYPE_CLUSTER`; AUXILIARY remains follow-up
  work.
- H.264 at 30 FPS remains fixed while geometry is varied.
- Supported carriers are the existing landscape 480p and 720p modes.
- Content dimensions must be positive, no larger than the carrier, and produce
  even total margins so the requested content rectangle remains centered.
- Profile changes do not claim to select map versus turn-card content. That is
  recorded as phone policy unless future evidence disproves it.
- `ServiceDiscoveryUpdate` is not used; current 17.3 evidence limits it to a
  narrow input-service update.
- The community proto submodule and frozen External API numerics remain
  untouched.
- MAIN display descriptors, decoding, focus, and touch behavior must not
  change. The opt-in turn-data bit is explicitly session-scoped in AA 17.3 and
  therefore changes the shared session-configuration field.

## Profile contract

The controller accepts a map with these optional keys, overlaying the current
profile before whole-profile validation:

| Key | Values | Default |
|---|---|---|
| `resolution` | `480p`, `720p` | `480p` |
| `dpi` | integer `80..640` | `140` |
| `content_width` | positive integer within carrier | `300` |
| `content_height` | positive integer within carrier | `300` |
| `turn_data_available` | boolean | `false` |

An empty or malformed update is rejected without changing the active profile
or reconnecting. Reset restores the compiled 480p/140-DPI/300x300 baseline.

## Implementation tasks

### Task 1 — Runtime profile and descriptor contract

- Extend `ProjectedDisplayConfig` with a value-type profile, validation,
  normalized map application, resolution mapping, and derived centered
  geometry.
- Make `ServiceDiscoveryBuilder` serialize the supplied CLUSTER profile and OR
  the turn-data capability into the existing session-configuration mask.
- Replace fixed CLUSTER geometry reads in `ProjectedDisplaySession` with a
  per-session profile snapshot.
- Test invalid updates, literal derived margins, descriptor fields, and
  unchanged MAIN discovery.

### Task 2 — Controller and reconnect boundary

- Make the existing CLUSTER `ProjectedDisplaySession` the one QObject
  controller owned by the Android Auto plugin/orchestrator boundary.
- On accepted profile changes, update the next-session snapshot and request one
  reconnect only when AA is active.
- Ensure the builder and CLUSTER session receive the same snapshot before the
  replacement protocol session begins.
- Expose current values, last result text, and generation for diagnostics.
- Test accepted, rejected, unchanged, connected, and disconnected behavior.

### Task 3 — Debug and API control surfaces

- Register `aa.cluster.applyProfile` and `aa.cluster.resetProfile` through
  ActionRegistry when the experimental CLUSTER path exists.
- Add a compact CLUSTER lab section to Debug Settings with baseline/full-frame
  presets and editable resolution/DPI/content controls.
- Keep validation and mutation in the controller; QML and ActionRegistry are
  adapters only.
- Document the actions and runtime reconnect behavior.

### Task 4 — Verification, handoff, and review

- Run focused tests during red/green cycles.
- Update `docs/aa-protocol/aa-display-rendering.md`, relevant reference docs,
  and `docs/session-handoffs.md`.
- Run the native build, explicit `openauto-prodigy` target,
  `ctest --output-on-failure`, and `./cross-build.sh` because embedded QML and
  the Pi artifact change.
- Run one major Codex-authored Fable review through `scripts/review-gate.sh` and
  adjudicate every finding within the repository's two-pass limit.
- Hardware experiments and their observations remain pending until the new Pi
  binary is deployed and a phone session is available.

## Acceptance criteria

- A valid Debug/API profile change updates the next CLUSTER descriptor and crop
  from the same generation and reconnects only the AA session.
- Invalid or no-op updates do not reconnect and do not partially mutate state.
- The current baseline is exactly reproducible through reset.
- MAIN-only and experimental-flag-off behavior remain unchanged.
- Logs identify the applied generation and fully resolved CLUSTER profile.
- Required repository verification and the bounded review gate are green.
