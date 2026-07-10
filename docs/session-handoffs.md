# Session Handoffs

Newest entries first.

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
