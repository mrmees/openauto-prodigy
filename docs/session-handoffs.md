# Session Handoffs

Newest entries first.

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

## 2026-07-14 — Bench liveness check RESULT: expiry PASS (31.99 s, route torn down); self-heal FAIL companion-side → companion FIXED + re-run PASS same evening

**What changed:** ran the B2 liveness live check remotely — no code changed. Setup: Pixel 8 on canonical adb (`E:\Android\Sdk\platform-tools\adb.exe`, serial 39260DLJH000LX), companion connected + reporting with an active SOCKS route (`socks5://10.0.0.21:1080`), AA projecting. Toggled airplane mode via `adb shell cmd connectivity airplane-mode enable` at 17:41:48 CDT — true silent death (no FIN; phone BT stayed up per Android's connected-devices airplane exemption, so gearhead kept retrying wireless-AA over RFCOMM every ~5 s throughout).

**Leg 1 — expiry: PASS.** Journal (SYSTEM unit): `17:42:13.830 API: reporting session expired after 31994 ms without an accepted report` — within the 30 s + 5 s-sweep spec — followed 95 ms later by `route state update: "set_proxy_route" state= "disabled"`. `companion_status` confirmed full owned-state strip: `connected:false, battery:-1, gps zeroed + gps_stale:true, proxy:""`.

**Leg 2 — self-heal: FAIL, companion-side (head unit blameless).** Airplane off 17:43:38; phone re-joined the AP and AA re-projected at 17:43:51 (13 s). The companion service stayed up but never delivered a SYN to :9810 — logcat every ~38 s: `CompanionService: Wi-Fi-bound API v1 socket was denied; retrying this socket unbound`. Root cause: the reconnect loop binds to a **stale `Network` handle** from before the airplane cycle (bind denied), then the unbound fallback routes via the phone's default network (cellular — the AP validates as no-internet) and times out silently (~35 s, which is the retry cadence). No self-recovery in 4+ min; foregrounding the activity didn't help (17:47:04 retry still stale); `am force-stop` + relaunch recovered in <20 s — `connected:true`, battery/GPS repopulated, **SOCKS route re-applied** (also proves the HU re-accepts and re-applies on a fresh session, i.e. the prodigy side of self-heal works). Fix belongs in the companion repo (`org.openauto.companion`, CompanionService socket path): refresh the stored Network on every `onAvailable`/attempt instead of caching, and drop the unbound fallback for AP-subnet targets (it can never route there and burns the whole retry period). Fix prompt: `personal/openautopro/companion-airplane-stale-network-fix.md`. User-facing workaround until then: restart the companion app.

**Verification:** journal lines + `companion_status` transitions quoted above (queried via `nc -U /tmp/openauto-prodigy.sock`); bench left healthy (companion reporting, AA projecting, route active).

**RE-RUN after companion fix — self-heal now PASSES (same evening).** Matthew shipped the companion-side fix (APK `lastUpdateTime` 17:57:29) and the full cycle was re-run unattended: airplane ON 18:19:24 → `18:19:57.830 API: reporting session expired after 34615 ms without an accepted report` (34.6 s, within spec) → route disabled 88 ms later → airplane OFF 18:20:10 → AA transport back 18:20:24 → `18:20:57 route state update: "set_proxy_route" state= "active"` — companion reconnected, re-sent ConnectivityReport, SOCKS route re-applied **~33 s after WiFi rejoin with zero intervention**; `companion_status` fully repopulated (battery/GPS fresh). **Both legs of the B2 liveness contract are now bench-validated end-to-end.** Fix prompt file marked resolved.

**Next 1-3 steps:** (1) web EQ editor promote-or-park (see EQ audit entry below); (2) milestone tag on Matthew's call.

---

## 2026-07-14 — EQ parity audit COMPLETE: on-HU + YAML legs hold, web advanced-EQ leg absent; 4 findings filed to wishlist

**What changed:** executed the Phase F2 audit (`docs/plans/2026-07-05-phase-f-light-plans.md` § F2) against the current tree at `d7ec7c7` — docs only, no code changed. The original outcome statement (roadmap since inception: "EQ plugin with YAML settings file, on-HU component for basic changes / profile swapping, web settings backend for advanced EQ setup and profile creation") audits as: **legs 1–2 hold, leg 3 entirely absent.** On-HU (`EqSettings.qml` via `EqualizerPlugin`) exceeds "basic changes": per-stream (Media/Nav/Phone) 10-band sliders with per-band reset, bypass toggle, preset picker (9 bundled + user section), save-as-user-preset (named or auto-named), swipe-to-delete. YAML round-trip persists per-stream preset names + the user-preset library (2 s debounced + `saveNow` on quit). Web leg: zero `web-config` routes/templates, zero `IpcServer` EQ commands, zero External API EQ surface. Filed to `docs/wishlist.md` § "From EQ parity audit (2026-07-14)": the web editor gap (+ fix shape) and three quirks — unsaved manual gains/bypass reset on restart (`writeToConfig` persists names only); "Phone" engine actually EQs the AA *system* stream (`AndroidAutoOrchestrator.cpp:343`, F2's known quirk re-confirmed) while real call audio (SCO) has no EQ; BT A2DP bypasses EQ entirely (BlueZ→PipeWire native; original OAP's sink-level 15-band LADSPA EQ'd everything — F2's "audible preset change during BT playback" verify line was never satisfiable). Roadmap item moved to Done; program-status header refreshed (Phase F remainder = 0x8012 experiment + key-event nav notes); F2 section annotated COMPLETED.

**Verification:** audit claims grounded by direct reads of `EqualizerService.{hpp,cpp}`, `EqSettings.qml`, `web-config/server.py` + templates, `proto/api/`, `BtAudioPlugin.cpp`, `MediaPlayerPlugin.cpp:49`, `PlaybackEngine.cpp:131`. One empirical check: a throwaway QML-invocation test (QQmlEngine + QQmlExpression reproducing the exact `EqSettings.qml` call path) confirmed `saveUserPreset(int, string)` marshals the JS number into the `StreamId` enum param correctly on Qt 6.8 — 5/5 pass, named + auto-named presets both land ("FromQml", "Custom 1"). Scratch test + CMake line reverted after the run; build dir reconfigured clean (`~/builds/openauto-prodigy`). No production build/ctest ceremony — docs-only change set.

**Next 1-3 steps:** (1) bench liveness live check — needs Matthew + phone at the Pi (procedure in `next-session-prompt-b2-loose-ends.md` § 1: airplane-mode the phone → expect `reporting session expired` in the SYSTEM journal within ~35 s → widgets flip → self-heal on reconnect; record RESULT here); (2) Matthew decides whether to promote the web EQ editor (and/or the persistence/labeling quirks) from the wishlist — any fix is a fresh brainstorm → plan cycle; (3) milestone tag remains Matthew's call.

---

## 2026-07-14 — B2 teardown SHIPPED: CompanionListenerService, port 9876, and companion.* are gone; liveness expiry live on the Pi

**What changed:** executed `docs/archive/plans/2026-07-14-b2-teardown-plan.md` (design same dir; 4 tiered SDD tasks, every review approved, zero fix loops). (1) Core deletion `fa31f73`: the service (705-line .cpp) + its 720-line test + IHostContext/HostContext surface + all main.cpp wiring (incl. the daemon-reconnect proxy-route resync — redundant since `ConnectivityReport` re-applies the route on every ~1 Hz report) + IpcServer legacy fallback; `companion_status` is API-inbound-only (+`gps_stale` rider, −`vehicle_id`, error string unchanged). (2) Installers `725adc6`: `companion-polkit.rules` → `clock-sync-polkit.rules` (installed name `50-openauto-time.rules` unchanged for upgrade overwrite), `companion.*` YAML blocks removed, custom-AP prompt dropped (Matthew decision: 10.0.0.1/24 is the enforced invariant), udisks polkit rule added to the prebuilt installer. (3) Liveness expiry `5beda1f` (PR #19 deferred gate finding settled): 30 s report-age expiry per owning session, 5 s sweep armed only while reporting sessions exist and never restarted while active, expiry strips ONLY the reporting role (actions/notifications/socket untouched), self-healing on next report; 7 tests, injectable clock. (4) Docs sweep `e21686e`: comments, reference docs, wishlist ledger, 2026-07-11 phase docs archived COMPLETED.

**Codex gate (`reviews/2026-07-14-144637`): 1 P1 + 2 P3 — 2 confirmed+fixed, 1 dismissed.** The P1 was a genuine catch: removing a mid-interface virtual shifted the `IHostContext` vtable while `HOST_API_VERSION` stayed 1 with `<=` acceptance — a stale v1 `.so` (e.g. built against the morning's first GitHub release headers) would load and mis-dispatch. Fixed `8abff00`: `HOST_API_VERSION` 2 + exact-match acceptance (C++ plugin ABI has no cross-version vtable compat) + `test_plugin_discovery` RED→GREEN + plugin-api.md note; ride-along `8b3076e` bumps in-tree plugins' `apiVersion()` to 2 for template coherence (static path never validates). P3 confirmed: the two rider wishlist items closed as SHIPPED. P3 dismissed with reason: exact test counts inside the (now archived) plan are point-in-time acceptance criteria in a dated execution artifact; the no-counts convention governs living reference docs. Small fixes — no gate re-run per AGENTS.md.

**Verification:** suite + app target green at every task boundary (`ctest` in `~/builds/openauto-prodigy`); cross-built + deployed to the Pi: pre-flight 4/4, both D-Bus subscription lines positive, ZERO `Companion:` startup lines, on-device `--version` = `ALPHA-26-07-14-01-11-g8b3076e` (matches HEAD), port 9876 connection-refused, API 9810 accepting. Task 3 worker survived an API-drop mid-report (resumed from transcript, committed after re-verify).

**Next 1-3 steps:** (1) bench liveness live check (airplane-mode the phone mid-session → `connected` drops within ~35 s; not a merge gate); (2) EQ audit — last queued design-sprint item; (3) next milestone tag on Matthew's call (prebuilt release now also carries the udisks polkit fix).

---

## 2026-07-14 — Official tags now ship a Pi release; first GitHub prerelease published

**What changed:** process adoption (Matthew): every official ALPHA tag also ships a prebuilt Pi release — cross-build → `tools/package-prebuilt-release.sh` → `gh release create --prerelease`. Codified in AGENTS.md § Versioning and tag-alpha.sh's next-steps output. Executed for ALPHA-26-07-14-01: the repo's FIRST GitHub release is live (<https://github.com/mrmees/openauto-prodigy/releases/tag/ALPHA-26-07-14-01>, asset `openauto-prodigy-prebuilt-ALPHA-26-07-14-01-pi4-aarch64.tar.gz`, `git_commit 300ab23`). The codec deb (`1.4.2-1+rpt3+prodigy1`) was pulled from the Pi's `~/pipewire-msbc/` into `tools/pipewire-msbc/out/` (both gitignored — the deb stays out of git). Earlier same session: tagged binary cross-built + deployed to the Pi (journal healthy, on-device `--version` = ALPHA-26-07-14-01; SSH `--version` needs `QT_QPA_PLATFORM=offscreen`).

**Verification:** tarball contents checked (installer, tagged binary, codec deb, RELEASE.json fields); release page live with the asset attached.

**Next 1-3 steps:** (1) B2 teardown planning; (2) upstream PipeWire draft approval (Matthew); (3) companion-repo rotation-blip fix (shape in memory).

---

## 2026-07-14 — PR #19 final pre-merge Codex gate: 9 findings adjudicated, 3 fixed

**What changed:** final `gpt-5.6-sol` review of the FULL PR #19 range (`origin/main..dev`, 57 files) before merge to main. 9 findings (8 P2, 1 P3) — 3 confirmed + fixed, 2 deferred to wishlist with reasons, 4 dismissed (3 as prior-round adjudications that stand, 1 as technically incorrect). No P1s.

**Confirmed + fixed:** (1) `ApiServer::stop()` left an open pairing window live — a stop/start cycle within the timeout would accept the stale PIN, and QML kept the QR (PIN/QR now cancelled in `stop()` with `pairingChanged` notify; `testStopCancelsPairingWindow` covers stop-during-pairing + restart). (2) packager accepted any `--msbc-deb` filename while `install-prebuilt.sh` finds the staged deb only by the `libspa-0.2-bluetooth_*+prodigy*_arm64.deb` glob — a misnamed deb packaged "successfully" and was then silently skipped at install (basename now validated against the installer glob; negative packaging test case 4). (3) `BatteryWidget` rendered `-1%` for a connected companion that had only sent time/GPS reports (text now requires `batteryLevel >= 0`; canvas fill was already clamped safe).

**Deferred to wishlist (§ PR #19 pre-merge gate):** patched-deb upgrade path (any installed `+prodigy` blocks future `+prodigy2`/rebuild candidates — tolerable while exactly one revision exists; touching hold/upgrade logic in both installers pre-release adds untested risk) and companion reporting-session liveness expiry (silent phone death leaves `CompanionState.connected` true until TCP gives up — expiry cadence is a companion-contract decision, settle at B2).

**Dismissed:** ClockSync blocking exec (pre-wishlisted 2026-07-06, re-dismissed 2026-07-13 — adjudication stands); custom-AP QR/admission hardcoding (wishlisted 2026-07-13, decision is Matthew's); backpressured terminal-frame drain (dismissed-to-wishlist in the wire-fix round — handshake frames are first-bytes-on-fresh-connection); BT `Track` demarshal "cannot extract nested a{sv}" — incorrect: `QDBusArgument >> QVariantMap` is the canonical registered demarshal for `a{sv}` (QtDBus's delivery refusal applies to `a{sa{sv}}`, which this PR fixed via `qDBusRegisterMetaType<BtInterfaceMap>`), the block is pre-existing (2026-02-18) and live-proven on the Pi via the initial-read callers; residual property-read warning already wishlisted (Startup QDBus warnings).

**Verification:** suite 123/123 (`ctest --output-on-failure`), `test_api_server` 10/10 incl. the new slot, `test_prebuilt_release_package` incl. case 4, app target green in `~/builds/openauto-prodigy`. Verdict: `reviews/2026-07-14-074451-codex-review.md` (gitignored, MINIMEES). Fixes are small/scoped — no gate re-run per AGENTS.md (re-run is for substantial fixes).

**Next 1-3 steps:** (1) merge PR #19 dev→main; (2) mint + push the ALPHA milestone tag, reconfigure + rebuild; (3) B2 teardown planning.

---

## 2026-07-14 — Settings merge: Companion + External API become one Companion page

**What changed:** Matthew-approved design (archived: `docs/archive/plans/2026-07-14-companion-settings-merge-design.md`) executed in `2a35275` + gate fixes. One menu entry ("Companion", phone icon, pageId `api`); `ApiSettings.qml` is the merged page — Remote Client Pairing (PIN + QR), Phone Status (five live rows ported verbatim, `CompanionState`/`SystemService` bound), Advanced (`api.enabled` with a "powers companion, web widgets, and remote clients" caption; `api.expose_lan`). `CompanionSettings.qml` DELETED with its legacy 9876 pairing controls (companion.enabled toggle, Generate Pairing Code, legacy QR dialog — pre-approved B2 content that was driving a DISABLED listener). `companion.*` config keys work UI-less until B2; `CompanionService` context property now has zero QML consumers (comment updated; B2 sweeps it). `settings-tree.md` + `state-matrix.md` updated same-commit.

**Why:** two overlapping settings sections for one feature area, one of them half-dead — a user could "pair" the retired legacy protocol against a listener that isn't running.

**Codex gate (2 P2 + 1 P3, all confirmed, 0 dismissed):** (1) pairing UI was enabled even when the API server wasn't running (main.cpp exposes ApiService unconditionally) — `ApiServer` gains a `running` Q_PROPERTY (+`runningChanged` on start/stop transitions), `startPairing()` refuses on a non-running server (guards the action path too), QML gates the section with an "API not running — enable it under Advanced" hint; `testPairingActionRegistered` updated to the new contract (registration pre-start ✓, window pre-start ✗) and the loopback QR test asserts the unstarted no-op. (2) this handoff entry (was planned post-gate; gate correctly wants it pre-push). (3) merge-contract regression test added (`testCompanionApiMergeContract`: single Companion→api menu entry, no legacy pageId/page/controls, all three sections + config paths present).

**Verification:** suite 123/123 (`ctest --output-on-failure`) + app target green in `~/builds/openauto-prodigy`; `scripts/check-doc-links.py` OK; cross-built + deployed to the Pi (journal healthy). On-device eyeball of the merged page = Matthew, at the same bench visit as the QR scan.

**Next 1-3 steps:** unchanged from 2026-07-13 (2): (1) companion QR end-to-end scan at the bench — last PR gate; (2) upstream PipeWire draft approval; (3) B2 teardown planning.

---

## 2026-07-13 (2) — Post-bench execution: time "regression" was a false positive (3 real bugs fixed + live-validated), QR pairing shipped, installers wire the codec, upstream draft ready

**What changed:** All four post-bench work items executed same-day (commits `aad49fc..3a1e60e` + this entry).

**(1) §7 time row — the headline: the bench diagnosis was WRONG, and the investigation found worse.** `timeReported` was never dangling — `main.cpp` has consumed it since `8e7878c` (2026-07-06, in the deployed lineage). The FAIL verdict came from a bad grep + `journalctl --user` on a SYSTEM unit + an observable that can't fire on an NTP-synced Pi. Systematic re-investigation surfaced **four real bugs**, all fixed: (a) both clock-step copies passed a UTC wall-clock string to `timedatectl set-time`, which parses LOCAL time — the clock would step ~5 h wrong on the Chicago Pi; (b) no polkit rule for `set-timezone` (v1.1 zone step was dead on arrival); (c) **timedated refuses `SetTime` entirely while NTP is enabled — even offline/unsynced, i.e. ALWAYS in the car** (gate re-run P1; refusal reproduced verbatim; the legacy path shared this flaw, so in-car phone clock sync never worked); (d) the logic lived in two untested copies. Now: tested `ClockSyncService` (injectable exec/clock/zone; " UTC" suffix; `set-ntp false/true` sandwich restored on failure; timedatectl exit semantics hardened), `main.cpp` wired to it, polkit rule grants all three timedate1 actions to `isInGroup("bluetooth")` (was hardcoded `matt`, set-time only). Legacy copy untouched — dies at B2. **Live-validated twice on the Pi**: induced −2 min drift → `ClockSync: clock adjusted by 119922 ms` + tz round-trip Denver/Chicago; then again with NTP ENABLED + udp/123 blocked (true in-car state) → `ClockSync: clock adjusted by 119865 ms`, timesyncd journal showing the sandwich. **B2 teardown planning unblocked.** Runbook §7 row carries the dated correction + both re-validations.

**(2) QR pairing (PR gate item):** `ApiServer.pairingQrDataUri` — lazily rendered QR of `prodigy://pair?host=10.0.0.1&tcp=&ws=&pin=` while a window is open (hidden when either listener is down — no dead-endpoint QRs), shown beside the PIN in ApiSettings.qml; shared `core/QrPng` (4-module quiet zone per ISO 18004, raster-verified in test). Payload contract unit-tested. Companion-side scanner task: `personal/openautopro/companion-qr-pairing-prompt.md` — **end-to-end scan needs Matthew's companion update + bench**. New binary deployed to the Pi (journal healthy).

**(3) Installers/packager wire the shipped codec:** both installers gain `install_msbc_codec_fix` (payload > repo-out > home deb search, first match wins; apt `-s` simulation guarded against `set -e`; install + hold; bluetooth-first activation restarts; idempotent path live-verified on the Pi). `package-prebuilt-release.sh` now REQUIRES the deb (`--msbc-deb <path>` explicit, `--allow-missing-msbc-deb` dev override); release test covers staged/refused/override. CVSD drop-in stays out of installers (repo fallback only).

**(4) Upstream PipeWire issue:** draft with the controlled A/B at `personal/openautopro/pipewire-lc3-swb-issue-draft.md` — **Matthew approves BEFORE posting** (post-approval checklist inside).

**Codex gate:** round 1 — 7 findings (1 P1, 6 P2), all confirmed, 0 dismissed: 6 fixed (`cb2a285`; one self-caught pre-report, `19bd32f`), 1 wishlisted as pre-existing systemic (custom-AP address unsupported by admission + both QRs — decision is Matthew's). Re-run — 5 findings (2 P1, 3 P2): 3 fixed (`3a1e60e`, incl. the NTP P1 above), 2 dismissed to wishlist with reasons (backward-guard identical-target semantics; blocking exec — both pre-wishlisted, legacy-identical, async is a one-spot seam change). One re-run max; dismissals stand.

**Verification:** suite 123/123 (`ctest --output-on-failure`) + app target green in `~/builds/openauto-prodigy`; release-package test green; `bash -n` on all three shell scripts; live Pi evidence inline in the runbook §7 rows; Pi left clean (NTP re-synced, zone Chicago, service healthy, both D-Bus subscription lines).

**Next 1-3 steps:** (1) Matthew: run the companion QR prompt in the companion repo, then bench the end-to-end scan — last PR gate; (2) Matthew: approve/edit the upstream draft, then post + link the issue in `tools/pipewire-msbc/README.md`; (3) B2 teardown planning (design §B2) — now unblocked; fold in the legacy ClockSync/QR copies and the custom-AP wishlist decision.

**ADDENDUM 2 (same day, wire fix — companion live bench):** the companion's Pixel bench caught a REAL server bug my FakeTransport wire test masked: **no terminal frame (the typed Error OR any legacy AuthReject) had ever reached a real TCP client** — the frame was written in the same event-loop turn as teardown, and ApiServer's `deleteLater` destroyed the socket on the next turn before Qt's write buffer flushed (destroy = abort = frame discarded → EOF after 0 bytes, reproduced against the deployed Pi with an AP-address probe before fixing). Fix (`707be98` + gate round `829f247`): `flush()` before close on both transports (kernel delivers + FINs even if the object dies); new real-TCP regression mirrors the production session lifetime exactly (heap session + deleteLater) and requires prefix + all 29 bytes BEFORE EOF; slow-consumer kill kept immediate via new `IApiTransport::abort()` + `teardown(CloseMode::Discard)` (flush had turned the overflow RST into data-behind-a-full-buffer); `closed()` now emits exactly once (once-guard, real-socket test). Gate on the fix: 2 P2 + 1 P3 — 2 fixed, 1 dismissed-to-wishlist (bounded-drain teardown for backpressured mid-session terminal frames; handshake frames are first-bytes-on-fresh-connection). **Post-fix live probe on the Pi: `00 00 00 1d` + 29 bytes then EOF — byte-identical to the documented contract** (dump in `companion-qr-pairing-prompt.md`). Suite 123/123; deployed.

**ADDENDUM (same day, contract rev 2 — companion integration feedback):** two head-unit contract gaps fixed (`f01f338` + `d947693`, gated: 1 P3 confirmed+fixed, 0 P1/P2): (a) QR gains required additive `ssid` field, percent-encoded, from `connection.wifi_ap.ssid` (Android can redact the AA-owned network's SSID; companion persists it for reconnect) — bench Pi emits `...&ssid=Prodigy_e57d`; (b) closed/expired pairing window now answers the TYPED `Error{ERROR_CODE_PAIRING_WINDOW_CLOSED(5), "Pairing window closed"}` echoing the ClientHello request_id, then clean close (was untyped AuthReject; other auth failures keep AuthReject). Wire frame pinned byte-exact in `test_api_session` + maintainer doc (`companion-qr-pairing-prompt.md`, exact bytes verified against a reference encoder). Companion side reported DONE same day — end-to-end bench scan is the remaining PR-gate step. Deployed to the Pi; suite 123/123.

---

## 2026-07-13 — HFP bench + companion v1 cutover: mSBC ships, LC3-SWB bug confirmed clean, time-sync regression found (B2 blocker)

**What happened:** Full bench session per `docs/plans/2026-07-11-hfp-bench-runbook.md` (all RESULT rows filled inline there — that file is the detailed record). Service restart + journal check passed (both D-Bus subscription lines, zero failures; BtAudio caught live A2DP/AVRCP on hot connect).

**Codec track (§0-§2):** Bench mic on the Unitek was ANALOG-DEAD (USB capture running, mixer clean, pure zeros) — the original "far end hears nothing" evidence was contaminated; mic swapped (Jieli UACDemoV1.0, default persisted via `wpctl set-default`). Matthew-approved deviation: stock LC3-SWB retest with working mic BEFORE the patch — Codec `y 3` live, premises green, far end SILENT → **LC3-SWB encode bug CONFIRMED with a controlled A/B** (same hardware/path: CVSD `y 1` audible, LC3-SWB silent). Patched deb installed + held (`libspa-0.2-bluetooth 1.4.2-1+rpt3+prodigy1`); mSBC `y 2` negotiated, far end clear, wideband quality good. **mSBC IS THE SHIPPED FIX**; CVSD drop-in deleted from /etc (kept in repo as fallback).

**L-rows (§3-§6):** DTMF ✓ (IVR reacted to "1"/"8"). RejectSCO=true half ✓ — no SCO nodes, call stays on handset, no AA degradation → **default stays `false`**. Samsung S25 Ultra row: mSBC ✓, caller-ID ✓, Phone-view dial ✓, HU hangup ✓, far-end mic ✓; answer/reject buttons DEAD during AA (wishlisted, zero journal trace on press). Moto G Play 2024: NO cell service — pairing + HFP SLC ✓, rest N/A. Volume rocker tracks car output ✓, echo fine ✓.

**§7 cutover (`companion.enabled: false`, migrated companion app):** Port 9876 dead (refused from LAN) ✓. GPS/battery/charging/SOCKS5 all validated over API v1 (redsocks actively relaying; owner-disconnect clearing verified via adb force-stop → full reset + widget offline + clean reconnect) ✓. Legacy fallback: **wire-proven zero** — 90 s packet sniff across app relaunch, 0 packets on 9876 ✓. **TIME PAYLOAD FAILED — cutover regression:** `ApiInboundState::timeReported` is a dangling signal (nothing consumes it); legacy `adjustClock()` (timedatectl/polkit, drift+backward guards) never wired to v1. RTC-less Pi in the car will never sync its clock. **This blocks B2 teardown.**

**Ops rules discovered:** restart order `bluetooth` → `pipewire wireplumber` → `openauto-prodigy.service` (RegisterProfile NotPermitted race kills HFP silently; app's PipeWire enumeration dies with the daemon). Telephony `Codec` property lingers with no call — always pair with SCO-nodes-present check; verify ag1 by property read (`busctl tree` shows no children). Units are SYSTEM-level (`journalctl -u`, not `--user`). Canonical adb: `E:\android\sdk\platform-tools`.

**Next 1-3 steps:** (1) **Wire `timeReported` → adjustClock logic** (extract from CompanionListenerService; small, TDD) and re-validate §7 time row — B2 planning stays blocked until then; (2) installer wiring for the shipped codec: patched-deb install + `apt-mark hold` procedure into both installers (per §2 verdict; document the hold + upgrade caveat); (3) upstream PipeWire issue draft with the A/B evidence (CVSD audible / LC3-SWB silent, same stack) — **Matthew approves text BEFORE posting**. New findings in `docs/wishlist.md` "From HFP/9876 bench (2026-07-13)" (8 items + adb tooling note).

**dev→main PR gates (Matthew, 2026-07-13):** the PR waits on BOTH (a) the time-sync fix + §7 time-row re-validation, and (b) **QR-code pairing for API access working** — companion onboarding needs the QR path functional before this ships to main.

**Verification:** All claims instrumented live on the Pi during calls (busctl property reads with SCO-node cross-checks, pw-link topology, level-metered captures, packet sniffer); per-payload IPC polls for §7. Bench hardware casualties: Unitek-attached mic (dead capsule) + bench amp (died mid-session, replaced/power-cycled).

---
