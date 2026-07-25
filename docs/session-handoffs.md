# Session Handoffs

Newest entries first.

---

## 2026-07-24 — Web config boot ordering cycle COMPLETE

**What changed:** both source and prebuilt installers now order the web config
service after `network.target` only. The installer contract test renders the
real application, system, and web units; recreates their enabled
`multi-user.target`/`graphical.target` boot graph; and asks systemd to build the
boot transaction so a reintroduced ordering cycle fails. The harness also
checks the web-unit destination and drops privileges when a root-run test suite
invokes systemd's unprivileged-only test mode.

**Why:** the web service was enabled under `multi-user.target` but ordered after
the application, while the application was enabled under and ordered after
`graphical.target`. Because `graphical.target` follows `multi-user.target`, the
four edges formed a boot cycle and systemd deleted the web-service start job.
The Flask server does not need the application at startup; it opens the IPC
socket per request and already reports the app-unavailable case.

**Status:** COMPLETE on local `dev`; included as a separate commit in the
Assistant microphone branch. The corrected unit is deployed on the Pi without
a reboot or application-binary change. `openauto-prodigy-web.service` is
enabled and active on port 8080; its live ordering no longer includes
`openauto-prodigy.service`.

**Review gate:** the concurrent Codex/Fable pass found three unique issues:
root-run systemd test incompatibility, omission of the enabled system service
from the simulated graph, and an unchecked unit destination. All three were
confirmed and fixed. Following the review-policy change, Opus 5 performed the
final single-repository pass and returned no P1/P2. Its three P3 suggestions
were dismissed: the target placement is intentionally an explicit boot
contract rather than derived from the unit under test; Debian Trixie supplies
the `nobody` account used for the privilege drop; and a dedicated `--case boot`
selector would change only test ergonomics.

**Verification:** the focused restart installer contract passed both normally
and with the Python suite launched through `sudo`. The full local build, the
explicit `openauto-prodigy` app target, and `ctest --output-on-failure` passed.
On the Pi, the web unit is enabled and active with one process and zero
restarts; the listener binds `0.0.0.0:8080`, and `/api/status` succeeds locally
and from the development workstation. The application, hostapd, and Bluetooth
retained their original PIDs and zero-restart counts.

**Next 1-3 steps:** (1) publish the combined Assistant microphone and web boot
fix branch as the planned PR; (2) after its next natural reboot, confirm the
web listener is active without a manual start; (3) continue wishlist triage
from the clean merged baseline.

---

## 2026-07-24 — AA Assistant microphone transport COMPLETE

**What changed:** implemented the promoted Android Auto AVInput microphone
path from protocol request through Pi PipeWire capture to timestamped phone
PCM. The handler now reports capture startup honestly, enforces a bounded
phone transmit window, validates acknowledgements, and rejects unsupported
input codecs. The orchestrator owns capture-first teardown; a generation-safe
SPSC bridge keeps all Qt/socket work off the real-time callback, applies the
configured gain, drops stale audio on overflow/window stalls, and recovers at
the live edge. The fast Pi cross-build now keeps object churn in a persistent
Docker volume with serialized, private locks and recoverable ownership.

**Why:** the former AVInput surface advertised microphone capability but never
connected the phone's request to capture. A direct signal connection would
have crossed PipeWire's real-time thread into Qt transport state, while an
unbounded or teardown-unsafe bridge could buffer delayed speech or outlive its
AA session.

**Status:** COMPLETE on local `dev` through `2118430`; not pushed. The reviewed
aarch64 binary (SHA-256 `3feecbfde4b44c4a1a866831c8448d28ff4407cb1da3a73c21f036b31b5565bb`)
is deployed on the Pi. The recoverable pre-feature binary remains at
`~/openauto-prodigy/build/src/openauto-prodigy.rollback-pre-aa-mic-20260724`.

**Review gate:** Codex and Fable ran concurrently through the implementation
iterations. Across the gate, 19 unique actionable findings were confirmed and
fixed; six residual findings were dismissed with explicit rationale (invalid
ACKs remain fail-closed, cross-user cache sharing is unsupported, no test-only
hook enters the RT path, aggregate drop diagnostics are intentionally coarse,
one thread-affinity assert was cosmetic, and a bounded stale-session ACK case
could not create credit). The final Fable pass was deploy-ready with no P1/P2;
its one shared first-use lock race was reproduced and fixed before deployment.

