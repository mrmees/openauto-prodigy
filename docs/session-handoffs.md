# Session Handoffs

Newest entries first.

---

## 2026-07-22 — Android Auto session/transport lifecycle remediation COMPLETE

**What changed:** Android Auto sessions now separate graceful phone-visible
shutdown from idempotent terminal finalization, closing persistent handlers and
detaching their send paths before owner teardown or connection replacement.
Messenger start/stop now owns a full framing, assembly, crypto, signal, and send
reset boundary, including cancellation of writes after reentrant teardown.
Fatal TLS handshakes have a typed immediate failure path, both channel-open
delivery paths use one validated service-channel response helper, and wireless
discovery independently retries transient RFCOMM listener and SDP startup
failures within bounded budgets.

**Why:** teardown and replacement could call destroyed handlers, preserve stale
channel state, or transmit through an old/new messenger at the wrong protocol
phase. Restarting a messenger retained partial wire state, fatal TLS input was
misclassified as an incomplete handshake, one duplicate channel-open path could
reply on the wrong channel, and a single boot-time RFCOMM listen failure left
wireless discovery terminally unavailable.

**Status:** COMPLETE on
`agent/aa-session-transport-lifecycle-remediation`, based on merged PR #29.
The reviewed aarch64 binary was deployed after snapshotting the prior binary at
`/var/backups/openauto-prodigy/20260723T030441Z`. One application process owns
responsive IPC. The Pixel automatically rediscovered and reconnected, every AA
service channel opened, and H.265 projection decoded its first 800x480 frame.
Hostapd PID 46989 and Bluetooth PID 672 were unchanged with zero restarts. The
Pi checkout's pre-existing dirty QML/submodule state was preserved without a
pull, reset, clean, or unrelated overwrite.

**Review gate:** the first pass returned three findings and the rerun returned
three deeper reentrancy/ordering findings. All six were confirmed and fixed in
`9939d76` and `caaaffb`; none were dismissed. The final review returned LGTM
with no unadjudicated finding.

**Verification:** focused messenger, session FSM, cryptor, control-channel,
Bluetooth-discovery, and orchestrator tests passed. The full build, explicit
`openauto-prodigy` target, and `ctest --output-on-failure` passed locally.
Documentation links and `git diff --check` passed. `./cross-build.sh` produced
the deployed aarch64 binary. Pi verification covered binary identity, service
state, exact process ownership, IPC status, automatic wireless reconnection,
service-channel establishment, and H.265 first-frame decode.

**Next 1-3 steps:** (1) publish this branch as a draft PR targeting `main`;
(2) review and merge it as an independent wave; (3) revalidate the next bounded
remediation group before activating another public plan.

---

## 2026-07-22 — API/core asynchronous lifecycle remediation COMPLETE

**What changed:** External API pairing now uses a versioned 24-character
Base32 secret with 120 random bits, atomic fail-closed credential persistence,
typed legacy-credential rejection, and matching additive protobuf fields in
Prodigy and Companion. API handshake stages have independent deadlines and
explicit teardown ordering. Clock correction is asynchronous and serialized;
connectivity, EventBus delivery, system-service reconnect, plugin exceptions,
and local IPC framing now have bounded ownership and failure semantics.

**Why:** the former six-digit pairing transcript was cheaply enumerable, and
several independent asynchronous boundaries could block the main thread,
retain stale state, retry incompletely, invoke work after teardown, or accept
unbounded/unframed input. The consolidated wave repaired those roots at their
owners without changing Android Auto, HFP, PipeWire routing, or public API
semantics outside additive secure-pairing negotiation.

**Status:** COMPLETE on `agent/api-core-async-lifecycle-remediation`, based on
the merge of PR #28. The reviewed aarch64 binary and matching Companion APK
were deployed together. Companion storage retired its legacy record, QR
pairing created generation-2 records on both sides, and force-stop/relaunch
reconnected with the saved credential. Live battery, GPS, connectivity, and
SOCKS reports resumed; duplicate steady-state connectivity reports did not
rebuild the route. One application process (PID `189732`) owns responsive IPC,
wireless Android Auto is connected with H.265, and a reversible Moto AVRCP
play/pause probe drove active A2DP through `openauto-bt-eq-in` before restoring
the original paused state. Bluetooth PID `672` plus hostapd PID `46989` are
unchanged. Rollback snapshots are retained at
`/var/backups/openauto-prodigy/20260723T013643Z` and
`/home/matt/backups/openauto-companion/20260723T013643Z`.

