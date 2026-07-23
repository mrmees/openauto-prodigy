# Configuration Startup Contract Remediation — Design

Date: 2026-07-22
Status: COMPLETED 2026-07-22
Grounded on: `45f6684e39beb649a1df046f64337a98e8023ab9`

## Goal

Make the existing logging configuration and display-brightness startup paths
honor their documented values without broadening the public configuration API.
Malformed YAML that replaces a known mapping with a scalar must also enter the
existing corrupt-file fallback instead of surviving load and failing later.

## Current Contract Breaks

1. `logging.verbose` and `logging.debug_categories` are consumed by startup,
   settings, and IPC code but are absent from the built-in schema. Scalar
   writes are therefore rejected and the sequence has no typed read/write path.
2. A user scalar can replace a built-in mapping during deep merge. `load()`
   returns successfully, but a later typed accessor can throw `BadSubscript`.
3. `DisplayService` begins with an observable brightness of 80. Applying a
   saved value of 80 at startup returns early before the selected backend sees
   an initial assignment.

## Design Decisions

### Logging configuration

- Register `logging.verbose` as a scalar boolean defaulting to `false`.
- Register `logging.debug_categories` as an empty sequence.
- Keep the generic dot-path bridge scalar-only. This lets the existing QML
  verbose toggle work while preserving the established rule that sequences use
  typed accessors.
- Add typed `YamlConfig` accessors for verbose mode and the category list.
- Derive one effective runtime mode everywhere:
  - verbose `true` enables all debug categories;
  - verbose `false` applies the persisted selective category list, including
    an empty list for normal quiet mode;
  - changing the selective category list selects non-verbose mode so its live
    and post-restart behavior agree.
- The command-line `--verbose` startup override remains authoritative for that
  startup. Later explicit settings or IPC mutations retain their current live
  control behavior.
- IPC applies the in-memory mutation live but reports a failed schema write or
  disk save as `ok: false`; it must not claim persistence succeeded.

### YAML merge safety

- When a built-in mapping exists, an overlay for that node must also be a
  mapping. A scalar or sequence at that location is structurally invalid.
- Throw during the merge so the existing `YamlConfig::load()` catch path moves
  the file aside as `.corrupt`, restores the complete defaults tree, and never
  exposes a partially incompatible tree to typed accessors.
- Unknown user keys remain retained. Scalar value conversions and sequence
  replacement behavior outside the known-map mismatch are unchanged.

### Initial brightness application

- Preserve the public default and observable property value of 80.
- Track whether any brightness value has reached the backend.
- The first `setBrightness()` call applies the clamped value even when it equals
  80. It emits `brightnessChanged` only if the observable value changed.
- Once initialized, repeated identical values remain suppressed. Later changed
  values keep the current clamp, backend, signal, and software-overlay behavior.
- A narrow overridable backend-application seam permits deterministic tests
  without accessing real sysfs or DDC hardware.

## Acceptance Criteria

- Logging defaults are readable with `verbose == false` and an empty category
  list.
- Scalar settings writes can change and persist `logging.verbose`.
- Typed category lists retain every string through save/reload.
- Startup and later IPC/settings mutations apply the same verbose/selective
  precedence, and selective categories remain effective after restart.
- Logging IPC returns failure when mutation or persistence fails.
- A valid YAML document that replaces a known mapping with a scalar is moved to
  `.corrupt`; subsequent typed reads return defaults without throwing.
- Unknown keys and valid deep merges retain their current behavior.
- A startup brightness of 80 reaches the backend exactly once without emitting
  a false property-change notification; repeated 80 assignments are suppressed.
- Existing non-default brightness, clamp, software-overlay, and notification
  behavior remains unchanged.

## Out of Scope

- Logging format, destinations, retention, general startup-noise auditing, or
  new logging categories.
- QML layout/control changes, video-codec sequence controls, or a generalized
  sequence-capable `ConfigService` API.
- Android Auto protocol/protobuf, External API, wireless transport, Bluetooth,
  HFP, ofono, PipeWire routing, or equalizer behavior.
- Display backend discovery or a hardware-backlight redesign.

## Verification and Pi Matrix

### Required locally

- Focused YAML/configuration, logging IPC, and display-service tests.
- Full local build, explicit `openauto-prodigy` target, and full CTest suite.
- `git diff --check` and the repository review gate with every finding
  adjudicated.
- Aarch64 cross-build before any deployment.

### Required after separately approved deployment

- One application process and responsive IPC.
- Verbose and selective-category changes apply live and survive an application
  restart.
- The configured brightness remains correct after restart.
- Bluetooth and hostapd retain their PIDs; neither daemon is restarted.

The current Pi uses the software-overlay brightness backend. Exact proof that a
default-valued first assignment reaches a hardware backend therefore comes from
the deterministic fake-backend unit test, not from a claim about unavailable Pi
hardware.

### Optional

- Repeat logging mutations from both the in-car toggle and web configuration.
- Exercise a non-default brightness and restore the original value.

### Not required

- Bluetooth restart or re-pairing, HFP calls, Android Auto protocol capture,
  or unrelated QML inspection.

Any binary deployment, application restart, configuration mutation on the Pi,
or other disruptive operation requires Matthew's separate approval.
