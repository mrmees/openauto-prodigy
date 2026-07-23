# Session Handoffs

Newest entries first.

---

## 2026-07-23 — PR #33 pre-merge review fixes

**What changed:** two confirmed P3 findings from the PR #33 review were fixed
on the branch. `BluetoothManager::shutdown()` now passes the same signature
strings its ObjectManager connects used, so QtDBus hook matching actually
removes the `InterfacesAdded`/`InterfacesRemoved` subscriptions. The dead
re-announcement branch in `TelephonyClient::onInterfacesAdded` (which called
`adoptAg()` into its own second-AG guard, logging a misleading warning) now
refreshes the selected gateway's cached address directly.

**Review adjudication:** two findings confirmed and fixed (disconnect
signatures, dead adoptAg branch). Two P3s wishlisted without code change:
no-retry on a failed initial `GetManagedObjects`, and the MAC-fallback pairing
prompt name never refreshing from a later snapshot. The removed SCO
Idle-to-Active recovery (head-unit restart mid-call no longer resurrects the
call UI) was reviewed and accepted as the documented phantom-call trade-off.
The stale "reset video decoder state" wishlist entry (shipped in PR #32) was
removed. Nothing dismissed silently.

**Verification:** full local build, explicit `openauto-prodigy` target, and
`ctest --output-on-failure` (all tests) passed, including the isolated
`dbus-run-session` Bluetooth suites.

**Next 1-3 steps:** (1) merge PR #33; (2) pick up the wishlisted BlueZ startup
retry alongside the next Bluetooth-adjacent work.

---

## 2026-07-23 — Bluetooth, HFP, and AVRCP state remediation COMPLETE

**What changed:** Bluetooth adapter, paired-device, connected-device,
first-run, and auto-connect state now comes from one asynchronous/coalesced
BlueZ ObjectManager owner with explicit service and object epochs. The
DisplayYesNo Agent1 surface now distinguishes delayed decisions, immediate
display prompts, cancellation, and unsupported keyboard input. Bluetooth audio
uses asynchronous snapshots and an ordered event journal, retains partial
Device1 names, and clears complete player state at loss. Telephony caches both
phones but publishes one deterministic selected gateway, accepts only its
same-object call/transport evidence, and no longer treats idle SCO as a call;
PhonePlugin teardown disconnects provider callbacks safely.

**Why:** startup and hot-plug paths could block the Qt loop, publish stale
Bluetooth/player state, leave pairing replies actionable across object epochs,
let a second phone replace the selected HFP transport, or synthesize calls from
ambiguous SCO activity. The new boundary owners make each lifetime explicit
without changing wireless AA, HFP roles, ofono policy, frozen API, routing, EQ,
or media-control contracts.

**Status:** COMPLETE on `agent/bt-hfp-avrcp-state-remediation`, based on merged
PR #32. The reviewed aarch64 binary at `1a811fc` was retained behind rollback
snapshot `/var/backups/openauto-prodigy/20260723T203433Z` and deployed. One
application process (PID `293938`) owns responsive IPC and reports
`ALPHA-26-07-15-02-130-g1a811fc`. Startup immediately adopted the connected
Pixel and two paired devices without an auto-connect storm. Pi-side AVRCP drove
the Moto from paused to playing and back; its 257000 ms duration was retained,
position advanced from 16212 ms to 63062 ms, transport became active, and
PipeWire linked the Moto BlueZ source through `openauto-bt-eq-in`. Fresh audible
confirmation was unavailable remotely; the same Moto/EQ path was previously
heard by Matthew. With both telephony gateways present, the Pixel remained the
selected idle gateway. Hostapd PID `46989` and Bluetooth PID `672` stayed
unchanged with zero restarts; no daemon restart, re-pair, call, or Pi checkout
mutation occurred.

**Review gate:** seven finding-bearing passes returned 27 findings; all 27 were
confirmed and fixed, none were dismissed, and none were silently dropped. The
fixes cover snapshot epochs and parse failures, pairing prompt lifetimes,
adapter/device replacement, AVRCP journal ordering and failed-scan fallback,
and gateway/call failover isolation. The terminal repair-only rerun reported
`LGTM — no issues found`.

**Verification:** focused Bluetooth manager, Bluetooth audio, media-status,
serializer, telephony, phone-state, phone-plugin, audio-policy, and SCO tests
passed. `cmake --build . -j$(nproc)`, the explicit `openauto-prodigy` target,
and `ctest --output-on-failure` passed in `~/builds/openauto-prodigy`; an
unrelated audio-ring-buffer sampling flake on the first full run passed ten
consecutive stress reruns and the clean full-suite rerun. Documentation links,
`git diff --check origin/main`, and `./cross-build.sh` passed. Live validation
covered binary identity, exact process/IPC ownership, already-connected state,
Moto AVRCP/A2DP/EQ flow, millisecond progress, two-gateway selection, wireless
AA reconnect, and unchanged service lifetimes.

**Next 1-3 steps:** (1) review and merge the draft PR as an independent wave;
(2) privately select a consolidated next tranche from the remaining work; (3)
publish and approve that tranche's bounded design and plan before execution.

---

## 2026-07-23 — PR #32 pre-merge review fixes

**What changed:** two confirmed findings from the PR #32 review were fixed on
the branch. `AndroidAutoOrchestrator::stop()` now flushes the active socket
after `session_->stop(7)` so the buffered ShutdownRequest reaches the wire
before synchronous transport teardown aborts the socket (no event-loop wait —
the flush is a synchronous call, consistent with the earlier dismissal of a
shutdown-ack wait). `HOST_API_VERSION` was bumped to 3 for the appended
`IHostContext::nightModeService()` vtable entry, and all plugin and fixture
`apiVersion()` overrides now return `PluginDiscovery::HOST_API_VERSION` so a
binary always reports the headers it was built against.

**Review adjudication:** two findings confirmed and fixed (wire flush, version
bump). Observations recorded without action: replacement admission during
`Connecting` is documented design; `ThemeNightMode` is now dead code (cleanup
candidate); the reset-on-failed-`initCodec` silent video path predates this
work. Nothing dismissed silently.

**Verification:** full local build, explicit `openauto-prodigy` target, and
`ctest --output-on-failure` (all tests) passed. The new wire-flush assertion
was proven non-vacuous: disabling the flush fails the test at exactly that
assertion.

**Next 1-3 steps:** (1) merge PR #32; (2) consider removing `ThemeNightMode`
in a cleanup pass; (3) wishlist a surfaced error state for decoder reset
failure.

---

## 2026-07-23 — Android Auto input/video/night remediation COMPLETE

**What changed:** decoder stream resets now run as ordered worker commands and
discard stale queued packets before new codec detection. Evdev touch keeps raw,
locally claimed, and phone-visible ownership separate; serializes complete
MotionEvent transitions; retires contacts on ownership loss; and applies
mapping, grab, reopen, and stop changes at reader-safe boundaries. A shared
application-lifetime night service drives both the shell and AA sensor cache.
Wireless admission rejects extra clients during active/backgrounded projection,
uses one effective port, and derives setup counts from the advertised codec
list. Review fixes also close stale queued-frame delivery, GPIO recovery,
reconnect-wait wakeups, mixed-report touch overlap, and viewport mapping drift.

**Why:** process-long decoder state could poison the next session; cross-thread
evdev mutation and conflated pointer ownership could corrupt phone input;
session-scoped night providers could diverge from the shell; and unauthenticated
or redundant wireless events could replace or destabilize an active session.
The shared boundary owners now make those transitions explicit and testable.

**Status:** COMPLETE on `agent/aa-input-video-night-remediation`, based on
merged PR #31. The reviewed aarch64 binary at `f694e51` was retained behind
rollback snapshot `/var/backups/openauto-prodigy/20260723T154221Z` and deployed.
The final process (PID `272291`) owns responsive IPC. Forced H.264 decoded an
800x480 hardware frame; the restored two-codec configuration decoded H.265 and
then decoded H.265 again after a graceful `SIGUSR1` session boundary without an
application restart. A live extra TCP client was closed without disturbing
projection. Forced day and night runs produced matching shell state and first
NIGHT_DATA indications. The original configuration was restored byte-for-byte,
temporary captures were removed, and the Pi's pre-existing dirty checkout was
preserved. Hostapd PID `46989` and Bluetooth PID `672` remained unchanged with
zero restarts. Physical touch was unavailable during remote validation; live
evdev grab/content dimensions and the product-path touch tests passed.

**Review gate:** the initial pass returned four findings; all were confirmed
and fixed. The required rerun returned five findings: mixed touch replacement
ordering and viewport mapping refresh were confirmed and fixed; three were
dismissed because they requested pre-existing codec expansion, live night-source
reconfiguration, or an event-loop shutdown-ack wait outside or contrary to the
approved boundaries. No finding was silently dropped, and the required rerun
gate is complete.

**Verification:** focused decoder, touch, night, wireless, session, discovery,
and runtime-bridge tests passed. `cmake --build . -j$(nproc)`, the explicit
`openauto-prodigy` target, and `ctest --output-on-failure` passed in
`~/builds/openauto-prodigy`. Documentation links and `git diff --check` passed.
`./cross-build.sh` produced the deployed aarch64 binary. Live validation covered
H.264/H.265 first-frame decode, a same-process session reset, active-client
rejection, day/night shell plus protocol delivery, exact restoration, process
and IPC ownership, and unchanged hostapd/Bluetooth lifetimes.

**Next 1-3 steps:** (1) publish this branch as a draft PR targeting `main`;
(2) review and merge it as an independent wave; (3) revalidate the next bounded
subsystem wave before promoting another design and plan.

---

## 2026-07-23 — Android Auto protocol crypto/flow remediation COMPLETE

**What changed:** OpenSSL setup and established-session I/O now use checked,
transactional results with bounded diagnostics and fail-closed TLS record
handling. Fragmented-message assembly retains FIRST's declared total, enforces
16 MiB per-message and 32 MiB aggregate bounds, validates continuation flags
and exact completion, and surfaces one terminal protocol error. Audio returns
one receive permit per accepted frame, session liveness uses the configured
pong deadline with echoed-timestamp correlation, and navigation remains active
through REROUTING. The implementation is recorded in `fb54c37`, `c6d98aa`,
`a2ee3d7`, review fixes `5392092`, `714e0e5`, and `9ee3200`.

**Why:** unchecked or misclassified OpenSSL failures could crash, hang, or
silently poison a session; malformed fragmented input could grow without a
bound or deliver inconsistent payloads. Audio exhausted the advertised permit
window at every tenth frame, liveness ignored its timeout setting and accepted
uncorrelated pongs, and rerouting incorrectly cleared active guidance.

**Status:** COMPLETE on `agent/aa-protocol-crypto-flow-remediation`, based on
merged PR #30. The reviewed aarch64 binary at `9ee3200` was deployed after the
prior binary was retained at
`/var/backups/openauto-prodigy/20260723T123621Z`. One exact application process
(PID `250566`) owns responsive IPC and reports the deployed version. The Pixel
automatically reconnected, every AA service channel opened, all three audio
channels negotiated PCM, and H.265 hardware projection decoded its first
800x480 frame. No AA media stream was supplied during the bounded check, so
audible media was not exercised. Hostapd PID `46989` and Bluetooth PID `672`
were unchanged with zero restarts. The Pi's pre-existing dirty QML/submodule
state was preserved without pull, reset, clean, daemon restart, or re-pairing.

**Review gate:** the initial pass returned four findings, the required rerun
returned three, and the final candidate pass returned two. All nine were
confirmed and fixed; none were dismissed or left unadjudicated. The fixes cover
handshake-stage failure closure, initial ping ordering, short-message rejection,
gate command chaining, complete TLS-record consumption, raw fragmentation
metadata and size validation, safe liveness configuration, pong correlation,
and a valid-but-mismatched credential test.

**Verification:** focused protocol, cryptor, framing, messenger, session,
audio, navigation, and bridge tests passed. `cmake --build . -j$(nproc)`, the
explicit `openauto-prodigy` target, and `ctest --output-on-failure` passed in
`~/builds/openauto-prodigy`. Documentation links and `git diff --check` passed.
`./cross-build.sh` produced the deployed aarch64 binary. Live validation covered
binary identity, exact process ownership, IPC, wireless discovery and session
establishment, all channel opens, PCM negotiation, H.265 first-frame decode,
deadline stability, and unchanged hostapd/Bluetooth lifetimes.

**Next 1-3 steps:** (1) publish this branch as a draft PR targeting `main`;
(2) review and merge it as an independent wave; (3) revalidate the next bounded
subsystem wave before activating another public plan.

---

## 2026-07-23 — Audio and equalizer real-time safety remediation COMPLETE

**What changed:** `AudioRingBuffer` now uses strict single-producer/
single-consumer cursor ownership and drop-newest short writes. PipeWire
playback and capture validate their containers, chunks, offsets, sizes, and
bounded capacities before access; rejected playback buffers are recycled as
empty; static buffering replaces unreachable adaptive growth; and RT callbacks
publish primitive diagnostics for later Qt-thread logging. Equalizer public
ingress and preset namespaces are validated, restoration is save-free until a
user mutation, and the engine publishes complete coefficient snapshots through
lock-free atomic primitives with one bounded RT read attempt.

**Why:** the previous overflow, malformed-buffer, configuration, preset, and
coefficient-publication boundaries could race, dereference invalid PipeWire
state, allocate or block on the RT thread, index outside fixed state, rewrite
configuration without a user action, or expose a torn filter update. The fixes
keep routing, focus, protocol, API, and QML behavior unchanged.

**Status:** COMPLETE on `agent/audio-eq-rt-safety-remediation`, based on merged
PR #33. The behavior-changing aarch64 binary through `fea943b` was retained
behind rollback snapshot `/var/backups/openauto-prodigy/20260723T224456Z` and
deployed. One application process (PID `306153`) owns responsive IPC and
reports `ALPHA-26-07-15-02-140-gfea943b`. The Pixel automatically resumed
wireless H.265 projection; AA Media, Speech, and System streams and the
Bluetooth EQ graph were present without stream errors or restarts. Audible
A2DP was unavailable while the Pixel occupied Android Auto, so that row is
recorded as unavailable rather than inferred. Hostapd PID `46989` and Bluetooth
PID `672` remained unchanged with zero restarts. The original configuration was
restored byte-for-byte, and the Pi's pre-existing dirty checkout was untouched.

**Review gate:** the first pass returned three findings and the required rerun
returned two. All five were confirmed and fixed; none were dismissed or left
unadjudicated. The fixes cover clean-shutdown persistence, capture bounds,
preset namespace consistency, rejected-buffer metadata, and exact accepted
ring-stream coverage. No second rerun was performed under the one-rerun policy.

**Verification:** focused audio-ring, AudioService, configuration, equalizer
service, and equalizer engine tests passed. The ring stress passed fifty
repetitions and the engine stress passed one hundred. `cmake --build .
-j$(nproc)`, the explicit `openauto-prodigy` target, and `ctest
--output-on-failure` passed in `~/builds/openauto-prodigy`.
`git diff --check origin/main..HEAD` and `./cross-build.sh` passed. Live
validation covered binary identity, exact process/IPC ownership, audio graph
creation, wireless H.265 reconnect, exact configuration restoration, and
unchanged hostapd/Bluetooth lifetimes.

**Next 1-3 steps:** (1) review and merge the draft PR as an independent wave;
(2) revalidate the next consolidated subsystem batch; (3) approve its bounded
design and plan before implementation.

---
