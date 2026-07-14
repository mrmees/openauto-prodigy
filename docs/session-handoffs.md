# Session Handoffs

Newest entries first.

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