**Verification:** the full local build, explicit `openauto-prodigy` target, and
`ctest --output-on-failure` passed with Qt's headless offscreen backend. The
final fast aarch64 build completed in 93 seconds, left no owned container, and
produced the deployed byte-identical binary. On the Pi, the Pixel completed two
clean Assistant microphone open/close cycles at 16 kHz mono with configured
gain 2.8 and recognized spoken input. No capture/overflow error or leaked mic
stream appeared; wireless H.265 projection remained active. The application,
hostapd, and Bluetooth retained one healthy process each with zero restarts.

**Next 1-3 steps:** (1) push `dev` and open the feature PR with Matthew's
go-ahead; (2) merge after the already-completed Fable gate; (3) select and
re-research the next qualified wishlist item before promotion.

---

## 2026-07-24 — AA Assistant microphone transport promoted

**What changed:** the qualified Assistant microphone item was re-researched
against the current AVInput handler, orchestrator, PipeWire capture service,
audio ring, configuration, tests, recovered protocol evidence, and the
established aasdk/OpenAuto source-channel implementation. It is removed from
the unapproved wishlist and promoted into an ACTIVE design and implementation
plan. Roadmap Now and the documentation index point to that bounded wave.

The design keeps AVInput request/response and transmit permits in the protocol
handler, owns PipeWire capture in the orchestrator, and crosses the real-time
callback through a fixed SPSC PCM bridge drained only on the Qt owner thread.
Immediate capture failure returns response value 1 rather than false success;
valid captures return 0. The phone's `max_unacked`/ACK exchange bounds sends,
configured gain is applied with S16 saturation, and capture-first teardown
prevents callbacks or stale PCM from crossing a session generation.

**Why:** the missing signal connection was real, but connecting it directly
would have called Qt transport state from PipeWire's real-time thread, ignored
phone flow control, claimed success before capture existed, left configured
gain unused, and raced session finalization. The active design resolves those
owners without changing protobuf definitions, playback/focus, HFP/SCO, or any
other wishlist item.

**Status:** ACTIVE as a planning package on local `dev`, grounded at
`975b3ef7`. No runtime code has changed and no implementation has started.
The attached Pi microphone is sufficient for the required live gate. Fable's
initial planning review returned one P2 and two P3 findings; all three were
confirmed and fixed. They pin fail-closed behavior without a capture
controller, the config-schema range/consumer update, and accurate Tier-main
review-gate wording. No finding was dismissed. The focused no-timeout Fable
recheck returned LGTM with no remaining P1, P2, or P3 findings and a
merge-safe verdict.

**Verification:** `git diff --check` and
`python3 scripts/check-doc-links.py` are the documentation gates. Full build,
explicit app-target build, CTest, cross-build, and Pi/Pixel validation belong
to implementation Tasks 2-5 and are not claimed by this planning commit.

**Next 1-3 steps:** (1) commit the reviewed planning package locally; (2) begin
Task 2 only with Matthew's implementation go-ahead; (3) retain Fable as the
final pre-push review after implementation.

---

## 2026-07-24 — Future-item qualifiers and AA next-steps triage

**What changed:** local and remote `dev` were fast-forwarded to the merged PR
#38 commit before the next planning pass. Every wishlist entry now states
current Prodigy/Companion stack fit, hardware needs, and whether promotion
requires normal design, one targeted spike, or research-first feasibility work.
The external `E:\tmp\Next Steps for AA in Prodigy.txt` intake was checked
against the current Prodigy tree, the in-tree protocol definitions, the
recovered display/channel reference, and current official Android for Cars
documentation. The wishlist now contains 24 capabilities, 14 marked long-term,
and all 24 carry all three qualifiers.

The intake produced five net capabilities: CarLocalMedia exposure, native
cluster-lite, AA/native blended UI, AA broadcast-radio exposure, and vehicle
climate/control. Cluster-lite is separated from true projected multi-display;
native FM is separated from uncertain AA radio activation. The former OBD item
now describes a GPS/OBD/CAN/GPIO sensor-provider bridge, and the key-event item
now includes rotary, D-pad, call, Assistant, and navigation input. Existing
audio-policy items absorbed the remaining audio notes. The reusable probe
harness remains a bounded research method inside activation-dependent items,
not a product feature, and already-shipped AA/local media arbitration was not
re-added.