**Review gate:** the initial Prodigy review returned five findings and the
permitted rerun returned six; all were confirmed and fixed, with no dismissals
or unadjudicated findings. Fixes completed command-timeout recovery, strict and
atomic credential storage, bounded timezone/IPC work, reconnect framing reset,
and exception-safe EventBus accounting. The Companion review found one Unicode
normalization edge; it was confirmed and fixed, and the full rerun returned
LGTM.

**Verification:** focused API, auth/pairing, clock, publisher, EventBus,
system-service, plugin, IPC, and media tests passed. `cmake --build .
-j$(nproc)`, the explicit `openauto-prodigy` target, and `ctest
--output-on-failure` passed in `~/builds/openauto-prodigy`. Documentation links,
proto byte identity, and `git diff --check` passed; `./cross-build.sh` produced
the deployed aarch64 binary. Companion passed `./gradlew
:app:testDebugUnitTest :app:assembleDebug`. Live validation covered migration,
QR pairing, saved reconnect, reporting/SOCKS stability, split and coalesced IPC,
process and daemon lifetimes, Moto A2DP/EQ routing, and AA continuity.

**Deviations:** review-driven atomic-save, cumulative IPC-budget, strict-store,
timezone-reconciliation, and exception-containment fixes stayed within the
approved boundaries. The Companion branch intentionally carries only the
previously validated airplane-mode recovery change from outside this wave.

**Next 1-3 steps:** (1) publish separate draft PRs for Prodigy and Companion;
(2) review and merge the coordinated pair together; (3) select the next bounded
subsystem wave from the saved remediation map.

---

## 2026-07-22 — Configuration startup contract remediation COMPLETE

**What changed:** built-in YAML defaults now register `logging.verbose` and a
typed `logging.debug_categories` sequence. Startup, settings, and IPC share one
logging-policy boundary; IPC validates complete payloads and canonical category
names before mutation, and persisted values survive restart. The logging
handler now honors selective Android Auto library output independently of
global verbose mode. Known map-shape replacements enter corrupt-config
quarantine, while `DisplayService` applies its first configured brightness even
when that value equals the observable default. The implementation is recorded
in `8a0b406`, `ca32725`, `8efd047`, review fixes `d7f00d5` and `1dd45ed`, with
the active-plan setup in `437d5dc`.

**Why:** logging controls were present in settings and IPC but absent from the
schema, so persistence and category lists were ineffective. A valid YAML
scalar could also replace a built-in mapping and fail later in a typed accessor,
and the default brightness value returned before any backend assignment.

**Status:** COMPLETE on `agent/config-startup-contract-remediation`, based on
`origin/main`; the branch remains unpushed. The reviewed aarch64 binary was
deployed to the Pi. Selective `aa` logging emitted real protocol debug output
live and after restart; verbose logging emitted thread-context output live and
after restart. The final Pi state has one application process (PID `181465`),
responsive IPC, wireless H.265 projection, and a reappeared A2DP transport.
Bluetooth PID `672` and hostapd PID `46989` were unchanged. The original config
is restored byte-for-byte, with quiet/default logging effective. Rollback
snapshot: `/var/backups/openauto-prodigy/20260722T235330Z`.

**Review gate:** the initial review returned four findings; all were confirmed
and fixed. They covered strict IPC payload validation, canonical category
validation, shared restart/settings coverage, and whitespace. The rerun found
one real handler-stage selective-AA suppression defect; it was confirmed and
fixed with actual handler-output coverage. The final review returned LGTM with
no findings. Nothing was dismissed or silently dropped.

