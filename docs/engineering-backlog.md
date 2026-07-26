# Engineering Backlog

This is an evidence ledger, not executable scope. The current audit was
grounded on dev at bbce99a after PRs #26–#37. “Code-confirmed” means the stated
condition is visible in that tree; it does not mean the proposed remedy is
approved or still correct after later changes.

Before promotion, every entry must be re-researched against the then-current
tree, recent remediation plans, tests, and hardware where applicable. Close
anything already fixed or no longer reproducible. A surviving item must be
rewritten into bounded acceptance criteria, exact files, explicit out-of-scope
limits, and a verification command before implementation.

## Configuration and Web Config

- **Web settings submit fields the IPC writer ignores** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. settings.html sends video_resolution,
  brightness, and night_mode, while IpcServer::handleSetConfig handles none of
  them and still returns success. Candidate deliverable: each control either
  writes through its canonical runtime owner with visible failure or is removed.

- **Video-codec controls use scalar-only generic config access** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. DebugSettings sends an array to
  ConfigService, but YamlConfig generic reads/writes reject sequences; service
  discovery can also advertise VP9/AV1 although the shipped decoder supports
  only H.264/H.265. Candidate deliverable: typed sequence persistence and
  decoder-aligned advertisement.

- **Installers write unsupported display dimensions** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. Both installers emit display.width and
  display.height, which are absent from runtime defaults and typed access;
  connection.bt_name works only because unknown loaded keys survive merge.
  Candidate deliverable: installers write only supported schema, and every
  intended installer key has runtime and documentation coverage.

- **Web config has no route authentication** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. Flask exposes read and mutation routes on the
  configured LAN listener without an authentication boundary. Candidate
  deliverable: one coherent authentication/session design protects every page
  and API route, including config and theme upload.

- **Web-config IPC failures are classified by English substrings** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. Theme upload maps a hand-maintained subset of
  error text to HTTP 503, while empty or unexpected transport failures become
  500. Candidate deliverable: ipc_request returns structured
  transport-versus-application results consumed consistently by all routes.

## Bluetooth, Boot, and Audio Recovery

- **Initial BlueZ snapshots do not self-retry after failure** — Evidence:
  **CODE-CONFIRMED 2026-07-24** and explicitly left open by the PR #33 review.
  BluetoothManager and BtAudioPlugin retain state and log a failed initial
  GetManagedObjects reply but schedule no paced retry unless a later topology
  event happens. Candidate deliverable: transient startup failure recovers
  without restarting the application or Bluetooth.

- **Pairing fallback name does not refresh from a later snapshot** — Evidence:
  **CODE-CONFIRMED 2026-07-24** and explicitly left open by the PR #33 review.
  A prompt can capture a MAC-derived name before deviceNamesByPath is populated;
  applyManagedObjectsSnapshot replaces the cache without updating the active
  prompt. Candidate deliverable: the same prompt publishes the resolved name
  when authoritative device data arrives.

- **SDP socket ownership depends on a short polling race** — Evidence:
  **HARDWARE REVALIDATION REQUIRED**. The standardized BlueZ drop-in still tries
  five half-second polls for /var/run/sdp and silently exits if creation is
  later; the older bench observed root:root after clean boots. Candidate
  deliverable: service ordering or creation-aware handling guarantees the
  bluetooth group owns a writable socket before application SDP registration.

- **User-service diagnostics do not survive reboot** — Evidence:
  **INSTALLER GAP CONFIRMED 2026-07-24; HARDWARE POLICY REQUIRED**. Neither
  installer provisions persistent, bounded journald storage. Candidate
  deliverable: enough capped history survives reboot for PipeWire/WirePlumber
  forensics without unbounded SD-card wear.

- **AudioService cannot reconnect after PipeWire daemon loss** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. PipeWire context, core, loop, and registry are
  constructed once; stream errors are surfaced but there is no core-loss
  reinitialization path. Candidate deliverable: daemon restart repopulates
  devices and allows playback/capture again without restarting Prodigy.