Matthew's clarification pass confirmed CarLocalMedia as research-first;
cluster-lite as long-term with an available USB-display-plus-HDMI test setup;
and blended MAIN-display embedding as long-term/research-first without new
hardware. AA radio remains an experiment that may replace a native FM widget,
backed by an available RTL-SDR Blog V4. USB webcams can prototype an API/
ActionRegistry-driven camera. OBD-II and CAN are available; Companion GPS
loopback and optional PL2303GL GNSS remain sensor experiments. GPIO and device
variability move behind an external backend that registers actions, while an
experimental AA vehicle-control bridge maps semantic controls to that backend.
An attached microphone makes Assistant transport the first likely promotion
candidate.

**Why:** “future” previously mixed software-ready work, hardware-bound work,
and protocol ideas whose phone-side activation is still unknown. The three
qualifiers make those constraints visible before prioritization and prevent a
plausible recovered schema from being mistaken for an implementation-ready
feature.

**Status:** COMPLETE as a documentation-only intake and triage on local `dev`.
No feature was promoted; Matthew selected microphone transport as the first
likely promotion candidate rather than automatically accepting the source
document's full priority order. Fable found two P3 terminology inconsistencies
in the initial qualifier pass; both were
confirmed and fixed. The focused Fable recheck returned LGTM with no remaining
P1, P2, or P3 findings and a merge-safe verdict. Fable clarification review:
LGTM with no P1, P2, or P3 findings and a merge-safe verdict.

**Verification:** each of the 24 capability bullets has exactly one Stack,
Hardware, and Investigation qualifier. `python3 scripts/check-doc-links.py`
and `git diff --check` are the local documentation gates. Build, app-target
build, and CTest are not required because only planning documentation changed.

**Next 1-3 steps:** (1) re-research and bound Assistant microphone transport
for promotion; (2) discuss additional wishlist ideas; (3) promote other
capabilities only when their hardware and investigation gates are understood.

---

## 2026-07-24 — Post-milestone planning cleanup review

**What changed:** the `dev` planning-documentation range received its final
cross-AI review with Claude model Fable. The review confirmed four findings:
the archive index omitted four completed remediation-plan pairs; two retired
config-plan headers still routed residual technical findings to the feature
wishlist; project vision still named the former `claude-dev` environment; and
its bench-hardware list omitted the Pixel 8 used in current validation. The
index, archive routing metadata, and project-vision details were corrected.

**Why:** the cleaned wishlist and evidence backlog are only a dependable
baseline if every entry point routes future work into the right bucket and the
supporting project context matches the current development and validation
environment.

**Status:** COMPLETE as documentation-only review remediation on `dev`. Fable
reported no P1 findings. Of five total findings, four were confirmed and fixed
(one P2 and three P3); one P3 handoff-rotation suggestion was dismissed because
the repository rule rotates the oldest month and all entries remaining here are
from the current month. Interrupted Codex/default-model attempts did not count
as the final gate. The focused Fable recheck returned LGTM with no remaining
P1, P2, or P3 findings and a merge-safe verdict.

**Verification:** `python3 scripts/check-doc-links.py` and `git diff --check`
are the local documentation gates. Build, app-target build, and CTest are not
required because this range changes documentation only.

**Next 1-3 steps:** (1) merge the planning-cleanup PR after checks pass; (2)
discuss genuinely new wishlist ideas; (3) re-research any selected engineering
lead against the current tree before promotion.

---

## 2026-07-24 — ALPHA-26-07-24-01 validation ledger cleared

**What changed:** all three current milestone observations received live
dispositions on the Pi at `192.168.1.149`. Prodigy reached systemd READY about
32 seconds after boot with both wait-online units disabled. The source/prebuilt
policy mismatch became a re-research-required engineering lead. The former
Bluetooth `MediaPlayer1 Track` warning was absent, while three UDisks
MountPoints `QDBusArgument` warnings became a precise local-media lead. After
the connected Pixel 8 disconnected, an unpaired phone discovered
`Prodigy_e57d`; the Bluetooth-advertising observation passed and was deleted.
The stale Pi address in project vision was corrected.