**Verification:** focused YAML/config, logging, IPC, and display-service tests
passed. `cmake --build . -j$(nproc)`, the explicit `openauto-prodigy` target,
and `ctest --output-on-failure` passed in `~/builds/openauto-prodigy`.
Documentation links and `git diff --check` passed, and `./cross-build.sh`
produced the deployed aarch64 binary. Live validation covered both logging
modes before and after restart, configured brightness `80` on the Pi's
software-overlay backend, final process/IPC health, reconnects, exact config
restoration, and unchanged daemon lifetimes. Hardware-backlight first-apply
behavior is pinned by the deterministic fake-backend test because this Pi has
no hardware brightness backend.

**Deviations:** review-driven strict category validation and the selective AA
handler fix remained inside the approved logging contract. No QML, protocol,
External API, HFP, ofono, PipeWire routing, or Bluetooth-daemon change was made.

**Next 1-3 steps:** (1) obtain Matthew's push approval; (2) push the branch and
open a standalone draft PR targeting `main`; (3) after merge, select the next
bounded tranche from the consolidated remediation queue.

---

## 2026-07-22 — Bluetooth AVRCP time-unit remediation COMPLETE

**What changed:** BlueZ `MediaPlayer1.Track.Duration` and top-level `Position`
now enter `BtAudioPlugin` as widened milliseconds without the erroneous
divide-by-1000 conversion. Initial player adoption and later
`PropertiesChanged` delivery share the same parser and notification contract;
duration-only Track changes update independently, while invalid time fields,
player replacement, removal, and BlueZ loss clear stale observable state.
`MediaStatusService` receives coherent, duplicate-suppressed progress and a
bounded startup/reconnect snapshot seam. The work is recorded in `3e2d504`,
`89300eb`, review hardening `1af734f`, and catch-up coverage `ffe3f60`.

**Why:** BlueZ already reports AVRCP time in milliseconds, but the ingestion
boundary treated those values as microseconds. A normal multi-minute track was
therefore reduced to a fraction of a second across Bluetooth labels, shared
now-playing state, progress, and the External API; duration could also remain
stale when text metadata did not change.

**Status:** COMPLETE on `agent/bt-avrcp-time-units-remediation`, based on
`origin/main`; the branch remains unpushed. The reviewed aarch64 binary through
`ffe3f60` was deployed after Matthew's approval. Predeployment live evidence
reproduced BlueZ Duration `252000` becoming API `252`; after deployment BlueZ
and API Duration were exactly `131239`, and a pause edge exposed Position
`84556` identically through BlueZ and the API. The Moto A2DP PipeWire node was
running through `openauto-bt-eq-in`; Matthew confirmed audible playback and
correct Bluetooth/shared time labels and progress. One application process
(PID `169509`) owned responsive IPC. Hostapd PID `46989` and Bluetooth PID
`672` were unchanged with zero restarts. The deployment rollback snapshot is
`/var/backups/openauto-prodigy/20260722T214321Z`.

**Review gate:** the initial repository review returned three findings; all
were confirmed and fixed, with no dismissals. The fixes bounded stale-state
and invalidation handling at the same BlueZ player boundary and made the real
QtDBus delivery test mandatory. The permitted single rerun returned one
startup/reconnect catch-up coverage finding; it was confirmed and fixed, with
no dismissal. No finding was left unadjudicated.

**Verification:** focused Bluetooth audio, media-status, and API serializer
tests passed. `cmake --build . -j$(nproc)`, the explicit
`openauto-prodigy` target, and `ctest --output-on-failure` passed in
`~/builds/openauto-prodigy`. Documentation links and `git diff --check`
passed, and `./cross-build.sh` produced the deployed aarch64 binary. Live
validation covered the failing predeploy unit boundary, exact postdeploy
Duration and Position propagation, A2DP/EQ routing, UI/API agreement,
process/IPC health, and unchanged hostapd/Bluetooth lifetimes. The Pi's
pre-existing dirty QML/submodule state was preserved; no pull, reset, clean,
Bluetooth daemon restart, re-pairing, HFP call test, or AA capture occurred.

