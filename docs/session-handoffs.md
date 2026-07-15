# Session Handoffs

Newest entries first.

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

## 2026-07-11 — HFP/9876 phase stage-1 code-complete: dead-slot fixes, codec kits, CompanionState migration

**What changed:** Executed `docs/plans/2026-07-11-hfp-mic-9876-retirement-plan.md` (11 tasks, SDD with per-task review; branch `worktree-hfp-mic-9876-retirement` off `ae7bf8b`). Commits `e501a3d..03fdba3`:
(1) PhoneStateService hot-plug fixed — registered `QMap<QString,QVariantMap>` slot + `adoptBluezDevice` seam, no live read-back (`56c5dbc`); (2) BtAudioPlugin's three handlers made real slots + sender-path filtering on PropertiesChanged + connect-result logging (`ea14ba0`); (3) CVSD WirePlumber drop-in `config/50-prodigy-hfp-cvsd.conf` (`9215750`); (4) patched-mSBC PipeWire build kit `tools/pipewire-msbc/` — **real package is `libspa-0.2-bluetooth` (RPi OS `1.4.2-1+rpt3`)**, strict dep pinned, deb built + staged at `matt@192.168.1.149:~/pipewire-msbc/`, NOT installed (`bbac826`); (5) ApiInboundState parity — GPS bearing/accuracy/age, staleness (30 s, injectable), per-report owner tracking + disconnect clears, `connected` (`e2f577a`); (6-8) all consumers migrated to new QML context `CompanionState`: 3 widgets incl. dead-`proxyStatus` fix (`ef4c61d`), CompanionSettings status rows — legacy controls annotated for B2 (`e127c4e`), IPC `companion_status` prefers inbound + `"source":"api"` (`65c01e4`); (9) bench runbook `docs/plans/2026-07-11-hfp-bench-runbook.md`; (10) companion handoff prompt at `personal/openautopro/companion-9876-migration-prompt.md` (outside repo); (11) roadmap Now entry (`3f2be13`) + package-fact corrections (`03fdba3`).

**Why:** Mic uplink silent at far end (L6, LC3-SWB encode below prodigy — pin mSBC via patch, CVSD as config diagnostic/fallback); HFP hot-plug + BT plugin subscriptions silently dead (`a{sa{sv}}` family); 9876 retirement gated on companion migration, which needed head-unit inbound parity first (sol review finding — legacy deletion would have broken 4 QML surfaces + IPC).

**Status:** Code-complete, all task reviews Approved (no Critical/Important findings; Minors ledgered in `.superpowers/sdd/progress.md`). Local suite 122/122 + app target green. **Codex gate (gpt-5.6-sol) run + one re-run — 6 P2 findings total, all 6 confirmed and fixed, 0 dismissed:** round 1 (`14aae29`) fresh-pair late-adoption rescan, report-presence decoupled from proxy-route ownership, GPS extras validation (NaN/inf/bearing-range); round 2 (`659d6aa` + `ccd7dfc`) sender-path guard on the disconnect branch (unrelated BT device could reset a live call — pre-existing bug), full GPS reset in clearGps (stale-location leak via IPC), executable runbook mic-capture command. Fix diffs Fable-adjudicated; per-finding TDD evidence in `gate-fix-report.md` (job scratch). Cross-built binary (incl. fixes) rsync'd to the Pi; **service restart + journal check pending Matthew** (remote restart perms) — two positive "D-Bus subscriptions" log lines expected, no "Could not connect".

**Next 1-3 steps:** (1) Pi service restart + journal check (deployed binary includes all gate fixes; expect two positive "D-Bus subscriptions" lines) — then bench session per the runbook (mic A1a→A1b decides shipped codec + installer wiring); (2) Matthew runs the companion migration prompt in the companion repo; (3) B2 teardown planning ONLY after the cutover validates with `companion.enabled: false`. Pushed to origin/dev at end of session (fast-forward from `ae7bf8b`); MINIMEES main checkout needs `git pull` (its local dev is behind); the `hfp-mic-9876-retirement` worktree is merged and can be removed.