**Why:** current validation observations must resolve into passed evidence or
confirmed technical leads rather than linger as implied work. The deployed
binary is at runtime commit `23e4385`; the 17 later milestone commits contain no
`src`, QML, library, or CMake runtime delta, so the checks exercise the tagged
runtime behavior.

**Status:** COMPLETE. `docs/validation-current.md` has no pending observations.
No runtime or Pi configuration was changed.

**Verification:** live `systemd-analyze`, systemd unit state, application and
BlueZ journals, `bluetoothctl`/D-Bus adapter state, and the unpaired-phone RF
scan were checked. `python3 scripts/check-doc-links.py` and `git diff --check`
pass. Build, app-target build, and CTest were not run because repository changes
are documentation-only.

**Next 1-3 steps:** (1) discuss new product ideas against the cleaned wishlist;
(2) select one current feature or engineering lead; (3) re-research it against
the current tree before promotion and planning.

---

## 2026-07-24 — Feature wishlist group triage

**What changed:** Matthew reviewed all four feature groups item by item. The
wishlist now contains 19 intentional capabilities, ten explicitly marked
long-term. Shipped source arbitration and in-car theme selection were removed;
battery charging presentation, proxy-result acknowledgement, WiFi password
rotation, dashboard-editing polish, and generic widget-size work were declined
or returned to their future owning phase. Native call controls now exclude AA
ownership, the remaining AA System-audio gap is stated precisely, and the
Miata-specific work is framed as reusable vehicle GPIO/power integration.

**Why:** the wishlist should express capabilities Matthew actually wants, not
preserve stale assumptions, already-shipped behavior, or incidental design
practices. Explicit long-term labels keep valid directions visible without
making them look ready for promotion.

**Status:** COMPLETE as documentation-only triage on local `dev`. No item was
promoted, implemented, or pushed.

**Verification:** `python3 scripts/check-doc-links.py` and `git diff --check`
pass. Build, app-target build, and CTest were not run because only Markdown
changed.

**Next 1-3 steps:** (1) discuss genuinely new wishlist ideas against this clean
baseline; (2) run and retire or promote the three current Pi validation checks;
(3) re-research any selected engineering lead before design or execution.

---

## 2026-07-24 — Backlog audit and canonical rebucketing

**What changed:** local `dev` was fast-forwarded to the post-milestone cleanup
commit `bbce99a`, then every retained wishlist entry was checked against the
current code and recent PR #26-#37 remediation history. The result is 27
atomic, user-facing capabilities in `docs/wishlist.md`, 33 evidence-tagged
technical leads in the new `docs/engineering-backlog.md`, and three Pi checks
in the new `docs/validation-current.md`. Six stale, unsupported, or
implementation-criterion entries were closed during the audit. The roadmap's
stale unpromoted Later list was removed, concrete product ideas were preserved
in the wishlist, and the reusable non-Qt protocol direction moved into project
vision.

**Why:** the previous wishlist still made broad reliability practices and old
review findings look like approved deliverables. The new buckets distinguish
product decisions from technical evidence and unconfirmed hardware
observations, while requiring engineering findings to be re-researched against
the current tree before any plan is written.

**Status:** COMPLETE as a documentation-only audit on local `dev`; nothing was
promoted, implemented, pushed, or given execution priority. `dev` remains ahead
of the stale remote branch and includes the local post-release cleanup commit.

**Verification:** `python3 scripts/check-doc-links.py` and `git diff --check`
pass. The local branch is `dev`; the audit was grounded on `bbce99a`. Build,
app-target build, and CTest were not run because only Markdown and repository
workflow guidance changed.

**Next 1-3 steps:** (1) run and retire or promote the three current milestone
validation checks on the Pi; (2) discuss the user-facing wishlist groups and
remove or prioritize candidates; (3) re-research only the selected engineering
lead before converting it into a bounded design and plan.

---

## 2026-07-24 — Wishlist consolidation and stale-plan retirement