- **HFP profile loss after external stack restart has no runtime recovery** —
  Evidence: **HARDWARE REVALIDATION REQUIRED**. Installers now restart
  Bluetooth before PipeWire/WirePlumber, but the running app does not detect a
  missing AudioGateway profile or remediate RegisterProfile failure. Candidate
  deliverable: HFP availability recovers or surfaces an actionable degraded
  state after independently restarted services.

- **BT EQ tap does not recreate streams after a PipeWire stream error** —
  Evidence: **CODE-CONFIRMED 2026-07-24**. BtAudioTap tears down on playback or
  capture error and remains stopped. Candidate deliverable: bounded recovery
  recreates the tap safely while preserving direct-to-sink fallback.

- **BT EQ tap does not adopt pre-existing live BlueZ input nodes** — Evidence:
  **CODE-CONFIRMED 2026-07-24; HARDWARE REVALIDATION REQUIRED**. The
  WirePlumber rule retargets nodes at creation, but tap startup contains no
  graph sweep/relink for a stream already alive. Candidate deliverable:
  starting Prodigy during active A2DP routes that existing stream through EQ
  and head-unit volume without reconnecting the phone.

## Android Auto, Calls, and Core Lifecycle

- **Runtime CLUSTER action validation is not observable over External API** —
  Evidence: **CODE-CONFIRMED 2026-07-25**. ActionRegistry reports whether an
  action ID was dispatched, while the CLUSTER controller's accepted/rejected
  result is available only through its QML diagnostics and application log.
  Candidate deliverable: if the experimental lab becomes a supported remote
  surface, add an additive profile/result publisher or an action-result
  contract without weakening the frozen External API rails.

- **Protocol-library session-configuration enum lags the AA 17.3 turn-data
  bit** — Evidence: **STATIC AA 17.3 CONFIRMED; SUBMODULE FOLLOW-UP REQUIRED**.
  AA 17.3 `ity.d` maps `xmm.UI_ELEMENT_NAVIGATION_TURN_DATA_AVAILABLE` to bit
  16, while the hands-off protocol submodule's `SessionConfigurationEnum.proto`
  documents only the four 16.2 values. Candidate deliverable: confirm the
  current-app consumer path in open-android-auto, then add the named bit and
  updated provenance there; Prodigy must not patch the submodule proto locally.

- **AUXILIARY display semantics and projected content need current-app/live
  confirmation** — Evidence: **STATIC AA 17.3 ANALYSIS; LIVE REVALIDATION
  REQUIRED**. The pinned open-android-auto analysis and maintainer response in
  issue #10 describe AUXILIARY as an independent logical display with
  navigation/turn-card routing, no evidenced media/phone projection path, and
  no runtime service replacement through message 26. The response also warns
  that some supporting documents retain stale 16.2 class names. Candidate
  deliverable: re-trace current Google Maps and YouTube Music, refresh stale
  symbols, then capture one bounded MAIN+CLUSTER+AUXILIARY session before
  promoting the next display-type phase.

- **AA EventBus connections accumulate across sessions** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. Navigation and media-status value-member
  handlers are connected during each new session but omitted from teardown's
  sender-to-orchestrator disconnect set. Candidate deliverable: one publication
  per handler signal after any number of reconnects.

- **Malformed navigation strings can flood protobuf UTF-8 diagnostics** —
  Evidence: **HARDWARE REVALIDATION REQUIRED; HANDLING GAP CODE-CONFIRMED**.
  NavigationChannelHandler parses proto string fields and converts them
  directly, with no invalid-byte policy; the previous phone bench logged
  NavigationTurnLabel errors continuously. Candidate deliverable: malformed
  phone text cannot flood logs and yields safe display text without modifying
  the hands-off proto definitions here.

- **Master-volume persistence retries forever** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. A failed YamlConfig save restarts the same
  two-second timer without a cap. Candidate deliverable: bounded retries,
  one terminal diagnostic, and a defined later mutation/restart recovery edge.

