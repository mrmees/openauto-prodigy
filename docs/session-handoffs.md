# Session Handoffs

Newest entries first.

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

## 2026-07-21 — Post-merge video/SCO review follow-up COMPLETE

**What changed:** added three bounded follow-ups after the memory and teardown
safety tranche merged. `22a5ecb` caps the video-frame pool's retained free list
at its configured pool size. `5b2e5d8` makes monitor shutdown publish the SCO
falling edge, and review fix `4a255d7` preserves that edge across both queued
signal delivery and connect-then-snapshot ordering while keeping repeated stop
idempotent. Focused regressions cover the pool bound and both SCO orderings.

**Why:** burst allocation could leave more decoded-frame buffers retained than
the configured pool size, and a runtime monitor stop could otherwise leave a
consumer with stale call-audio state when a queued edge was invalidated during
teardown.

**Status:** COMPLETE locally and ready for a standalone follow-up PR. This work
is not part of merged PR #22, has not been deployed to the Pi, and does not
change protocol, HFP role, routing, codec, or public API behavior.

**Review gate:** `bash scripts/codex-review.sh origin/main` reviewed
`1157de0..4a255d7` and returned LGTM with 0 P1, 0 P2, and 0 P3. An earlier
review identified the queued-edge ordering gap; it was confirmed, expanded to
cover the complementary snapshot ordering, and fixed in `4a255d7` before the
clean re-run. Verdict:
`reviews/2026-07-21-211105-codex-review.md` (gitignored).

**Verification:** `cmake --build . -j$(nproc)`,
`cmake --build . --target openauto-prodigy -j$(nproc)`, and
`ctest --output-on-failure` passed in `~/builds/openauto-prodigy`; the focused
SCO test also passed repeatedly; `git diff --check` passed.

**Next 1-3 steps:** (1) push `dev` and open the standalone follow-up PR; (2)
keep the documentation-drift remediation on a branch based from `origin/main`;
(3) deploy only after the follow-up is reviewed and merged.

---

## 2026-07-21 — Memory and teardown safety tranche COMPLETE: local, ARM, Pi, and review gates passed

**What changed:** completed the approved three-task safety tranche. `c6a5a02`
makes software video-frame reuse generation/capacity aware through weak shared
return state and explicitly releases the decoder's latest frame before pool
reset. `57256d3` protects the WeatherData returned during cache cleanup, treats
capacity as soft while entries are subscribed, retries cleanup on unsubscribe,
and carries `QPointer` targets through weather and geocoding completions.
`2b756e8` adds a direct AudioService pre-PipeWire-teardown stop edge for
ScoNodeMonitor, normalizes partial/repeated stop, and epoch-guards queued state
delivery. Review fix `d372b93` rolls back registry proxy/state when PipeWire
listener registration fails instead of leaving the monitor falsely active.

**Why:** the three defects could recycle undersized video allocations, update or
return deleted weather objects, or let an auxiliary monitor touch PipeWire
resources after their owner destroyed them. The batch stayed inside the approved
root-cause boundaries; no protocol, HFP-role, telephony, routing, codec, cache
redesign, or unrelated teardown behavior changed.

**Status:** COMPLETE and deployed; not pushed. Pi row S1 passed five clean
service restarts with a new PID each time, `ActiveState=active`,
`SubState=running`, `NRestarts=0`, and no post-deploy teardown errors. Pi row S2
passed with live running mSBC SCO source/sink nodes: restart during the active
call cleanly replaced the process, mSBC recovered, and a subsequent call again
produced running SCO endpoints. The final reviewed binary also passed a clean
sanity restart (`MainPID=47013`, `NRestarts=0`).

**Review gate:** `bash scripts/codex-review.sh ade13b4` returned 0 P1, 1 P2,
and 2 P3. The P2 was confirmed and fixed in `d372b93` (listener-registration
failure rollback). Both P3s were dismissed as test-depth suggestions rather
than product defects: the SCO owner edge received live Pi coverage with real
PipeWire/SCO resources, and Weather separately proves real cache eviction plus
guarded null-target completion while the production callback captures the
`QPointer` by value. The confirmed change was small and focused, so no review
re-run was required by the gate policy. Verdict:
`reviews/2026-07-21-150330-codex-review.md` (gitignored).