**What changed:** the wishlist was reduced from a dated finding log to 35
actionable outcomes grouped by reliability, product/UX, engineering, long-term
architecture, and current-release validation. Completed, superseded,
theoretical, and explicitly dismissed entries were removed; related config,
Bluetooth, call UI, audio, widget, web-config, media, and release items were
consolidated. The 2026-02-21 config-contract design and plan are now ABANDONED
and archived. Architecture now states the modal Dialog versus shell z-band
stacking contract, and live cross-references follow the new wishlist names.

**Why:** milestone ALPHA-26-07-24-01 closed a large remediation campaign, but
the old ledger still mixed shipped fixes and review history with promotable
work. The consolidated file is now a decision surface for the next discussion,
not an excavation log.

**Status:** COMPLETE as a documentation-only cleanup. No wishlist item was
promoted and no runtime behavior changed.

**Verification:** `python3 scripts/check-doc-links.py` and `git diff --check`
pass. Build and CTest were not run because only Markdown files moved or changed.

**Next 1-3 steps:** (1) discuss the 35 retained outcomes; (2) remove, split, or
re-rank them as Matthew decides; (3) only then add new ideas or promote one
bounded scope into a fresh design.

---

## 2026-07-24 — ALPHA-26-07-24-01 milestone release

**What changed:** Matthew declared the completed audit-remediation campaign a
milestone. Annotated tag `ALPHA-26-07-24-01` now points to `d6a92c1`, and the
official Pi 4 aarch64 prerelease contains the tagged binary plus the required
patched mSBC package.

**Why:** PRs #26-#37 completed and live-validated the consolidated reliability
campaign, leaving no active implementation wave. The tag gives that reviewed
state a stable installable release boundary before wishlist promotion resumes.

**Status:** RELEASED at
`https://github.com/mrmees/openauto-prodigy/releases/tag/ALPHA-26-07-24-01`.
The uploaded asset SHA-256 is
`89e32d628e7d745cb0e0ea3635c7a5dc30ac4a3497d980e81351b5865c22dea9`.

**Verification:** local build and explicit `openauto-prodigy` target passed;
`QT_QPA_PLATFORM=offscreen ctest --output-on-failure` passed after the plain
non-interactive invocation exposed the expected missing-X-display condition.
`./cross-build.sh` embedded the exact tag, packaging passed, and the asset
downloaded through GitHub's API matched the local SHA-256.

**Next 1-3 steps:** (1) discuss and prioritize wishlist candidates; (2) promote
only the selected scope into a new bounded design; (3) retain physical
multi-touch, popup-switching, and navbar-boundary checks in the next live drive.

---

## 2026-07-24 — PR #37 review follow-up

**What changed:** the shared installer hardware contract now pins the Bash
locale to `C` while validating both two-letter country codes and hexadecimal
input-property bitmaps. The completed prebuilt WiFi capability, BlueZ SDP, and
preflight work was removed from the wishlist.

**Why:** Bash ERE ranges follow the caller's collation locale. Under
`en_US.UTF-8`, a non-ASCII country-code character could match `[A-Za-z]` and
reach hostapd configuration even though the intended contract is ASCII only.
Keeping the locale boundary inside each parser makes source and prebuilt
install behavior independent of the invoking shell and Pi image locale.

**Status:** COMPLETE on `agent/installer-deployment-lifecycle-remediation`.
Both actionable PR review findings were confirmed and fixed. The three stated
non-actionable nits remain unchanged: installer-internal IPv4 defaults, the
existing passphrase control-character scope, and transaction-directory
tampering after capture.

**Verification:** the hardware-contract test passes under both `C` and
`en_US.UTF-8`, with direct non-ASCII rejection also exercised at the sourced
function boundary. Bash syntax, error-level ShellCheck, focused installer and
release-package tests, the full local build, explicit `openauto-prodigy`
target, `ctest --output-on-failure`, documentation links, and diff checks pass.

**Next 1-3 steps:** (1) push the review follow-up to draft PR #37; (2) let the
reviewer verify the locale case; (3) merge the final wave when approved.

---

## 2026-07-24 — Installer and deployment lifecycle remediation COMPLETE