**Verification:** `ctest --output-on-failure` (all green) + `cmake --build . --target openauto-prodigy` in `~/builds/oap-hfp-9876`; per-task TDD evidence in `/home/matt/.claude/jobs/bbd97254/tmp/task-*-report.md`; deb presence on Pi verified by `ls` (367,840 bytes, matches local).

---

> Older entries are archived in `docs/archive/session-handoffs/` (2026-02--2026-03 and 2026-07-02--2026-07-08 rollups).

## 2026-07-09 — PR #16 opened + post-review fixes: build-dir docs, development.md refresh

**What changed:** PR #16 (`dev` → `main`) opened for the structure cleanup. Two ride-along commits:
(1) AGENTS.md + docs/plans/README.md now point local builds at the ext4 build dir
(`~/builds/openauto-prodigy`) — building in-repo on `/mnt/e` fights the 9p IO hit (6bae217).
(2) `docs/development.md` refreshed after Matthew's external Codex review flagged it P2-stale:
dropped retired Ubuntu/Qt 6.4 platform framing + dead dual-Qt compat items, replaced the
hardcoded 8-test list with the command per docs convention (suite is far larger), replaced the
rotted component tree with a top-level map pointing at `architecture.md`, fixed stale Pi IP
(.152 → .149), dropped the APK-indexer section (tool moved to open-android-auto in Feb, e0df0cd)
and added a pointer note to `docs/aa-protocol/apk-indexing.md`, updated INDEX.md description.

**Codex review adjudication (post-PR, Matthew-run):** 1 finding (P2 development.md staleness) —
CONFIRMED via tree/ctest checks and fixed as above; 0 dismissed. Codex verification note: one
intermittent `test_companion_listener` timing failure on first full ctest run, passed on reruns —
known-flaky candidate, parked in wishlist.

**Status:** pushed to `dev`, riding in PR #16. Next: merge PR #16.

## 2026-07-09 — DOCS/REPO STRUCTURE CLEANUP: COMPLETE — Diátaxis-lite tree, AGENTS.md SSOT, public face

**What changed:** executed `docs/plans/2026-07-09-docs-structure-cleanup-plan.md` (15 tasks, subagent-driven).
Docs tree restructured: `docs/reference/` (7), `docs/aa-protocol/` (8), `docs/how-to/` (2), single
`docs/archive/` (plans incl. milestones, session-handoffs, validation, research, openauto-pro). Every
plan file carries a `Status:` header; `docs/plans/` holds only live plans + README (conventions +
refreshed executor guidance). Feb/Mar handoffs rotated out of this file (16 kept / 30 archived,
byte-verified). All live pointers re-targeted; `tools/aa_proto_graph.py` output moved to
`docs/aa-protocol/protocol-reference.md` (gitignored). New: `docs/architecture.md`, root `AGENTS.md`
rewritten as agent-instruction SSOT + 4 nested AGENTS.md (src/, src/core/aa/, libs/prodigy-oaa-protocol/,
qml/), CLAUDE.md reduced to pointer stub, tests/scripts/tools READMEs, contributor-facing README,
CONTRIBUTING.md + .github templates, `scripts/check-doc-links.py`. needs-review triage (Matthew):
2 research docs → archive/research, codex corrections → archive/plans, miata hardware ref moved OUT
of repo to personal/miata/. Bonus fix: bare `archive/` gitignore rule anchored to root (was silently
ignoring new files under docs/archive/).

**Why:** contributor-ready public face + one authoritative instruction surface for agents; stale
paths and dead guidance were accumulating (46-entry handoff log, 40+ unstatused plans, README dated Feb).

**Status:** COMPLETE. All 16 commits on `dev`, NOT pushed (awaiting go-ahead).