- **Native incoming-call UI has no projection ownership rule** — Evidence:
  **CODE-CONFIRMED 2026-07-24; HARDWARE REVALIDATION REQUIRED**.
  IncomingCallOverlay is visible for every Ringing state, does not consult
  ProjectionStatus, and owns only QML MouseAreas while AA Pi touch is routed
  through evdev. Candidate deliverable: AA-owned calls do not raise the native
  popup; outside AA, native answer/reject owns the actual touch path.

## Local Media

- **UDisks MountPoints decoding emits read-only QDBusArgument warnings** —
  Evidence: **LIVE HARDWARE CONFIRMED 2026-07-24** on the
  milestone-equivalent runtime. A clean boot emits exactly three warnings while
  the initial USB scan decodes the mounted root, boot, and removable filesystems;
  the former Bluetooth MediaPlayer1 Track-type warning is absent. Candidate
  deliverable: MountPoints decoding is warning-free for system and removable
  filesystems, with representative D-Bus variant coverage.

- **Valid tracks shorter than 500 ms can be classified as unplayable** —
  Evidence: **CODE-CONFIRMED 2026-07-24**. PlaybackPolicy compares the observed
  progress watermark with a fixed 500 ms threshold, while PlaybackEngine
  throttles progress publication to that same interval. Candidate deliverable:
  known-valid short clips and near-end seeks do not consume unplayable strikes.

- **Filesystem MountPoints changes are ignored** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. UsbMediaWatcher subscribes to generic
  PropertiesChanged but returns unless the interface is Drive1; external
  Filesystem MountPoints changes are not reconciled. Candidate deliverable:
  external mount/unmount changes update sources, playback, and library state
  exactly once.

- **Eject affordance is inferred from path prefixes** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. The destructive action validates
  UsbMediaWatcher ownership, but source enumeration/UI eligibility still uses
  /media, /run/media, and /mnt heuristics. Candidate deliverable: the model
  exposes authoritative removable/canEject state and the UI uses it.

- **Nested media roots can duplicate records** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. refreshSources deduplicates exact canonical
  roots only and explicitly leaves nested-root precedence out of scope.
  Candidate deliverable: deterministic root precedence and record-level
  canonical deduplication.

## Web-Widget Runtime

- **Shim readiness is one-shot across reconnects** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. prodigy.ready resolves once and never represents
  a later disconnected interval. Candidate deliverable: callers can reliably
  distinguish connected readiness across repeated WebSocket lifetimes.

- **Subscription ownership is fire-and-forget and unsubscribe is local-only** —
  Evidence: **CODE-CONFIRMED 2026-07-24**. subscribe does not surface the
  server response, and its returned function removes only the callback while
  the server retains the topic until another rebuild. Candidate deliverable:
  acknowledged subscription state and server-side unsubscribe/rebuild.

- **Shim reconnect can die on malformed URL and drops connection-level errors**
  — Evidence: **CODE-CONFIRMED 2026-07-24**. WebSocket construction inside the
  reconnect callback is unguarded, and request-id-zero Error frames fall
  through stream handling. Candidate deliverable: bounded reconnect survives
  configuration errors and exposes connection-level failures.

- **Manifest traversal validation over-rejects safe filenames** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. entry.contains("..") rejects names such as
  foo..bar.html even though the resolver's canonical-path jail owns real
  traversal defense. Candidate deliverable: component-aware path validation
  accepts safe names and rejects escape attempts.

- **A transient empty widget URL destroys a live web view** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. WebWidgetHost binds Loader.active directly to
  the current effective-config URL rather than latching the activation URL.
  Candidate deliverable: a live view survives transient model/config gaps and
  changes URL only through an explicit lifecycle.

- **Every page instantiates the full widget model** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. HomeMenu repeats WidgetGridModel on each page
  and filters delegates by visibility/page; isCurrentPage keeps expensive work
  dormant but duplicate QML items still exist. Candidate deliverable: a
  per-page filtered model or Loader gate instantiates one delegate per placement.