**What changed:** source mode now resolves and validates the complete checkout
that owns the running installer, rejects stdin or copied-script execution before
mutation, supervises package/build process groups, and preserves the entry
application state across rebuilds. Source and prebuilt modes share hexadecimal
touch parsing, AP-capability/channel selection, strict SSID/country validation,
carrier-independent networking, a canonical preflight, and canonical application
unit rendering. Prebuilt upgrades stage and validate managed payloads before an
unconditional managed-service stop boundary, restore payload/unit/activity and
enablement policy on failure, and require application, system, and web readiness
before retiring rollback material. Startup now conditions on the actual user
PipeWire socket and Wayland socket, while the resolution helper accepts only an
Xvfb child whose PID, lock, socket, and parent ownership agree. The bounded
implementation spans `0fa4f08` through `3e151b3` after documentation activation
commit `f150a65`.

**Why:** checkout assumptions, decimal bitmap parsing, unsupported radio
selection, unmanaged child lifetimes, in-place payload replacement, divergent
startup assets, cross-manager PipeWire ordering, and lock-file PID authority
could leave an install incomplete, run stale code, produce unusable network
configuration, or signal an unrelated process. The new boundaries make those
ownership and readiness decisions explicit without changing application C++,
QML, Android Auto, Bluetooth media, HFP, protobuf/API, or audio behavior.

**Status:** COMPLETE on
`agent/installer-deployment-lifecycle-remediation`, based on merged PR #36. The
reviewed aarch64 application target was built but not deployed because this wave
changes installer, packaged shell assets, tests, and documentation only. The Pi
matrix ran from temporary roots and units; all temporary files and units were
removed. Production remains one responsive application process (PID `380741`)
with zero service restarts, the original configuration hash
`f038a7a3f6f768ac10070e0db36b1cc90afe7d465839cab62a70a6d642344c10`, and the
pre-existing dirty checkout untouched. Hostapd PID `46989` and Bluetooth PID
`672` remained unchanged with zero restarts.

**Review gate:** the initial pass returned five P2 findings and the required
rerun returned five P2 findings plus one P3 finding. All eleven were confirmed
and fixed; none were dismissed or silently dropped. Fixes cover enablement
rollback, systemd-special paths, Wayland races, owned Xvfb readiness, DFS and
6 GHz filtering, AP capability and SSID bounds, real PipeWire coordination, web
readiness, and managed-service stop races. No third pass was run under the
one-rerun policy.

**Verification:** focused source-lifecycle, hardware-contract, prebuilt
transaction/package/selection, operations-systemd, and resolution-validation
tests passed with Bash syntax and error-level ShellCheck. The full local build,
explicit `openauto-prodigy` target, `ctest --output-on-failure`, documentation
link/private-reference checks, and `git diff --check origin/main..HEAD` passed.
`./cross-build.sh` produced the aarch64 target. On the Pi, the shared parser
identified the sole direct touchscreen and selected live wlan0 channel 36 with
VHT while the synthetic DFS-only case fell back to 2.4 GHz channel 6. Temporary
prebuilt success, every forced rollback boundary, exact service activity and
enablement restoration, web readiness failure, and real user-systemd
Wayland/PipeWire absent/present conditions passed.

**Next 1-3 steps:** (1) publish this completed branch as a draft PR targeting
`main`; (2) review and merge it as the independent final consolidated wave;
(3) promote future work from the wishlist under a new approved plan.

---

## 2026-07-24 — PR #36 pre-merge review

**What changed:** nothing — the review confirmed the branch as-is.

**Review adjudication:** zero findings. Verified in depth: scanner stop
correctness (generation plus worker-pointer identity reject the stale queued
completion without dereferencing the joined worker; destructor path is
emission-free; FFmpeg interrupt_callback bounds the join even mid-read on a
dying device; avformat_open_input's documented free-and-null-on-failure makes
the RAII close safe); restore-ownership completeness (the loosened
retryPendingRestore gate cannot clobber active playback because every
user-initiated playback path — transport keys, seek, and all queue
selections — clears the pending restore, and the fallback restore is always
paused); eject ordering (duplicate guard, scanner quiesced before the UDisks
request, successor scan excludes canonical aliases of the target); and the
QSKIP-to-hard-failure conversion in playback fixture tests, which is correct
because the WSL dev environment is contractually the Pi target environment.
Observation without action: purgeVolume now quiesces the scanner on yank
paths too, costing a survivor-rescan restart in exchange for guaranteed
file-handle release.