**Codex gate (range 6b6dfce..19be155):** exit 0, 2 findings, both CONFIRMED + fixed (2bb8a8c), 0 dismissed:
(1) P1 — real hostapd `wpa_passphrase` preserved in the rotated Feb/Mar archive → redacted (deliberate
archive-edit exception). NOTE: the value sits in already-pushed history via the pre-rotation
session-handoffs.md — **rotate the Pi AP passphrase** (cheap: it's our own AP; re-pair phone once).
pi-config/hostapd.conf + wireless-setup.md verified sanitized. (2) P3 — runbook's dead SKILL.md
pointers + stale `../openauto-pro-community/` cross-ref + retired `.152` IP → fixed. Small fixes,
no gate re-run per AGENTS.md convention. Secret-scan + checker-hardening wishlisted.

**Verification:** `scripts/check-doc-links.py` → OK: 0 broken links. Stale-path sweeps clean
(self-references inside the cleanup plan/design docs are spec content, sanctioned). App target builds;
`ctest --output-on-failure` → 100% passed, count identical to pre-cleanup baseline. Per-task subagent
reviews: Tasks 1–4, 6, 11–14 spec ✅; Tasks 7–10 opus batch review → 1 Important (false INI-migration
claim in architecture.md) fixed 69ac1c6.

**Next 1–3 steps:** 1) push `dev` (needs Matthew go-ahead — 20 commits incl. 4 pre-cleanup);
2) rotate Pi AP passphrase; 3) delete the empty `docs/superpowers/` shell on MINIMEES once whatever
Windows process holds it lets go (git-invisible, cosmetic).

## 2026-07-09 — MEDIA PLAYER STAGE 1 BENCH: COMPLETE — all rows pass; 8 bench bugs found+fixed live

**Bench verdict (Matthew on Pi hardware, 2026-07-08 evening → 07-09):** rows 1–7, 9–13
incl. row 11 addendum ALL PASS. Row 8 (BT coexistence) SKIPPED — Matthew deprioritized
BT support entirely ("not sure I even want to support it"; decision wishlisted).
Row 10 verified over the wire with a scratch Python API client (localhost = trusted,
TCP 9810; `ServerHello` v1.1 → subscribe TOPIC_MEDIA → LOCAL_MEDIA + advancing pos).

**8 bugs found by the bench, fixed + verified same session** (commits `37899da..61e7988`):
1. **Focus duck/mute was decorative** — `applyDucking()` wrote `AudioStreamHandle::volume`
   and NOTHING read it; unit tests asserted the field, so 115/115 green while row 9/13
   silently failed. Fix: `FocusGain.hpp` ramped gain (~20ms swing) applied in the PW RT
   callback next to EQ; `volume` → atomic `targetGain` + RT-only `rtCurrentGain`.
   **Lesson: assert observable output (samples), not intermediate fields.**
2. **destroyStream() left survivors muted** — never re-ran applyDucking; AA teardown
   mid-playback would have muted local forever (became real once #1 landed).
3. **Row 13 policy conflict** — main.cpp §6 hook (AA playing → pause local, level-triggered)
   fought the priority-51 design. Fix: local play-start sends **KEYCODE_MEDIA_PAUSE (127)**
   over the AA input channel (same proven path as next/prev) → phone's MediaSession
   actually pauses; AA→local hook now EDGE-triggered (phone re-reports "playing" while
   our pause is in flight — level-trigger whack-a-moles local).
4. **Duck engaged but never released; later nav prompts muted** — focus lifetime keyed to
   phone AUDIO_FOCUS messages, but phones hold nav focus across prompts (no release), and
   one RELEASE after our pause killed speech focus while local Gain muted the channel.
   Fix: focus follows **AV channel streamStarted/streamStopped** (ground truth of audible
   audio); phone's request type kept only as duck-vs-mute hint (GAIN_TRANSIENT=mute,
   GAIN_NAVI=duck) for the speech stream.