**Verification:** `cmake --build . -j$(nproc)`,
`cmake --build . --target openauto-prodigy -j$(nproc)`, and
`ctest --output-on-failure` passed in `~/builds/openauto-prodigy`; focused video,
weather, and SCO targets passed; `./cross-build.sh` passed at final head; the
final aarch64 binary was rsynced to the Pi and restarted healthy. Plan and design
marked COMPLETED and archived in this commit.

**Next 1-3 steps:** (1) push `dev` only on Matthew's go-ahead; (2) open the
dev-to-main PR and run its normal checks; (3) select the next bounded audit
tranche rather than expanding this completed batch.

---

## 2026-07-15 — Bench-findings batch COMPLETE: Phase A Stage A shipped (SCO un-hijacked, probe-verified) + ALL bench rows PASS; push/tag pending Matthew

**What changed:** Phase A executed at the bench and the whole batch bench-validated. Task 1 marker probe (Pi, live): `api.bluez5.profile` IS visible at monitor-rule eval — A2DP node fired the `a2dp-source` marker, SCO fired only the HFP marker, and the shipped rule was observed hijacking a live call (SCO downlink `target.object = openauto-bt-eq-in`, caller's voice mixing through the tap with music). **Stage A GO** → `f354003`: positive `a2dp-source` match added to `config/50-openauto-bt-eq.conf` + `docs/architecture.md` corrected; deployed + verified. Task 2B skipped. Phase B binary deployed; live unit sed'd (disconnect hook gone) + daemon-reload; stale S25 bond removed from the HU.

**Bench rows (ALL PASS):** SCO calls incoming AND outgoing route direct (`target.object` null on both SCO nodes mid-call), voice both ways — uplink proven on the HU mic (phone mic covered) — rocker volume works, Moto music stayed tap-routed throughout and post-hangup (brief skips during simultaneous SCO+A2DP = single-radio contention, not a defect). Clean app restart while streaming: **both phones stayed connected** (first non-kicking deploy ever). Input device: HU selection → `audio.microphone.device` on disk (dead key absent) → restart → picker + live value restored (IPC-verified). Volume: rapid drag → debounced final-value write (45); restart → restored; set-55-via-IPC + restart <2 s → **55 survived** (aboutToQuit flush proven). Mute-at-0 rides the same mechanism (boot-at-0 silence bench-proven at EQ ship).

**Field amendments + intel:** (1) item-1 acceptance amended — the phone auto-pauses on clean app stop (graceful teardown releases the transport); Matthew accepted as correct behavior (matches his AVRCP-pause preference); pause/resume returns through the EQ. (2) Pixel-in-AA with HFP down routes call downlink over the AA link and uses the PHONE mic for uplink — gearhead degrades gracefully around our unimplemented AVInput (intel for that wishlist item). (3) Pi user journald not persistent (wishlisted). (4) A wireplumber restart tears down HFP registration; the phone re-attaches on its next BT toggle — remember when benching calls after stack restarts.

**Wishlist flips:** ExecStopPost, input-device, master-volume → SHIPPED 2026-07-15. Plan + design → COMPLETED + archived (this commit). Roadmap entry → COMPLETE, refs repointed to archive.

**Verification:** all claims grounded in live SSH/pw-dump/busctl/IPC evidence in-session (commands + outputs in the conversation); suite 124 + app target green @ `2f37839` (controller-run); Task 2A is a conf+docs change post-gate (the exact fix was twice Codex-reviewed at spec level; gate closed on the code range per one-re-run rule).

**Next 1-3 steps:** (1) push dev on Matthew's go; (2) dev→main PR; (3) tag + prerelease ONLY on Matthew's declaration (`bash scripts/tag-alpha.sh` → reconfigure → cross-build → package → `gh release create --prerelease`). Top wishlist candidates after: pairing-window UI fix, AA-assistant mic transport (AVInput, `Tier: main`).

## 2026-07-15 — Bench-findings batch: spec+plan (3x Codex-reviewed) + Phase B COMPLETE (6-task SDD, final review, gate closed); Phase A SCO probe + bench pending Matthew

**What changed:** the full promote→design→plan→execute cycle for the bench-findings batch (design + plan in `docs/plans/2026-07-15-bench-findings-batch-{design,plan}.md`). Scope: items 1–3 promoted by Matthew (ExecStopPost removal — decision: remove entirely; input-device persistence; master-volume persistence) + items 4–6 folded in from Codex's post-merge PR#20 review (HFP-SCO/EQ-tap hijack — **live call-audio regression on the deployed Pi**; plugin ABI runtime gate; roadmap staleness). Spec Codex-reviewed twice (2P1/4P2/1P3 then 1P1/3P2/2P3 — ALL accepted, zero dismissals), plan once (5P1/5P2 — all accepted). Phase B (Tasks 3–8) executed via SDD, commits `8699bec..2f37839` (9): install.sh hook deleted; all input-device callers on canonical `audio.microphone.device` (root cause was split-key + setValueByPath schema rejection); `masterVolumeChanged` emit outside locks + only-on-change (deadlock regression test); single debounced volume writer in main.cpp w/ aboutToQuit flush, slider converted to live-value view, IPC single-writer + persistence failures surfaced (`ok:false`); PluginLoader owning LoadResult + PreventUnloadHint cleared pre-instance() + exception-safe binary metadata, 4 compiled fixtures (stale/imposter/valid/throwing); roadmap Now→Done + refs repointed.

**Quality gates:** zero task-review fix loops across all 6 tasks. Final whole-branch review (fable): READY, 0 Critical/Important, 4 minors fixed same day (`9b93e14`), carried minors deferred with reasons. Codex gate round 1: 0P1/2P2/3P3 — 3 confirmed+fixed (`67d18b5`: exception-safe metadata [red = whole-binary std::terminate abort], picker stale-index always-assign, slider release resync), 2 dismissed (unbounded retry = wishlisted design tradeoff matching shipped EQ pattern; extract-component = deliberate design decision, bench row covers). Re-run: P1 = SCO rule still live — ACKNOWLEDGED, that IS Phase A, push blocked until call rows pass; 1 P2 confirmed+fixed (`2f37839` IPC silent-ok); 2 dismissals stand. Gate CLOSED (one re-run max).

**Discoveries this cycle:** AA assistant mic transport is UNIMPLEMENTED (`micCaptureRequested` has no consumer — wishlisted, `Tier: main`, own cycle; the "input-device fix unblocks the assistant mic" premise was necessary-but-not-sufficient); Qt 6 sets `QLibrary::PreventUnloadHint` by default (unload-on-reject silently no-ops without clearing it); the settings Master Volume slider was a second racing persist path.

**Verification:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc) && ctest --output-on-failure` (124 pass) + `cmake --build . --target openauto-prodigy -j$(nproc)` — controller-ran, green @ `2f37839`. NOT pushed; NOT deployed; NOT tagged.

**Next 1-3 steps:** (1) **Phase A** (Matthew + any phone at the bench): Task 1 marker-rule probe (~10 min, plan has the runbook) → Task 2A one-line conf fix or Task 2B app-side retarget; until then the Pi likely has silent call downlink whenever the app runs (interim: remove the conf + restart wireplumber if calls needed). (2) Ship ceremony: cross-build → deploy (incl. live-unit `sed` removing the disconnect hook + daemon-reload) → bench rows (call rows, restart-stays-connected, input-device persist, volume persist incl. <2 s restart flush, S25 bond cleanup). (3) Wishlist flips + plan/design archive + push on Matthew's go (tag only on declaration).

## 2026-07-15 — BT A2DP EQ bench COMPLETE: 7/7 runbook rows PASS; automated-ops BT funk root-caused (unit ExecStopPost hook); feature SHIPPED

**What changed (docs only):** completed the bench runbook from the 2026-07-15 partial (rows: boot-muted volume, focus BT↔local + speech duck, BT↔AA two-phone takeover, formal fallback/relink, idle economics, 10-min soak + pause/resume hammer, AA regression — ALL PASS). Wishlist: 3 SHIPPING→SHIPPED flips (BT-A2DP-bypasses-EQ, EQ-persistence, Phone→System relabel) + 6 new bench findings; plan+design flipped COMPLETED and archived.

**Row highlights:** volume-at-creation proven (silence at vol 0 while genuinely streaming — verified via graph — then smooth fade-in). Focus matrix green in all directions, last-started wins, loser ducks in-process and keeps playing. Fable-review relink row answered: **app start during live BT streaming does NOT relink the stream into the tap** — stays direct (un-EQ'd) until a transport cycle; fresh nodes/reconnects tap-route from birth (wishlisted sweep-on-bring-up). Idle economics: transport idle → BT Audio + tap idle, feature idle cost ≈ 0 (lost in AA-projection noise, ~19-23% either way). Soak (plain **aptX** — the Moto has no HD; earlier "aptX HD" notes were a vendor-codec-255 misread): 10 min zero disconnects, zero errors, ONE startup xrun total; hammer (~12 pause/resume cycles) clean — **epoch-quiesce dismissal stands, no promotion**. AA regression: media EQ'd (audible preset swap), nav-prompt duck+restore intact.

**Automated-ops BT funk ROOT-CAUSED (bench-opened investigation, closed same day):** `install.sh:1723` ships `ExecStopPost=[ "$SERVICE_RESULT" = "success" ] && bluetoothctl disconnect` — every clean stop/restart (= every deploy) deliberately kicks the phone; crash paths skip it (why 2026-07-14's "fallback held through every stop/error" vs the clean-stop kill at this bench). Stacked on top that day: the 120 s pairing re-arm (known) and a **degraded Moto BT stack** — btmon forensics: sniff-transition failures (LMP Response Timeout 0x22 / Connection Timeout 0x08 immediately after sniff→active), zombie profile-less ACL connects correctly reaped by bluetoothd's temp window, L2CAP `Invalid CID`, transport held "active" while paused 10+ min, reconnect stalls until the phone's BT settings screen opened. Phone reboot cured all of it; HU-side re-page recovery worked every time. Wishlist now carries the ExecStopPost DESIGN DECISION (remove vs scope-to-shutdown).

**Other new findings (all wishlisted):** input-device selection never persists (live only while the settings screen is open; pre-existing — blocks AA-assistant mic config); master volume not flushed to disk (restart reloads stale value); AA touch clicks open the System channel for ~5 s and focus full-ducks media the whole window (partial-duck/mix-over fix shape); AVRCP-pause-on-focus-loss preference (Matthew); tap-stays-down-after-stream-error folded into the existing PipeWire-daemon-restart item.

**Verification:** all bench claims grounded in live SSH/btmon/pw-dump/journal evidence in-session; docs-only change set (no build ceremony per convention). Bench technique that worked again: one Matthew-step at a time, controller runs every check; instrument-first (btmon) before theorizing.

**Next 1-3 steps:** (1) push dev on Matthew's go; (2) dev→main PR; (3) `bash scripts/tag-alpha.sh` → reconfigure → cross-build → package → `gh release create --prerelease` (milestone declared 2026-07-14). After: pairing-window UI fix and/or ExecStopPost decision are the top wishlist candidates.

---

## 2026-07-15 — BT A2DP EQ bench PARTIAL: core path PROVEN end-to-end (rule fix required); paused mid-runbook, pickup file written

**What changed:** first live bench of the BT A2DP EQ tap (deployed `ALPHA-26-07-14-01-33-g755f162` + WirePlumber conf; full ordered stack restart). **Headline: the feature works** — Moto G Play (aptX HD) routes phone → `openauto-bt-eq-in` → ring → "BT Audio" playback → EQ → sink fully automatically; preset swaps audibly change BT music (the F2 verify line, satisfied for the first time); stereo sane; HU master volume now controls BT level (row part A).

**One field fix (committed `9077e17`):** the conf's `media.class = "Stream/Output/Audio"` match line prevented the rule from EVER firing — the property is not set when monitor rules evaluate (bluez node fell back direct-to-sink, `target.object` unset). Diagnosed by manual `pw-link` routing (which proved the entire tap chain worked and isolated the fault to the rule), fixed by matching `node.name = "~bluez_input.*"` alone. The class gate was a spec-review suggestion that didn't survive contact with monitor-rule evaluation order. Deployed conf and repo conf now match.

**Pre-bench pairing saga (~45 min, three stacked causes getting the Moto bonded):** (1) stale half-bond HU-side from the HFP bench era — removed; (2) kernel `bondable` flag desynced from bluetoothd's `Pairable: yes` (btmon: `IO Capability Request Negative Reply — Pairing Not Allowed (0x18)` before the agent is consulted); (3) the real mechanism — **`PairableTimeout=120` is deliberate security design** (2026-02-27 bluetooth-cleanup design; Codex-verified via git archaeology) that the "Accept New Pairings" switch misrepresents as persistent, plus a `pairableChanged`-not-emitted cache bug making the switch show stale-on. Full verdict + fix shape in wishlist § "From BT A2DP EQ bench (2026-07-15)". Temporary `PairableTimeout=0` bench override self-healed via app restart (adapter setup re-arms it).

**New observations for next session:** (a) the tap does NOT restart after a stream error until app restart — observed live when a wireplumber-only restart errored the tap's streams (capture-first teardown fired correctly, fallback held, music never stopped — but the tap stayed down; same family as the wishlisted "app PipeWire connection doesn't survive daemon restart"); (b) **Matthew observed funky BT pairing behavior correlated with automated SSH/restart ops — works, but needs a closer look** (uncharacterized; note the RegisterProfile restart race + pairing-window interactions as suspects).

**Bench rows:** PASSED — Phase 0 pw-dump gate (`media.class` present on settled node, no dont-fallback; aptX HD), deploy + tap startup, graph routing + NO-mic-link assertion, audible preset swap + stereo, HU-volume-over-BT (part A). Fallback held audibly through every app stop/error during the session (row 6 de-facto observed, formal pass pending). REMAINING (pickup file `personal/openautopro/next-session-prompt-bt-eq-bench.md`): boot-muted volume (row was armed: HU volume left at 0, app freshly restarted — resume there), focus rows (BT↔local, speech duck, BT↔AA two-phone), formal fallback/relink rows, idle economics, long soak + pause/resume hammer (wishlist promote-trigger), AA regression.

**Next 1-3 steps:** (1) resume the runbook at boot-muted volume; (2) after all rows: wishlist SHIPPING→SHIPPED flips, archive plan+design, push on Matthew's go, dev→main PR + milestone tag (declared 2026-07-14); (3) investigate the pairing-ops interaction (b) + consider promoting the pairing-window UI fix.

---

## 2026-07-15 — BT A2DP EQ + hygiene riders CODE-COMPLETE: 8-task SDD run + fable whole-branch review + two Codex gate rounds, all adjudicated; bench + push pending Matthew

**What changed:** executed `docs/plans/2026-07-14-bt-a2dp-eq-plan.md` end-to-end (design: same-date `-design.md`, twice Codex-reviewed pre-execution). Eight tiered SDD tasks (T1-T7 opus, T8 sonnet), **every task review Approved with zero fix loops**: (1) AudioService options factories + per-handle capture + volume-at-creation + rate-match opt-out + activity/ring primitives; (2) focus recency — `(priority, sequence)` dominant selection, music unified at 50 (local media's 51 tie-break hack retired); (3) per-consumer EQ engine fan-out via `acquireEngine`/`releaseEngine` (kills the shared-Media-engine RT defect), authoritative bypass, NaN boundary; (4) durable persistence — gains/bypass round-trip to disk, validation at every ingress, parent-dir fsync, surfaced+retried flush; (5) `StreamId::Phone`→`System` (value 2, deprecated alias) + raw-YAML pre-merge config migration + QML relabel; (6) `transportActiveChanged(bool)` edge from `MediaTransport1.State=="active"` (interface presence ≠ activity; four forced-false paths); (7) `BtTapController` (pure state machine, 6 order-asserting specs) + `BtAudioTap` — capture-last bring-up, capture-first teardown, transport-edge activity + Gain focus; (8) `config/50-openauto-bt-eq.conf` WirePlumber retarget (media.class-gated, fallback preserved) + both installers + `src/AGENTS.md` requested-period rule correction + docs.

**Review chain after the tasks:** fable whole-branch review → 1 Critical (plan contradiction: Task 4's `IConfigService::save` void→bool vs the plugin-ABI freeze — **fully reverted**, the flush hook uses `YamlConfig::save` directly, nothing lost; reversible to a HOST_API_VERSION=3 bump if preferred) + 1 Important (`resetStreamRing`'s loop lock never excluded RT_PROCESS callbacks → reader-side `drain()`) + 2 hardening minors (errorContext scoping, Q_OBJECT); all fixed (`588af84`,`b521ec6`), re-review GATE-READY. **Codex gate round 1** (`reviews/2026-07-14-224631`): 1 P1 + 5 P2, ALL SIX CONFIRMED, none dismissed — the P1 caught that drain still raced `write()`'s overflow path (the tap's ring is exactly full at both drain sites) → **gated capture writer** (`captureEnabled_` atomic), drain now only runs fully quiescent on the activate side, deterministic flush test; plus per-transport activity map (any-active wins — a second idle phone can no longer silence the first), `captureCallbackImmutable` flag, dir-fsync failure → `save()` false, `applyVolumeToStream` min(channels,2) helper + [1,2] clamp, service-side gain clamp (`4763edb`,`52e67c5`). **Gate re-run** (one max): 2 P1 + 3 P2 — 3 CONFIRMED+fixed (`755f162`: capture-stream `state_changed` error surfacing → capture-first teardown; `InterfacesAdded` reads State from its own payload; `setStreamActive` result checked, activation failure queues error teardown instead of gating on capture + focus); 2 DISMISSED with reasons to wishlist § "From BT A2DP EQ pre-push gate" (epoch-quiesced ring transitions — µs residual vs ms cadence, worst case one 2.7 ms stale chunk, AudioRingBuffer redesign risk to all audio paths outweighs; legacy capture-callback replace race — pre-existing, zero callers).

**Verification:** suite green at every task boundary and finally **123/123** (`ctest --output-on-failure` in `~/builds/openauto-prodigy`) + app target links clean; ring stress test stable across repeated runs; `bash -n` clean on both installers; cross-build kicked off at session end (check `build-pi/` freshness before deploying).

**User-visible behavior changes** (docs updated in-branch): BT music obeys the Media EQ curve; HU master volume now controls BT playback; music sources (AA/local/BT) take focus from each other by recency — includes one edge change: AA media starting after local playback now wins; EQ slider/bypass state survives power-cut; the third EQ tab reads "System".

**Next 1-3 steps:** (1) deploy to Pi + **bench runbook** (plan § Integration verification, 10 rows — row 1 `pw-dump`s the bluez_input node BEFORE installing the rule; plus the fable-review addition: app-starts-while-BT-already-playing relink row); (2) on bench pass: flip wishlist SHIPPING→SHIPPED, archive plan+design COMPLETED, **push on Matthew's go**; (3) dev→main PR + next ALPHA tag + GitHub prerelease (milestone declared by Matthew 2026-07-14, rides after the EQ work).

---

> Older entries are archived in `docs/archive/session-handoffs/`.