**Verification:** full local build, explicit `openauto-prodigy` target, and
`ctest --output-on-failure` (all tests) passed on the branch.

**Next 1-3 steps:** (1) merge PR #36; (2) at the next milestone tag, deploy
the accumulated PR #32-#36 fixes together.

---

## 2026-07-24 — Media lifecycle and persistence remediation COMPLETE

**What changed:** exact saved tracks now exclusively own their saved position;
an available fallback starts at zero while the raw exact restore remains
pending. Explicit transport and seek actions durably take ownership, and
shuffle, repeat, and clamped seek values persist at their user boundary.
`MediaScanner` now has explicit generation-safe quiescence, cooperative
traversal/cache/tag/artwork cancellation, and an FFmpeg interrupt boundary.
Safe eject excludes canonical target aliases, suppresses duplicate requests,
waits for scanner ownership to end, and rebuilds only surviving roots;
shutdown observes the same ownership order. Repository-owned valid fixtures
fail on backend errors, and scanner completion failures cannot access absent or
expired state. The bounded implementation is recorded in commits `65487de`
through `23e4385` after the activating documentation commit `f20ecd8`.

**Why:** restore state, user intent, scanner workers, removable-media lifetime,
and test failure paths previously had overlapping or implicit ownership. The
new boundaries keep fallback state honest, prevent late restore takeover after
user action, release files before eject/shutdown, and ensure test failures stay
visible without changing QML, media consumer units, playback arbitration,
Bluetooth/HFP, Android Auto protocol, frozen API fields, or installer behavior.

**Status:** COMPLETE on
`agent/media-lifecycle-persistence-remediation`, based on merged PR #35. The
reviewed aarch64 binary through `23e4385` is retained behind rollback snapshot
`/var/backups/openauto-prodigy/20260724T115246Z` and deployed as
`ALPHA-26-07-15-02-163-g23e4385`. One application process owns responsive IPC
with zero service restarts. The exact original media configuration was restored
byte-for-byte; the original cache directories were restored; temporary screen
captures were removed; and the Pi checkout's unrelated dirty state was not
pulled, reset, cleaned, or overwritten.

**Review gate:** the initial pass returned one P1 and three P2 findings; all
four actionable issues were confirmed and fixed. The required rerun returned
six P2 findings, all confirmed and fixed. The only dismissed residual was the
precisely bounded possibility that one indivisible kernel filesystem syscall
finishes before the next cancellation checkpoint; forced thread termination
and custom FFmpeg I/O are outside the approved contract. No finding was
silently dropped, and no third pass was run under the one-rerun policy.

**Verification:** focused media-player plugin, playback-policy, scanner,
USB-policy, tag-reader, and playback-engine tests passed. The full local build,
explicit `openauto-prodigy` target, `ctest --output-on-failure`, documentation
link/private-reference checks, and `git diff --check origin/main..HEAD` passed.
`./cross-build.sh` produced the deployed aarch64 binary. On the Pi, an exact
saved track restored paused at its saved time, a missing-current fallback
started at zero, the late USB source reclaimed the exact pending track, and a
user next/pause takeover remained durable. Shuffle and repeat persisted
immediately and survived restart. Restart during a cold multi-root scan
returned one coherent successor process; safe eject during a cold rescan
removed and powered off only the target, preserved the surviving library, and
remounted cleanly after physical reinsertion. Local playback created the
expected EQ-routed PipeWire stream and updated shared now-playing state.
Wireless H.265 projection reconnected after deployment before the native-media
matrix; the Pixel was then deliberately disconnected for that matrix and was
offline at final reconnect, while the Moto remained connected. Hostapd PID
`46989` and Bluetooth PID `672` remained unchanged with zero restarts.

**Next 1-3 steps:** (1) publish this completed branch as a draft PR targeting
`main`; (2) review and merge it as an independent consolidated wave; (3)
revalidate the final installer/deployment lifecycle batch before activating
its bounded design and plan.

---

## 2026-07-24 — PR #35 pre-merge review

**What changed:** nothing — the review confirmed the branch as-is.