5. **QStringList plugin values serialized as ""** — YamlConfig setPluginValue default
   branch (`QVariant(QStringList).toString()`); read side only handled scalars. last_queue
   never survived. Fix: real YAML sequence + sequence read + LongLong; round-trip test.
   (This was the deferred "restorePaused unit test" gap — the bench found it first.)
6. **SIGTERM never reached shutdown** — no handler anywhere; `systemctl restart` killed
   the app before `app.exec()` returned, so `shutdownAll()`/saveState never ran under
   systemd AT ALL. Fix: SIGTERM/SIGINT → queued `QCoreApplication::quit()` (SIGUSR1
   pattern). PLUS: state now saves on every play/pause/stop/track edge — a car head
   unit dies by power cut, shutdown-only persistence was the wrong model.
7. **Stop-edge save clobbered position** — shutdown() saves pos then engine_->stop();
   QMediaPlayer resets pos to 0 on stop → the new edge-save overwrote 29s with 0.
   Fix: `shuttingDown_` gates the edge save (user stop still saves 0 intentionally).
8. **Restore guard defeated by its own seek; zero-frame garbage walked the queue** —
   (a) progressChanged pos>500 cleared `restoring_` because restorePaused's seek echoes
   the position back pre-decode → corrupt restored track auto-skip-played at boot (spec
   §10 violation). `restoring_` now = "no user interaction since restore"; only user
   actions clear it (next/previous were missing the clear).
   (b) FFmpeg misdetects urandom as MP3 (probe score 1), zero decodable frames,
   instant EndOfMedia, NO error → skip/toast policy never engaged. Fix: per-track
   progress high-water mark; trackFinished under 500ms routes into the same
   `handleUnplayable()` policy. Real-world case: corrupt files on USB sticks.

**State:** develop @ `61e7988`, ~36 commits ahead of origin, UNPUSHED. Final review of
the 8 bench commits dispatched (subagent) — push on pass per workflow. Pi runs the
current build; bench fixtures live at `~/Music/bench-row12/` (01/03 good copies,
02/04/05/06 urandom garbage) — kept for future benches. `~/Music` has real music.
Phone-side note: YT Music auto-resumes on AA reconnect (phone setting, not our bug);
"suppress via MEDIA_PAUSE after connect" is a possible wishlist item if it annoys.