- **JS proto generation is not byte-reproducible** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. gen-proto-js.sh installs major-version ranges
  for protobufjs and protobufjs-cli. Candidate deliverable: exact toolchain
  pins plus a documented/regression-checked regeneration procedure.

## Release Engineering and Documentation

- **Fable review-gate invocation needs a reproducible readiness/progress
  check** — Evidence: **LOCAL TOOLING OBSERVATION 2026-07-25**. Two Fable
  launches created the pinned prompt/diff and a live companion/Claude process
  but no raw output or completion signal during polling; the user authorized
  an Opus fallback, which completed the same immutable review range. Candidate
  deliverable: distinguish slow healthy execution from model/runtime failure,
  expose job progress without direct process inspection, and verify the
  `claude-fable-5` invocation used by `review-gate.sh`.

- **Prebuilt installs do not own the wait-online boot policy** — Evidence:
  **INSTALLER GAP CONFIRMED 2026-07-24; FRESH-IMAGE VALIDATION REQUIRED**. The
  source-installed Pi reaches Prodigy READY at roughly 32 seconds with both
  NetworkManager and systemd-networkd wait-online units disabled, while
  install-prebuilt.sh has no equivalent action and the target image presets
  systemd-networkd-wait-online enabled. Candidate deliverable: both install
  modes share one explicit wait-online contract and a fresh prebuilt install
  proves no unused network manager delays application readiness.

- **External API lacks a live user reference** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. The shipped API is documented only through
  archived design history and scattered authoring notes. Candidate deliverable:
  docs/reference/external-api.md covers transports, pairing/auth, requests,
  subscriptions, errors, capabilities, and additive compatibility.

- **Release packager trusts unsafe identity inputs and does not prove tag
  agreement** — Evidence: **CODE-CONFIRMED 2026-07-24**.
  package-prebuilt-release.sh accepts arbitrary version/target path components,
  defaults official-looking output to a timestamp, removes the derived staging
  path, and does not compare annotated tag, HEAD, or embedded binary version.
  Candidate deliverable: strict input allowlists and tag/commit/binary agreement
  are mandatory for official packaging.

- **Patched libspa packages have no revision upgrade path** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. Both installers return early for any installed
  +prodigy version, so a newer bundled patch is never selected. Candidate
  deliverable: deterministic candidate selection and dpkg version comparison
  safely unhold, upgrade, and re-hold only when newer.

- **Cross-build can leave an orphan container mutating build-pi** — Evidence:
  **CODE-CONFIRMED 2026-07-24; FAILURE PREVIOUSLY OBSERVED**. cross-build.sh
  runs an unnamed container against the writable in-repo build directory with
  no stale-container guard. Candidate deliverable: one owned container identity
  and cleanup policy prevent concurrent/orphan mutation.

- **Secret and documentation-path checks are incomplete** — Evidence:
  **CODE-CONFIRMED 2026-07-24**. No repository/history secret scanner is
  configured, and check-doc-links.py validates Markdown link syntax but not
  backticked live .md paths. Candidate deliverable: lightweight reproducible
  checks cover both without scanning generated/archive content incorrectly.

## Closed During the 2026-07-24 Audit

- Async USB drive-property resolution shipped in PR #36; UsbMediaWatcher now
  uses QDBusPendingCallWatcher for Drive and Filesystem property reads.
- ApiServer failed-start retry hygiene was removed: the supported composition
  starts once, and successful double-start/restart behavior is already covered.
  Reintroduce only if failed-start retry becomes a supported lifecycle.
- Backward clock-step agreement was removed because no product requirement
  justifies weakening the current large-backward-jump safety gate.
- Theme-upload path-agreement coverage was removed as a test criterion, not a
  standalone product or maintenance outcome.
- Cross-build ext4/ccache work was removed as optional local optimization.
- The dynamic web-widget package-catalog synchronization note was removed until
  runtime package installation is actually promoted.