**Review adjudication:** zero findings. Verified in depth: the navbar/popup
generation plumbing (every begin synchronously unregisters old zones, evdev
callbacks capture their generation and the marshaled slot rejects mismatches,
so retired geometry cannot fire actions); the deferred plugin-view retirement
ordering (plugin hook deactivates synchronously, the child QQmlContext
outlives the dying view, `~PluginModel` drains retirements while its child
view host is still alive, and drained IDs make leftover queued callbacks
no-ops); widget-grid invariants (every mutator applies the pending remap
first, baselines commit only on user mutations with all dashboards forced to
unified grid dimensions before persisting); and the per-zone/per-slot popup
press state replacing a lambda-static shared across all zones and fingers.
Observations without action: one event-loop turn of old/new plugin view
overlap on switch (new renders on top; retained context keeps the dying
view's handlers valid by design).

**Verification:** full local build, explicit `openauto-prodigy` target, and
`ctest --output-on-failure` (all tests) passed on the branch. Physical
multi-touch, popup switching, and exact navbar-boundary touches remain
unvalidated on hardware — worth a deliberate poke on the next live drive.

**Next 1-3 steps:** (1) merge PR #35; (2) at the next milestone tag, deploy
PR #32-#35 together and physically exercise navbar edges and popup switching.

---

## 2026-07-24 — UI input and widget-grid lifecycle remediation COMPLETE

**What changed:** navbar evdev claims now come from rendered QML geometry, with
generation-safe replacement, accepted touch-slot ownership, and popup state
isolated by generation, zone, and slot. Widget remaps keep retained placements
and reserved pages reachable, apply pending dimension changes before edits,
and persist page and dimension baselines across restarts. Plugin deactivation
now precedes replacement activation while the outgoing QML view and child
context survive the current dispatch and retire in order afterward. A dedicated
owner replaces window/current-screen DPI connections. The bounded implementation
and review fixes are recorded in commits `6f28137` through `5feaa5a`.

**Why:** fixed geometry, shared or stale gesture state, unreachable widget
placements, stale remap baselines, synchronous QML destruction, late plugin
deactivation, and accumulating screen connections could make shell input,
dashboard state, or plugin transitions disagree with their rendered and
persisted owners. The remediation establishes one explicit owner at each
boundary without changing Android Auto protocol/input mapping, frozen API or
YAML fields, audio, HFP, or Bluetooth behavior.

**Status:** COMPLETE on `agent/ui-input-widget-grid-lifecycle-remediation`,
based on merged PR #34. The reviewed aarch64 binary through `5feaa5a` was
retained behind rollback snapshot
`/var/backups/openauto-prodigy/20260724T032627Z` and deployed as
`ALPHA-26-07-15-02-154-g5feaa5a`. One application process (PID `332025`) owns
responsive IPC. A native dashboard screenshot and live dashboard transition
were verified through the trusted localhost API before Android Auto re-entered
wireless H.265 projection. The exact original configuration was restored
(`57415b...`); hostapd PID `46989` and Bluetooth PID `672` remained
unchanged with zero restarts. Physical multi-touch, popup switching, and navbar
boundary rows were unavailable remotely and were not inferred.

**Review gate:** the initial repository pass returned three findings and the
required rerun returned three more. All six were confirmed and fixed; none were
dismissed or silently dropped. The fixes cover stale navbar geometry events and
generation rollback, widget baseline persistence, and ordered plugin view,
deactivation, and context retirement. No third run was performed under the
one-rerun policy.

**Verification:** focused navbar, widget-grid/remap, plugin-model/view-host, and
display tests passed. `cmake --build . -j$(nproc)`, the explicit
`openauto-prodigy` target, and the full `ctest --output-on-failure` passed in
`~/builds/openauto-prodigy`. Documentation-link validation and
`git diff --check origin/main..HEAD` passed. `./cross-build.sh` produced the
deployed aarch64 binary. Live validation covered shell/dashboard rendering and
transition, one responsive process and IPC owner, wireless H.265 re-entry,
exact configuration restoration, and unchanged hostapd/Bluetooth lifetimes.

**Next 1-3 steps:** (1) publish this completed branch as a draft PR targeting
`main`; (2) review and merge it as an independent consolidated wave; (3)
revalidate the next audit wave before activating another bounded design and
plan.

---