**Next:** (1) review verdict → push develop; (2) stage 2 planning (library scanner +
udisks2 automount) — planning inputs in `.superpowers/sdd/progress.md` + wishlist
(per-AA-channel volume sliders from Matthew's bench feedback pairs with EQ page);
(3) BT support keep/demote/drop decision before §6 wiring gets more complex;
(4) NavigationTurnLabel UTF-8 journal spam (~1 line/s during nav) — proto `bytes`
fix belongs to open-android-auto (note filed in wishlist).

## 2026-07-09 — Tiered workflow adoption + Codex pre-push gate shakedown — Complete, push pending go-ahead

**What changed:**
- Adopted concepts from the `fabletieredworkflow` review repo into the superpowers loop (spec: `docs/archive/plans/2026-07-09-tiered-execution-codex-gate-design.md`): plan-time tier tags (`opus`/`sonnet`/`main`) + Definition of Ready, model-pinned dispatch, Opus→Codex(GPT-5.5)→Fable escalation ladder, per-feature pre-push Codex review gate. Deliberately NOT adopted: handoffs/ dir, RUN-STATE.md, /tier command, autonomous architect (duplicate ceremony).
- New `scripts/codex-review.sh` (read-only sandbox, stdin prompt + `-o` verdict, exit contract 0/1/2/4, artifacts in gitignored `reviews/`), TDD'd against a 10-check fake-codex harness. Workflow documented in AGENTS.md §Tiered Execution Workflow; pointer from the repo instructions stub.
- Shakedown = the gate run this push was waiting on. Round 1 (43 commits, 386KB diff): 5 findings → 3 confirmed+fixed (`e1bac4f`: AA focus RELEASE muting in-flight prompts — per-stream active flags; non-async-signal-safe SIGUSR1/SIGTERM/SIGINT — socketpair+QSocketNotifier self-pipe; BT metadata cleared on connect flip when AVRCP beats A2DP — runtime re-publish), 2 dismissed (startTrack persistence — setSource emits save-triggering edges, bench row 11; sub-500ms unplayable heuristic — deliberate bench row 12 trade-off).
- Bonus: fix worker found a PRE-EXISTING app-target build break from `e6c77e8` (`oap::`→`oap::aa::`, main.cpp:746/748) masked by a cached main.cpp.o — ctest never compiles main.cpp (`1927959`). AGENTS.md gate precondition now requires an explicit app-target build.
- Round 2 gate re-run: fix commits drew zero findings; 4 new/deeper findings → 2 confirmed+fixed (`e10920c`: shutdown-order UAF — AudioService is an earlier app child, destroyed before ~PlaybackEngine; became REACHABLE via the new clean SIGTERM quit; fixed via idempotent `releaseAudioResources()` called from plugin shutdown + new idempotency test; AA coexistence reset checked `==Disconnected` but teardown lands on `WaitingForDevice` — now resets on any non-projecting state, KEYCODE_MEDIA_PAUSE gated on `isAaConnected()`), 1 deferred to wishlist (PlaybackEngine ring-buffer flush — needs bench listen + RT-safe API design), 1 re-dismissed (sub-500ms, adjudication stands).
- Wishlist: signal-handler item marked DONE (Codex independently re-found it — nice outside-family validation); deferred flush finding added.

**Why:** Fable usage limits (token-heavy middle now routed to Opus/Sonnet) + formalizing the ad-hoc pre-push Codex review into a standing adjudicated gate.

**Status:** All tasks complete. 115/115 ctest green, app target builds. develop ahead of origin by 46 commits, NOT pushed — awaiting Matthew's go-ahead (gate passed, adjudication recorded).

**Next steps:**
1. Push develop after go-ahead (immediately after, no parallel work — commit/push race rule).
2. Pi deploy + bench: `systemctl restart` clean-quit path (self-pipe + shutdown-order fix), AA focus RELEASE with active nav prompt, BT reconnect metadata (AVRCP-first phones).
3. Media player stage 2 (per roadmap).

**Verification:** `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure` → app target links, 115/115 pass. Gate: `bash scripts/codex-review.sh` → verdicts in `reviews/2026-07-09-{121741,130359}-codex-review.md` (gitignored, on MINIMEES).

## 2026-07-09 — ALPHA-YY-MM-DD-NN versioning: designed → planned → executed → landed; first milestone tag next

**What changed:** Adopted `ALPHA-YY-MM-DD-NN` (annotated milestone tags, minted only on Matthew's call via `bash scripts/tag-alpha.sh`). CMake derives `OAP_VERSION` at configure time (`git describe --match "ALPHA-*" --dirty`, output format-validated, fallback `ALPHA-untagged-<hash>`); PUBLIC compile definition on `openauto-core` feeds every surface: Qt applicationVersion / `--version`, QML Settings→Software row (`Qt.application.version`), IPC status, External API ServerHello + SystemStatus, AA ServiceDiscovery `sw_build`/`sw_version` (previously hardcoded `"1"`/`"1.0"` — phones finally log real builds). `identity.sw_version` removed end-to-end (schema now rejects writes, test-locked; leftover user-config keys retained-but-unread by design). Docs swept: new AGENTS.md § Versioning, config-schema, settings-tree, release-packaging, wishlist. Task commits `487ad49..0a911c4`.

**Why:** Version identity was five disagreeing sources (hardcoded 0.1.0 ×2, config 0.3.0, AA "1", git tags v0.6.6).

**Deviation:** the version test shipped as `tests/test_oap_version.cpp` (the plan's `test_version` name collides with a target in the hands-off protocol submodule).

**Process:** brainstorm → spec → plan → Codex plan review (verdict REVISE: 2 P1 + 7 P2, folded in before execution) → SDD (5 subagent tasks, every task review approved; Task 2 worker survived a session interrupt via transcript resume) → Codex pre-push gate.

**Gate adjudication (reviews/2026-07-09-214747):** 3 findings, 0 code fixes, no re-run. P2 tagless-dirty-marker → dismissed (the no-tag state becomes unreachable once the first tag lands this session; marker informational-only there). P2 packager hardening → confirmed but pre-existing, this branch touched only a comment in that script → wishlist. P3 test depth → dismissed, accepted plan scope (derivation verified live in the landing steps below).

**Verification:** ctest 116/116 green, app target builds, `--version` → `OpenAuto Prodigy ALPHA-untagged-0a911c4`. Handoff rotation: 07-02..07-08 entries → `docs/archive/session-handoffs/2026-07-02--2026-07-08-handoffs.md`.

**Next 1-3 steps:** (1) tag `ALPHA-26-07-09-01` on this landing commit (Matthew pre-approved "tag it when it lands"); reconfigure + rebuild, `--version` must equal the bare tag. (2) cross-build, deploy to Pi, live-verify (Settings row / web widget / binary strings); AA phone-connect check pending if no phone at hand. (3) push dev + tag — requires Matthew's explicit go-ahead.

**Landed (same session):** `ALPHA-26-07-09-01` minted on `086ed24` via `bash scripts/tag-alpha.sh`; reconfigure+rebuild → `--version` = bare tag, `test_oap_version` passes against tagged form; cross-build embeds tag (strings-verified); Pi deployed + service active + journal clean; deployed binary strings-verified on Pi. Pending: AA phone-connect check and eyes-on Settings-row/web-widget check (no phone/bench this session). Push (dev + tag) awaiting Matthew's go-ahead. Gotcha for the record: `git mv` after editing does NOT stage the edits — the landing commit needed an amend + tag re-mint (delete-newest-reuses-number behavior worked as documented).

## 2026-07-10 — Media player STAGE 2 (library + USB automount): executed + gated — Pi deploy + bench pending rows

**What changed:** Stage 2 executed per `docs/plans/2026-07-09-media-player-stage2-plan.md` (Codex-reviewed ×2 pre-execution). Eight tasks: PlaybackPolicy extracted (promoted wishlist item — pure state machine, 10-slot lock incl. both 500ms boundaries); libavformat wired at 4 build sites + MediaTagReader (per-tag fallback, bounded probing) + committed ffmpeg fixtures; MediaLibrary (shared `mediaAlbumBucketKey`, dir-scoped VA collapse, merge resolution, ordinal-deterministic displays, art propagation, compilation role); MediaScanner (worker lifecycle w/ coalescing, per-root QSaveFile cache, grouped art pass, invalid-file caching); QML library tabs (Artists/Albums/Tracks + Folders preserved, play-by-path drill-downs); UsbMediaWatcher (udisks2 async, 3-source volumeRemoved dedupe, order-preserving queue purge w/ wrap, wasPlaying-gated yank recovery, validated eject w/ engine unload); installer polkit rule (5 actions incl. other-seat) + udisks2 runtime dep; docs swept.

**Process:** SDD, 7 worker tasks all review-approved (2 fix loops total: metatype form T3; none T1/2/4-7). Two workers survived session interrupts via transcript resume. Gate (gpt-5.6-sol): run 1 → 1 P1 + 6 P2 (P1: USB-mount race lost saved queue at boot → pending-restore retry on volumeMounted); re-run (THE one) → 2 P1 + 6 P2 (P1s: restore-retry watermark autoplay hazard → onTrackStarted before every restorePaused; eject didn't release the file → PlaybackEngine::unload via setSource(QUrl())). Final fix batch Fable-reviewed (Codex closed per one-re-run rule). Deferred-with-reasons → wishlist §"From media player stage 2 gate".

**Gotcha (recorded):** the cross-build Docker image is CACHED — Task 2's libavformat-dev:arm64 Dockerfile line didn't exist in the image until a manual `docker build`; cross-build failed at pkg_check until the image was rebuilt. Any future build-dep addition needs the image rebuilt too.

**Verification:** suite 121/121 + app target green at 27cf5df; offscreen smoke clean (single udisks-unavailable warning on WSL); cross-build green post-image-rebuild; Pi polkit rule installed (udisks2 pkg already present on Trixie image).

**Next 1-3 steps:** (1) deploy clean-stamped binary + restart, verify watcher armed in journal; (2) Matthew's 12-row bench (tabs-switch first row — no USB; USB rows last: hot-plug/eject/yank playing+paused, yank-race observation, late-Filesystem-interface case, boot-restore-from-USB row for the gate P1 fix); (3) push on go-ahead. NO tag unless Matthew declares a milestone.

## 2026-07-10 — Stage-2 bench saga: 5 rounds, 3 stacked root causes, sol-designed fix — USB hot-plug VERIFIED

**Symptoms (bench):** eject "not available" on served volumes; yank-while-playing undetected (buffer ride-out + strike-walking); replug invisible until manual refresh; immortal toasts with untappable X.

**Root causes (stacked — each fix exposed the next):**
1. QtDBus NEVER CONNECTED ObjectManager `InterfacesAdded` to a `QVariantMap` slot (`a{sa{sv}}` mismatch, runtime "Could not connect") — hot-plug deaf since Task 6; volumes only registered via restart initial scans. Fix: registered `UsbInterfaceMap` (QMap<QString,QVariantMap>) slot type. **BtAudioPlugin carries the SAME dead pattern** (its InterfacesAdded + PropertiesChanged connects fail in every startup journal) — latent, masked by agent/profile callbacks; wishlist.
2. `drivePathOf` only handled `QDBusObjectPath` variants; the newly-opened delivery path can carry other shapes. Fix: accept path/string/argument + log raw type.
3. udisks exports Drives BEFORE probing settles — `Removable`/`CanPowerOff` false ~3ms post-hot-plug, true later via `PropertiesChanged` (never subscribed). One-shot `resolveDrive` sampled the transient and permanently rejected. **Diagnosed by gpt-5.6-sol** (read-only codex escalation after 3 Fable rounds; design in `.superpowers/sdd/usb-cache-design-sol.md`). Fix (b5ffe05): persistent drives_/fsCandidates_ cache, Drive PropertiesChanged subscription, re-evaluation on settle, eligibility `Removable || ConnectionBus=="usb"`, Block Hint policy, consume-on-register, live CanPowerOff at eject, liveness guards; pure `usbCandidateDecision()` unit-locked (5 slots).
Also fixed en route: toast ttlMs=0 immortality (kind-scoped 5s default), 44px notification close target, SD-card partitions self-registering (prefix filter), competing-automounter reality (pcmanfm --desktop + gvfs RUN ON THE IMAGE — run-1 gate dismissal premise corrected in wishlist).

**Verified live (journal, 2 full plug cycles):** yank→instant purge; replug→defer-while-awaiting→registered+mounted in ~130ms; drive-before-block ordering handled; repeat ejects work; toasts self-dismiss. Rows i–ix of the stage-2 bench PASS.

**Remaining:** bench row — boot with saved queue on the stick (gate-P1 pending-restore retry; now exercisable since hot-plug works). Wishlist: Filesystem MountPoints PropertiesChanged; BT plugin dead-slot cleanup; eject-failure queue restore (sol latent #3); polkit scoping.

**Method note:** instrumentation-first debugging carried this — each round's qCInfo/qCWarning at decision boundaries turned the next layer's failure from theory into one journal line. Keep the logs; they are product code now.

**Stage-2 bench COMPLETE (2026-07-10):** boot-restore-from-USB row PASSED (queue restored paused post-reboot once the stick auto-mounted — the gate-P1 pending-restore retry verified live). All stage-2 rows green. Matthew declared the milestone: ALPHA-26-07-10-01.