**Deviations:** none from the approved product scope. Review-driven
stale-state/invalidation hardening and the startup catch-up seam stayed within
the same AVRCP time-state contract; optional second-player and track-change
rows were not required after the approved Moto playback and pause/resume proof.

**Next 1-3 steps:** (1) obtain Matthew's push approval; (2) push this branch
and open a standalone draft PR targeting `main`; (3) after review and merge,
select the next bounded remediation tranche from the private queue.

---

## 2026-07-22 — Android Auto initial night-state delivery remediation COMPLETE

**What changed:** `SensorChannelHandler` now retains the latest authoritative
night value independently of channel and subscription readiness, sends that
snapshot when NIGHT_DATA is subscribed, and preserves it across channel
close/reopen. `AndroidAutoOrchestrator` explicitly seeds the handler before the
AA session starts. Night providers expose whether their current value is valid;
GPIO startup preserves the retained snapshot through invalid reads and emits
its first valid sample even when it equals the provider's default storage
value. Forced active and pre-active session replacement now synchronously
resets old handler/session state before a new messenger can be wired.

**Why:** a provider could know the correct night state before the phone
subscribed, but the handler discarded that update and sent a hardcoded day
value. Android Auto could therefore begin in day presentation at night and
remain there until a later state transition.

**Status:** COMPLETE on `agent/aa-night-initial-state-remediation`, based on
`origin/main`; nothing has been pushed. The final reviewed aarch64 binary was
deployed after Matthew's approval. Forced day and night sessions decoded the
first outgoing NIGHT_DATA indication as false and true respectively, and
Matthew confirmed the matching immediate phone presentation in both cases.
The original Pi config was restored byte-for-byte, temporary captures were
removed, one application process owns responsive IPC, and wireless AA
reconnected with H.265 projection. Hostapd and Bluetooth were not restarted
and retained their original PIDs. The new rollback snapshot is
`/var/backups/openauto-prodigy/20260722T181708Z`; the earlier operations
snapshot remains intact. The Pi checkout's pre-existing dirty QML/submodule
state was preserved without pull, reset, or clean.

**Review gate:** iterative repository review produced six findings; all were
confirmed and fixed, with no dismissals or unadjudicated items. Coverage was
added for active and pre-active replacement, provider seeding, invalid GPIO
startup/recovery, and the first valid default-valued GPIO sample. The final
review returned `LGTM — no issues found`.

**Verification:** focused AA sensor, orchestrator, integration, and night-mode
tests passed. `cmake --build . -j$(nproc)`, the explicit
`openauto-prodigy` target, and `ctest --output-on-failure` passed in
`~/builds/openauto-prodigy`. Documentation links, forbidden-path inspection,
and `git diff --check` passed. `./cross-build.sh` produced the deployed binary.
Pi verification covered decoded protocol captures, phone presentation, exact
config restoration, application process/IPC health, wireless projection, and
unchanged hostapd/Bluetooth PIDs.

**Next 1-3 steps:** (1) obtain Matthew's push approval; (2) push this branch and
open a standalone draft PR targeting `main`; (3) after independent review and
merge, choose the next bounded remediation tranche from the private queue.

---

## 2026-07-22 — Operations deployment reliability remediation COMPLETE

**What changed:** source and prebuilt installs now deploy the same canonical
BlueZ SDP compatibility drop-in. Hostapd integration uses an optional
application `Wants`/`After` drop-in with no `BindsTo` or reverse `PartOf`, and
hostapd owns bounded on-failure recovery. Both install modes provide the same
canonical restart helper and readiness-aware application unit. Forced recovery
stops the unit before exact executable/UID/cgroup legacy-orphan cleanup, while
the application acquires a lock-backed single-instance boundary before
hardware initialization and opens IPC only after dependencies are wired.

**Why:** prebuilt installs could omit wireless-AA discovery prerequisites,
hostapd failures or clean application restarts could tear down the other
service, and the old detached restart path could race systemd into multiple
hardware-owning processes or a stolen IPC socket.

**Status:** COMPLETE on `agent/ops-deploy-p1-remediation`, based on
`origin/main`. The final aarch64 binary, helper, and project-owned unit changes
were deployed to the Pi after approval. Normal and forced helper restarts each
left one systemd `MainPID`, one exact application process, and responsive IPC.
Hostapd and BlueZ remained active on their original PIDs throughout; Bluetooth
and hostapd were not restarted. Wireless Android Auto rediscovered the phone,
reconnected over WiFi, and resumed H.265 projection. The pre-deploy rollback
snapshot remains under `/var/backups/openauto-prodigy/20260722T120501Z`. The
Pi checkout's pre-existing dirty QML/submodule state was preserved; no pull or
reset was performed.

**Review gate:** the initial repository review returned one P1, two P2, and one
P3; all four were confirmed and fixed in `8bf9959`. The required one-time rerun
returned three P2 findings; all were confirmed and fixed in `2ba534d`. The
one-rerun gate is closed with no dismissals or unadjudicated findings.

**Verification:** `bash -n` passed for every changed shell script. The focused
installer, package, real-user-systemd lifecycle, and cross-process IPC tests
passed. `cmake --build . -j$(nproc)`, the explicit `openauto-prodigy` target,
and `ctest --output-on-failure` passed in `~/builds/openauto-prodigy`.
Documentation links and `git diff --check` passed. `./cross-build.sh` produced
the reviewed aarch64 binary. On the Pi, `systemd-analyze verify`, effective-unit
inspection, exact process scans, IPC status requests, service PID checks, and
the live wireless-AA reconnect all passed.

**Next 1-3 steps:** (1) push this branch and open a new draft PR targeting
`main`; (2) review and merge it independently of prior merged branches; (3)
choose the next bounded remediation tranche from the remaining private queue.

---

## 2026-07-22 — Current documentation reconciliation COMPLETE; docs-only publication ready

**What changed:** reconciled present-tense guidance with the shipped C++, QML,
configuration, installer, packaging, and test surfaces. The pass covered
delivery status and wishlist state; architecture/design ownership and
threading; settings, configuration, plugin, widget, and release references;
root and subsystem contributor rules; and development, wireless, reconnect,
debugging, and Pi setup guides. Dated AA research is now labeled as snapshot
evidence, while the indexed runbooks describe current commands and owners.
Unused dnsmasq snapshots were removed. Completed media and Phase F plans were
archived; review restored the full Phase F task record and added only a dated
closure note so its history was not replaced by a retrospective summary.

**Why:** current guidance had accumulated shipped-as-pending status, deleted
names and helpers, wrong configuration and port claims, obsolete transport and
touch descriptions, incomplete setup contracts, and references that no longer
matched implementation. The corrections are subsystem-level so nearby prose
does not contradict the repaired claim.

**Status:** COMPLETE on the dedicated documentation branch. The range is
documentation and contributor guidance only; it contains no executable source,
runtime behavior, build, cross-build, deployment, or Pi change. Runtime defects
discovered while comparing docs to code remain unpromoted wishlist items.
Historical handoff entries were not rewritten, and the runtime follow-up series
remains on its separate branch and pull request.

**Review gate:** the first repository review returned six P2 findings; all were
confirmed and fixed in `6839d1f`. The required rerun returned five P2 findings
and one P3. Each had an actionable component corrected in `cbc6afc`; one mixed
finding also requested restoring a public pointer to controlled internal
material, which was dismissed to preserve the repository's publication
boundary. No finding was left unadjudicated, and the one-rerun gate is closed.

**Verification:** `python3 scripts/check-doc-links.py`,
`python3 tests/test_prebuilt_release_package.py`, targeted current-guidance
searches, and `git diff --check` passed. Corrected claims were compared directly
with their owning C++, QML, defaults, installers, packager, tests, and Git
state. The final path and history checks confirm a docs-only range based on
`origin/main`, independent of the runtime follow-up. No build was run because
the approved plan prohibited executable changes and none occurred.

**Next 1-3 steps:** (1) review the separate docs-only draft pull request; (2)
merge or otherwise sequence the runtime follow-up independently; (3) promote a
new bounded remediation tranche only through the normal wishlist/design loop.

---


> Older entries are archived in `docs/archive/session-handoffs/`.
