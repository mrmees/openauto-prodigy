# Session Handoffs

Newest entries first.

---

> Older entries (2026-02 / 2026-03) are archived in `docs/archive/session-handoffs/2026-02--2026-03-handoffs.md`.

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

## 2026-07-08 — Media-player arc, Task 12: integration verification + Pi deploy — bench checklist pending Matthew

**Branch:** `develop` @ `2aeb411` (`docs(aa): audio-focus push investigation`), 18 unpushed commits since `192b0fa` — the whole stage-1 media-player arc (Tasks 1–11). Working tree clean at build time; binary and QML both built/deployed from this commit.

### Verification results (automatable portion; Matthew not present for touch/audible rows)

1. **Full local suite:** `cmake` + `make -j16` + `ctest` in `~/builds/openauto-prodigy` — **114/114 passed, 0 failed** (26.77s). (The task brief's "92 tests" figure was stale — the suite grew with API v1 etc.; 100% pass is the gate.)
2. **Cross-build:** `sg docker -c './cross-build.sh'` (stale-group workaround needed as expected) — fast app-only mode, exit 0, `build-pi/src/openauto-prodigy` produced: `ELF 64-bit LSB pie executable, ARM aarch64`, 29.9 MB, `MediaPlayerView_qml.cpp.o` visible in the qmlcache compile.
3. **Deploy (17:00 EDT):** `mkdir -p ~/openauto-prodigy/build/src ~/Music` → rsync binary → rsync `qml/` → rsync `tests/data/media/` → `~/Music/fixtures/` (tone-44k.mp3, tone-48k.flac) → `systemctl restart openauto-prodigy.service`. All clean. Note: journal QML paths are `qrc:/…` — QML ships inside the binary via qmlcache, so the disk `qml/` rsync is belt-and-braces. The Pi's git clone is still at `9599ec6` (commits go over git after review, per plan).
4. **Health checks (all PASS):**
   - Service **active**; preflight 4/4 PASS; `sd_notify READY=1`; watchdog heartbeat started.
   - Journal: `Registered static plugin: "org.openauto.media-player"` + `Plugin initialized: "org.openauto.media-player"` (registration → init 09.197 → 09.514, clean).
   - **NRestarts=0**, MainPID 193378 stable across a 60s watch — no crash loop. Crash-keyword journal sweep: only routine lines (my own restart's "Stopped", MediaStatusChannel "STOPPED" debug from the auto-connected Pixel 8 / idle YouTube Music).
   - QML: **no errors**. One repeated **warning**: `NowPlayingWidget.qml:48:5 QML MaterialIcon: Binding loop detected for property "size"` — line 52 `size: Math.min(width, height) * 0.5` reads the MaterialIcon's *own* width/height (which derive from `size`) instead of `nowPlayingWidget`'s. Cosmetic, app healthy; **follow-up fix needed** (should be caught in the review pass before push).

### Bench checklist (spec §12 stage 1) — status

| # | Check | Result |
|---|-------|--------|
| 1 | Media Player appears in nav; Folders browse works by touch | [pending bench] — plugin registered + initialized in journal; view compiled into binary |
| 2 | Local file plays with audio out | [pending bench] — fixtures seeded at `~/Music/fixtures/` |
| 3 | **[M]** EQ preset audibly changes local playback | [pending bench] |
| 4 | Master volume applies to local playback | [pending bench] |
| 5 | Now-playing bar: art, title, progress advance, seek works | [pending bench] |
| 6 | Dashboard NowPlayingWidget: art, progress, source badge, play state correct | [pending bench] — widget instantiates on dashboard (binding-loop warning proves it's live); visual correctness needs eyes |
| 7 | Widget transport controls drive local playback | [pending bench] |
| 8 | **[M]** BT pauses when local starts; local pauses when BT starts | [pending bench] — Pixel 8 auto-connected via BT post-restart, so the BT side is ready |
| 9 | **[M]** AA nav prompt ducks local audio (not pause) | [pending bench] |
| 10 | API v1 media stream reports LOCAL_MEDIA + position fields | [pending bench] — serializer unit-covered in suite; live WS observation not run (no local playback possible without touch) |
| 11 | State restores paused after service restart | [pending bench] — needs a playing session first |
| 12 | Unplayable-file policy (skip once / 3-in-a-row stops with toast) | [pending bench] |

Phone re-pairing NOT needed: the fresh install already has the Pixel 8 paired (journal: `Found 1 paired device(s)`, `Phone connected via BT: "Pixel 8"`).

**Next:** (1) Matthew runs the 12-row checklist on the bench (~20 min incl. rows 8–9 with phone); (2) fix the NowPlayingWidget binding loop; (3) `superpowers:requesting-code-review` over the arc, push on pass; (4) plan stage 2 (library + USB automount).

Full verification evidence: `.superpowers/sdd/task-12-report.md`.

---

## 2026-07-08 — Media-player arc, Task 1 spike: QAudioBufferOutput real-time pacing — VERDICT: GO

**Branch:** `develop`. Standalone spike tool (own CMakeLists, builds outside the main tree) — gates Task 6 (PlaybackEngine) of the media-player arc; nothing else in Tasks 2–5 depended on it.

**Question:** does `QMediaPlayer` pace PCM delivery through `QAudioBufferOutput` in real time with **no** `QAudioOutput` device sink attached, or does it decode as-fast-as-possible? Built `tools/spike-qmp-tap` (`main.cpp` + `CMakeLists.txt`, exactly as specced) against system Qt 6.8.2, ran three invocations against throwaway 20s 440Hz tones generated with ffmpeg (44.1kHz mp3 + 48kHz flac, `/tmp/claude-spike-media/`, not committed).

**Results (RESULT lines):**
- mp3 (44.1kHz→48kHz convert): `RESULT: pcm=20.00s wall=20.04s ratio=1.00 formatOk=0 bufs=768` — steady-state ratio 1.00 throughout (verified at every buf#20 checkpoint from buf#20 to buf#760).
- flac (48kHz, no rate conversion): `RESULT: pcm=20.00s wall=20.00s ratio=1.00 formatOk=0 bufs=210` — ratio settles from 1.05→1.01→1.00 as the startup transient washes out.
- mp3 + seek to 15000ms at wall≈2s: seek accepted (`pos before: 1828`), **pos 1s after seek: 15882 ms** (within the 15000–16200 expected band), ratio stayed 1.00 pre- and post-seek, `RESULT: pcm=6.91s wall=6.94s ratio=1.00 formatOk=0 bufs=267` — EndOfMedia fired ~5s after the jump (15000→20035ms of remaining media), exactly as expected.

**`formatOk=0` in all three RESULT lines is a false alarm, not a format-conversion failure** — diagnosed with a scratch-only instrumented copy (not committed): in every run the *only* mismatching buffer is the very last one delivered, and it is `isValid()=false, byteCount=0, startTime=-1` — a default-constructed end-of-stream sentinel, not real PCM. Every real audio buffer (767/768 mp3, 209/210 flac) matched the requested 48kHz/stereo/Int16 format exactly, confirming Qt performs the 44.1→48kHz conversion transparently. **Real gotcha for Task 6:** `PlaybackEngine`'s `audioBufferReceived` handler must guard `buf.isValid()` (or `byteCount() > 0`) before touching format/data — the naive check-every-buffer approach in the spike's own harness code trips on this sentinel.

**Metadata:** title `'Spike Tone 44'` / `'Spike Tone 48'` visible on every `mediaStatus=` line as specced.

**Decision table:** ratio was ≈1.0 in all three runs from the outset (never racing ahead near-instantly) — the muted-sink retest (`QAudioOutput` @ volume 0) was **not needed** and not run.

**Verdict: GO.** Task 6 proceeds as written — `QAudioBufferOutput` tap with no device sink, paced by the media clock, no crutch required. Only addendum: guard `isValid()`/`byteCount()>0` on incoming buffers (documented above). Harness updated post-review: sentinel buffers are now skipped, so re-runs report formatOk=1 without needing this note.

**Files:** `tools/spike-qmp-tap/{main.cpp,CMakeLists.txt}` (spike tool, kept in-tree as a reusable harness — not wired into the main build). Full raw output + self-review: `.superpowers/sdd/task-1-report.md`.

---

## 2026-07-07 — Web-widget quality batch + Theme-upload endpoint (design → plan → implementation)

**Branch:** `develop` (direct, per single-develop-branch workflow). All work pushed. No Pi deploy this session (see Pending).

### Part 1 — Web-widget quality mini-batch (QB/QC/QD), each per-task reviewed, zero Critical/Important
Follow-ups from the JS-runtime FINAL REVIEW, dispatched SDD-style (sonnet implementer + independent sonnet review):
- **QB (`8b10c7f`) — shim hardening trio** (`resources/web/prodigy.js`, `qml/widgets/WebWidgetHost.qml`): (1) `subscribe()` uses `hasOwnProperty` not `in` (prototype-chain topics like `'toString'` were passing validation → garbage on the wire); (2) `pushContext()` on `LoadSucceededStatus` (a renderer crash-reload reuses the same WebEngineView, so the creation-time bootstrap spans go stale — reviewer confirmed); (3) `request()` guards `ws.readyState !== 1` (WebSocket `send()` on a CLOSED socket *silently discards* per WHATWG — reviewer independently spec-verified — so a request in the disconnect window black-holed a `pending` entry forever). Out-of-scope residual (deferred): OPEN-but-pre-serverHello window still black-holes since `readyPromise` never re-pends.
- **QC (`f66a387`) — field-debug logging + api.enabled warning** (`src/core/widget/WebWidgetScanner.cpp`, `src/main.cpp`): qInfo on a subdir skipped for missing `widget.yaml`; always-log the scan count+path (even 0); qWarning when web widgets are registered but `api.enabled` is false (zombie "connecting…" widgets). `api.enabled` defaults true (missing key does NOT warn — reviewer-verified not off-by-default).
- **QD (`382611d` + accuracy fix `81cfd8e`) — web-widget authoring guide** (`docs/web-widget-authoring.md` + cross-links): shim surface + v1 known-limitations (off-the-record ephemeral localStorage, D2 shared-origin quoted verbatim, `api.enabled` dependency, reconnect-gap request rejection, locked-down sandbox, D5 crash recovery). Reviewer traced every claim to source; two accuracy nits fixed forward (real api.enabled warning text; "five scripts inject" not two). NOTE: `382611d` got pushed pre-review (a concurrent `git push` for QC swept it up) — reviewed post-hoc, fixed forward. No force-push.

### Part 2 — Theme/wallpaper upload endpoint (companion migration off legacy port 9876)
Full brainstorm → design → plan → implementation, all this session.
- **Brainstorm decisions (Matthew):** Q1 auth = **none** (match web-config; a proper all-routes auth pass is wishlisted — the endpoint adds nothing `set_config` doesn't already expose unauthenticated); Q2 scope = **companion-only** (browser upload UI is a fast-follow); Q3 blob transport = **temp-file handoff** (Flask writes the wallpaper to a temp file, passes the *path* over IPC — not base64 inline); endpoint name `/api/theme/install`; require both `light`+`dark`; 5 MiB cap.
- **Design doc `911d325`:** [`superpowers/specs/2026-07-07-theme-upload-design.md`](superpowers/specs/2026-07-07-theme-upload-design.md). §4 is the **HTTP contract handoff artifact** the companion maintainer is blocked on (endpoint, `manifest` schema, response codes). Fixes the legacy ack-lie (`CompanionListenerService` sends `accepted:true` even on failure — the new endpoint returns the real `importCompanionTheme` result).
- **Plan `3c695d6`:** [`superpowers/plans/2026-07-07-theme-upload-implementation.md`](superpowers/plans/2026-07-07-theme-upload-implementation.md) — 3 SDD tasks, full code in every step.
- **Implementation (opus implementers, per Matthew; independent review each task):**
  - **T1 `5f0de8c` + fix `de87c16`:** pure `oap::parseThemeInstall()` validation module (`src/core/services/ThemeInstallRequest.{hpp,cpp}`) — name/color/camelCase→hyphen/wallpaper magic+size+canonical-path-under-`/tmp/oap-theme-upload` — plus a behavior-preserving `static ThemeService::slugify()` extraction. Fix: size-check before `readAll` (bound the read). 16-slot unit test incl. path-injection.
  - **T2 `47885c5`:** `install_theme` IPC command + thin `IpcServer::handleInstallTheme` (parse → `importCompanionTheme` → slugify). Socket round-trip test (implementer added a `processEvents` pump loop matching `test_companion_listener.cpp` — the brief's `waitForReadyRead`-only helper can't dispatch server slots on a shared thread; reviewer verified non-vacuous).
  - **T3 `0efc813`:** Flask `POST /api/theme/install` (`web-config/server.py`) — multipart → temp file → IPC; `MAX_CONTENT_LENGTH` 413; status mapping; `finally` unlink. 6 Python tests (reviewer re-ran 6/6).
- **Final whole-branch review (opus): READY TO MERGE, zero Critical/Important.** All 6 cross-task seams traced clean (field names, temp-dir literal, camelCase→hyphen applied exactly once + round-trips through ThemeService, response shape, ack-lie fix, size-cap coherence). `camelToHyphen` verified char-identical to the frozen `CompanionListenerService` lambda (intentional dup, wishlisted for dedup at 9876 retirement).
- **Polish `2029b64`:** temp-dir cross-reference comments (server.py ↔ IpcServer.cpp) + companion-maintainer note in design §4 (send `manifest` as a form field with **no filename**, else Werkzeug → `request.files` → 400).
- Suite: 107 → **109** C++ (T1 +1, T2 +1) + **6** Flask tests.

### Wishlist added this session
All-routes web-config auth pass; browser theme-upload UI (fast-follow); conversion/slugify dedup at 9876 retirement; `/tmp/oap-theme-upload` janitor; theme-upload temp-dir agreement integration test + T1 test precision; 503-vs-500 IPC-status mapping.

### Pi deploy — DONE (2026-07-07 ~17:32 CDT)
Cross-build (fast app-only, aarch64 verified: fresh mtime, `install_theme` symbol present) → rsync binary → `git pull` (Pi d5088f7 → **88f5324**) → restart `openauto-prodigy.service` → **enable+start `openauto-prodigy-web`** (it was enabled-but-never-started — the known "installer never started it" gap; Flask 3.1.1 present, came up clean). On-device verified: app **active NRestarts=0** (and mid-AA-session — H.265 HW decode, restart reconnected AA cleanly, no errors); web **active** on `0.0.0.0:8080`; `POST /api/theme/install` (empty) → **400 "missing manifest"** (route live, not 404); dashboard → 200; `/tmp/openauto-prodigy.sock` present (web↔app IPC path intact). The whole QB/QC/QD batch also rode this binary/`git pull`.

### Pending / next session
- **Real wallpaper upload end-to-end on-device (Matthew's check):** NOT run at deploy time on purpose — `importCompanionTheme` auto-switches the active theme, so a smoke test would change the live UI. Only the color-only path is unit-covered (the wallpaper path goes through the handler's hardcoded `/tmp/oap-theme-upload`, unexercised by any test — see wishlist temp-dir agreement test).
- **Companion maintainer:** the HTTP contract (design §4, incl. the manifest-form-field note) is ready to hand off; they were blocked on it.

---

## 2026-07-06 — External API v1 — Implementation Complete (Tasks 1–16)

**Branch:** `external-api-v1` (18 commits since `main`). Design doc: [`superpowers/specs/2026-07-06-external-api-v1-design.md`](superpowers/specs/2026-07-06-external-api-v1-design.md). Plan: [`superpowers/plans/2026-07-06-external-api-v1-implementation.md`](superpowers/plans/2026-07-06-external-api-v1-implementation.md).

**What changed — all 16 plan tasks executed, TDD per task, 102/102 tests green:**
- `proto/api/` — new protobuf contract (`api`, `common`, `media`, `navigation`, `projection`, `phone`, `system`, `notifications`, `actions`, `companion`), package `prodigy.api.v1`, additive-only from freeze.
- `src/core/api/` — full server: `ApiFramer` (TCP length-prefix framing), `ApiAuth`/`PairingManager` (windowed PIN challenge/response + `PairedClientStore`), `ApiSerializers` (service state → proto, per §8 normalization tables), `ApiTransport` (`IApiTransport` + `TcpApiTransport`/`WsApiTransport`), `ApiPublishers` (coalescing publishers for media/navigation/projection/phone/system), `ApiSession` (per-connection state machine: hello → auth/pairing → ready, backpressure disconnect), `ApiRequestHandlers`+`ApiInboundState` (action/notification/phone bridges, companion ingest for GPS/battery/connectivity/time), `ApiServer` (listeners on 9810/tcp + 9811/ws, peer-subnet admission policy, lifecycle).
- Wired into `main.cpp`: `ApiServiceRefs` struct binds concrete service pointers (media/navigation/projection/phone/theme/notifications/actions/config/bluetooth); `api.*` config defaults added to `YamlConfig::initDefaults()`; three media-transport actions (`media.playPause`/`media.next`/`media.previous`) registered so the API can dispatch playback control.
- `qml/applications/settings/ApiSettings.qml` — enable toggle, LAN exposure toggle, pairing PIN flow.
- `tests/test_api_*.cpp` — 11 new suites, including a full loopback integration suite (`test_api_loopback`) covering the design's §15 mandatory cases (auth, subscribe/publish, backpressure disconnect, capability-gated phone commands, companion ingest).

**Deviations from the design (all recorded, each with a reason):**
1. **Phone seam (Tasks 7/9/11).** HFP D2 (call audio) landed before API v1, so per the executor handbook's dependency map the API applied the truthful-capability code path from the HFP plan's Task 8 instead of the stale all-false mock branch: widened call-state mapping (Dialing/Alerting/Held/Waiting), capability flags sourced from `IPhoneStateService::telephonyAvailable()` (`hold_swap`/`multiparty` remain hard-false — not reachable in PipeWire 1.4.2's surface), `phoneCommand()` returns UNAVAILABLE/FAILED/OK, and `PhonePublisher` is also wired to `telephonyAvailableChanged`. Static `Capabilities.phone` in `ServerHello` stays all-false per the API plan — runtime truth lives in `PhoneStatus.capabilities`, not the handshake.
2. **ThemeService fix (Task 6, Matthew-approved).** Six derived color tokens (success / on-success / warning / on-warning / surface-tint-high / highest) were resolving to transparent via `color(name)`. Fixed by routing them through `ThemeService::activeColor()` (commit `44e61cf`); the API serializer stays plan-exact and just consumes the corrected values.
3. **Connectivity fold fix (Task 11, review-driven).** The `system` proto's proxy route is `internet_available && socks5_active` (commit `628af0a`), not internet alone — kills a false-positive where the proxy shows "up" while the phone is actually offline. Raw internet-only bit is not separately exposed in v1.
4. **`api.*` YamlConfig defaults pulled forward from Task 13 into Task 12.** `setValueByPath` validates keys against the defaults schema, so the defaults had to land before the config-write path that depends on them; values are exactly per the design doc's §12 table.
5. **Task 11 additions.** `ApiSession::peerHost()` accessor added — the connectivity ingest handler needs the peer host to attribute a proxy report to a connection. Dismiss-not-owned now responds with a request-scoped `Error NOT_FOUND` (no disconnect), matching `notifications.proto`'s intent rather than the stricter default.
6. **Task 15 adaptations.** The slow-consumer backpressure test caps client A's receive buffer and floods ~5.8M send iterations to force the condition under loopback buffer physics — the config byte cap (`api.max_queue_bytes`) itself is untouched; the test asserts server-side cap enforcement then observes the client-side close. Peer-admission policy was pulled out as static seams `ApiServer::inApSubnet`/`ApiServer::peerAllowed` and unit-tested directly — the design didn't call for this but it was zero-coverage otherwise.
7. **`distanceMeters` promotion (Task 13) also fixed a second fake provider** (`tests/test_widget_instance_context.cpp`) that the plan didn't list as in-scope. Also noted: `navBridge`/`bluetoothManager` are non-nullable in this `main.cpp` — the plan's comments said "may be nullptr" for both, but only `projectionStatusProvider` actually can be.
8. **Companion gap review (mid-execution, Matthew decisions, commit `6068190`).** Theme/wallpaper transfer to a companion routes via a new web-config HTTP endpoint (separate work item, not the TCP/WS API); the API v1.1 additive batch (SystemStatus display dims, `TimeReport.timezone_id`, `ServerHello.server_id`) was approved as a post-plan follow-up; SOCKS5 password travels in-band, password-only (no separate derivation scheme). Already recorded in the roadmap/design doc/wishlist — referenced here, not duplicated.

**Test trajectory:** 91 baseline → 102 final (11 new test binaries/suites: `test_api_proto_roundtrip`, `test_api_framing`, `test_api_auth`, `test_api_pairing`, `test_api_serializers`, `test_api_transports`, `test_api_publishers`, `test_api_session`, `test_api_request_handlers`, `test_api_server`, `test_api_loopback`). All green throughout — every task TDD'd with a per-task review + fix loop; 2 fix commits total (`44e61cf` theme, `628af0a` connectivity fold), both folded into the deviation list above.

**Status:** Implementation complete, Tasks 1–16 done. Local and cross builds both green (see Verification). Not yet deployed to or exercised on Pi hardware — no client (web or TCP) has driven the server against a live AP connection.

**Next steps:**
1. Pi deploy + live check — deploy per the standard cross-build + rsync workflow, then exercise the server from a real client (web widget or a small TCP/WS test client) against the actual AP, not loopback.
2. JS-runtime implementation plan is now unblocked (JS design §9 was gated on API v1 landing) — pick it up next per the roadmap.
3. API v1.1 additive batch (SystemStatus display dims, `TimeReport.timezone_id`, `ServerHello.server_id`) — approved, not yet scheduled.
4. Web-config theme/wallpaper transfer HTTP endpoint — approved, separate work item from this API.
5. Companion app migration onto this API continues in the sibling companion repo.

**Post-review follow-ups (final whole-branch review, 2026-07-05):**
- Proxy-route teardown on companion-session disconnect (legacy parity: `CompanionListenerService` cleared `setProxyRoute(false)` on drop; API v1 `sessionClosed` does not — dormant while the companion dual-stacks on 9876; needs a small design decision on route ownership before the companion migrates fully).
- RNG hygiene pass: nonce/salt generation to `QRandomGenerator::system()` (`ApiSession` + `PairingManager`, 3 sites).
- `ApiServer::start()` double-invocation guard.
- `WsApiTransport`: `setMaxAllowedIncomingMessageSize(maxFrameBytes)` for TCP/WS symmetry.
- Loopback `init()` should wipe `kConfigPath` alongside `kStorePath`; alpha-drop comment at `buildSystemStatus` `.name()` call; `PhonePublisher` `startedAtMs` synthesis for calls already `Active` at construction.
- QML: gate `ApiSettings` pairing controls on a real `apiRunning` server property (Codex finding — PIN can be displayed while listeners are down if `start()` failed).

Codex review of PR #12 (2026-07-06): 3 majors fixed in this commit (TimeReport clock wiring, pairing persist-failure rejection, turn_side hybrid); QML gating deferred.

**Verification:**
- Local: `cd ~/builds/openauto-prodigy && cmake . && make -j$(nproc) && ctest --output-on-failure` → clean build, `100% tests passed, 0 tests failed out of 102` (26.49s).
- Cross-compile: `sg docker -c "./cross-build.sh"` → clean aarch64 build to `build-pi/src/openauto-prodigy` (only pre-existing benign `qt6_import_qml_plugins` warnings for unused QML plugins, not errors).

---

## 2026-07-02 — Paid-alternative parity roadmap, repo resync, parallel quick wins

**What changed:**
- Resynced this machine (MINIMEES/WSL) from 1407 commits behind origin; reset CRLF-noise working tree; moved orphaned `libs/aasdk/` leftover out of the repo
- Paid-alternative parity gap analysis written (kept in a gitignored private doc, out of the public repo); reference materials for the paid alternative cloned outside the repo (their GitHub repo is docs/proto/examples only, **no license → read for understanding, never copy**)
- Roadmap promotions (Later): HTML/JS extensibility (spike → runtime), prodigy-private external API (TCP+WS protobuf), multi-dashboard + overlay framework, local media player. Wishlist: FM radio (deferred), companion notifications, key bindings
- Extensibility plan audited against source: **fully implemented** despite stale NOT STARTED header (EventBus, ActionRegistry, NotificationService, PluginViewHost, lifecycle, contract docs all exist with the plan's tests) — archived to `docs/plans/` with corrected header, along with the completed proto-migration plans
- Web config panel diagnosed: **code is healthy** (all pages/endpoints verified with mock IpcServer + graceful degradation without Qt app). Root cause was deployment — installers enabled but never started the service → both installers now `systemctl enable --now`
- `BluetoothManager::refreshPairedDevices()` now logs "Found N paired device(s)" only when the count changes (info level, new `lastPairedCount_` member); NavStrip QML warnings confirmed obsolete (NavStrip deleted in v0.4.5)
- WebEngine spike pre-check: `qml6-module-qtwebengine` 6.8.2+dfsg-4 exists in Trixie arm64 — packaging gate passes, spike is purely memory/perf on the Pi

**Decisions (Matthew):** interleave parity work with v0.7.0 kiosk milestone; API stays prodigy-private; HTML/JS is a primary future feature path; FM radio deferred; **GSD workflow dropped — use superpowers skills for process**

**Status (updated same day):** MINIMEES WSL is now a full build env — Debian Trixie distro, system Qt 6.8.2 + all deps + `libbluetooth-dev` (was missing from docs, now added), Docker with the `openauto-cross-pi4` image prebuilt. **Build green, 88/88 tests pass** (includes the BluetoothManager change). The old Ubuntu VM ("claude-dev", aqt `/opt/qt`) is retired.

**Verification still needed (Pi only):**
1. On Pi: `sudo systemctl start openauto-prodigy-web` → panel should load at `:8080` immediately (or `journalctl -u openauto-prodigy-web` if flask is missing on old installs)
2. Startup logs: exactly one "Found N paired device(s)" line, again only on pair/unpair

---

## 2026-07-05 — Fable Sprint Session 3: Phase D2 (HFP call audio) + Phase E (dashboards/overlays) + Executor Handbook

**What changed (all docs — design only, no implementation, per sprint program):**
- Phase D2: [`specs/2026-07-05-hfp-call-audio-design.md`](superpowers/specs/2026-07-05-hfp-call-audio-design.md) + [`plans/2026-07-05-hfp-call-audio-implementation.md`](superpowers/plans/2026-07-05-hfp-call-audio-implementation.md) (9 tasks). Key decisions: SCO audio routes via WirePlumber natively (NOT through AudioService's 3-stream model — that model is AA-session-scoped and never coexists with SCO); TelephonyClient (session-bus D-Bus adapter) + ScoNodeMonitor (PipeWire node-state watch) feed a normative call state machine in PhoneStateService (§5 table); `phone.reject_sco_during_aa` defaults **false** (HFP owns call audio always, commercial-HU behavior) pending live check L4; provider command invokables return bool (API FAILED-vs-OK truth); BluetoothManager's dead AG/HSP registration deletion verified safe (`updateConnectedDevice()` at BluetoothManager.cpp:736 already covers auto-connect cancel — `profileNewConnection` wiring was fully redundant).
- Phase E: [`specs/2026-07-05-dashboards-overlays-design.md`](superpowers/specs/2026-07-05-dashboards-overlays-design.md) + two plans: [`multi-dashboards`](superpowers/plans/2026-07-05-multi-dashboards-implementation.md) (7 tasks: YAML v4 `dashboards[]` + v3 migration, DashboardManager with context-property re-pointing, WebWidget kind, switcher pills, picker size presets) and [`overlay-framework`](superpowers/plans/2026-07-05-overlay-framework-implementation.md) (4 tasks: OverlayService with z-bands + action auto-registration, OverlayHost as root-Repeater, PairingDialog migration proof).
- Cross-cutting: [`plans/README-executor-handbook.md`](superpowers/plans/README-executor-handbook.md) — pickup workflow, verification gate, guardrails, cross-plan dependency map.

**Bugs found during substrate reading (fixes are plan tasks, not applied):**
- `IncomingCallOverlay.qml:11` triggers on `callState === 2` = provider **Active**, not Ringing (comment claims Ringing; numbering confusion with PhonePlugin's private enum). Fix in HFP plan Task 7.
- Shell z-stack violates the arch §6 band contract: IncomingCall (z:1000) above Gesture (999); Pairing/NotificationArea/dim collide at 998. Re-pin is overlay plan Task 2.

**Status:** Sprint phases A, B (proto frozen 875feaf), C1+C2, D1, D2, E, F ALL complete (F as light plans: [`plans/2026-07-05-phase-f-light-plans.md`](superpowers/plans/2026-07-05-phase-f-light-plans.md) — media player, EQ parity audit, 0x8012 experiment protocol, key-event nav sketch). NOT done: HFP live checks L1–L6 (self-contained in design doc §11, needs Pi + phone); JS-runtime impl plan (deliberately deferred until API v1 lands, JS design §9).

**Next steps:**
1. Execute plans per handbook dependency map (external-api-v1 and hfp-call-audio are the "Now" roadmap items).
2. Run HFP design doc §11 checklist L1–L6 when Matthew + phone are available (L4 decides the reject_sco default).
3. Sprint end: PR `fable-design-sprint` → main (ask Matthew before push).

**Verification:** docs-only session — no build/test cycle applicable. All commits on `fable-design-sprint`: 3762fd9 (D2 design), 5ff16e6 (D2 plan), 4560117 (E design), daee267 (E plans), plus this handoff.

---

## 2026-07-05 — HFP Call Audio (D2) — Implementation Execution (Fable, live hardware)

**Branch:** `hfp-call-audio` off `fable-design-sprint` (10 commits). NOT merged to fable-design-sprint. Sprint-docs PR to main still pending Matthew's go-ahead.

**What changed — all 9 plan tasks executed, TDD per task, 91/91 tests green:**
- **T1** `refactor(bt)`: deleted dead HFP AG/HSP profile registration from BluetoothManager (`BluezProfile1Handler`, `registerProfiles`/`unregisterProfiles`, `profileNewConnection`) — 129 lines. Verified redundant (`updateConnectedDevice()` already cancels auto-connect). Kills the boot-order race + "UUID already registered" spam.
- **T2** `feat(phone)`: `TelephonyClient` — session-bus D-Bus adapter to `org.pipewire.Telephony` (AG/transport/Call1 tracking, Dial/Answer/HangupAll/SendTones, RejectSCO).
- **T3** `feat(audio)`: `ScoNodeMonitor` — PipeWire registry watch for `headset-audio-gateway` node RUNNING state; `AudioService::pwThreadLoop()/pwCore()` accessors.
- **T4** `feat(phone)`: widened `ICallStateProvider` (Dialing/Alerting/Held/Waiting, bool commands), added `dial/sendDtmf/telephonyAvailable` to `IPhoneStateService`, implemented the §5 state machine in `PhoneStateService`; `phone.reject_sco_during_aa` + `phone.settle_grace_ms` config keys. 12 new state-machine tests. Mock mode preserved.
- **T5** `feat(phone)`: `CallAudioPolicy` — RejectSCO follows projection state.
- **T6** `feat(phone)`: wired TelephonyClient/ScoNodeMonitor/CallAudioPolicy in main.cpp.
- **T7** `refactor(phone)`: PhonePlugin gutted to a pure view over `IPhoneStateService` (−363 lines of duplicate BlueZ); fixed role-inverted header comment; fixed `IncomingCallOverlay.qml` triggering on Active (`=== 2`) instead of Ringing (`=== 1`).
- **T8**: API integration — `src/core/api/` is empty (only `.gitkeep`), so took the "API lands later" branch; note left above for the API executor.
- **T9**: final gate + Pi deploy + live checks (below).

**Deviations from the design (all recorded in the design doc in place, with evidence):**
1. **[live-found implementation bug, then fixed]** `InterfacesAdded/Removed` for Call1 are emitted by a **per-AG ObjectManager** (`/org/pipewire/Telephony/ag1`), not the root — subscription changed to service-wide (empty path). Design §4.1 amended.
2. **[live-found, THE fix that made calls work]** QtDBus silently drops `InterfacesAdded` when the slot takes `QVariantMap`: the payload is `a{sa{sv}}`, which needs a registered metatype. Added `oap::InterfaceMap` (`QMap<QString,QVariantMap>`) + `Q_DECLARE_METATYPE` + `qDBusRegisterMetaType`. Codex independently confirmed root cause; also added explicit wire-signatures + logged connect() bools as hardening. (Codex also flagged the BlueZ `InterfacesAdded` handler in PhoneStateService uses the same bad signature but is masked because it ignores its payload and re-reads via QDBusInterface — pre-existing, not touched.)
3. **[from L2 evidence]** New §5 row: setup + Call1 `State→"disconnected"` → Idle immediately (skip Settling). Live-verified reject path.
4. **[from L1 evidence]** Transport `Codec` is a **byte** (1=CVSD/2=mSBC/3=LC3-SWB), not a string. `TelephonyClient` maps it.

**Live checks L1–L6 (Pixel 8, Pi at 192.168.1.149 — DHCP drift from .152; results in design §11):**
- **L1 topology: DONE.** Transport iface shares the ag1 object; codec is a byte; Address empty in the InterfacesAdded payload (present via introspection).
- **L2 Call1 sequence: DONE.** Call1 State→"active" fires before InterfacesRemoved → clean path is PRIMARY; Call1 persists through the call on Pixel 8 (softer than D1's ephemerality claim). Reject emits "disconnected". Transport "active" during ring = in-band ringtone.
- **L3 DTMF: NOT RUN** (pre-empted by L6). SendTones wired + unit-tested; AG method confirmed in L1.
- **L4 RejectSCO under AA: PARTIAL.** Baseline (`false`) works, downlink audio over SCO during AA, no obvious video stutter (not rigorously timed). Reject-true half not exercised. **Verdict: keep default `false`** — no evidence to flip.
- **L5 interop: NOT RUN** — only Pixel 8 available (confirmed working). Samsung/Moto pending.
- **L6 audio quality: PARTIAL — one OPEN interop bug.** Downlink (phone→car) WORKS. **Uplink (car mic→far end) SILENT at far end.** Exhaustively isolated: mic hardware good (ALSA 19% FS, works on Windows), PipeWire captures it (18% FS during call), graph correct+live (mic→`bluez_output` linked, Running, vol 1.0, LC3-SWB). Audio reaches the SCO uplink node correctly but the far end hears nothing → **platform-level PipeWire/BlueZ SCO uplink-encode or phone-decode issue, NOT a prodigy defect** (likely LC3-SWB-specific; D1 never verified far-end uplink audio, so latent since D1). mSBC-force diagnostic was INCONCLUSIVE (`bluez5.codecs` drop-in didn't gate HFP SWB; removed, Pi restored clean).

**State machine works end-to-end (journal-verified):** `Call setup: incoming +1512…` → `call state Idle→Ringing` → (answer) `Ringing→Active` → (hangup) `Active→Idle`; reject → `Ringing→Idle` immediately.

**Next steps (open items, none block the D2 code which is complete + verified):**
1. **L6 mic uplink** — find the correct PipeWire/WirePlumber HFP-codec property to force mSBC/CVSD (NOT `bluez5.codecs`); if mSBC uplink is audible → LC3-SWB PipeWire/BlueZ bug, file upstream + pin fallback. Or capture BlueZ SCO debug logs.
2. **Persistent in-call control** (→ wishlist) — no hangup affordance once a call is Active and you're off the Phone view; belongs in the Phase E overlay framework, bound to `callState == Active`.
3. **L3/L4-reject/L5** — remaining live checks (DTMF via IVR, RejectSCO=true comparison, Samsung/Moto interop).
4. Wire API Tasks 7/11 to the widened provider when API v1 lands (T8 note above).

**Verification:** local `cmake && make && ctest` = 91/91 across every task; `./cross-build.sh` succeeded each deploy; deployed to Pi (.149), service active, journal clean (no "UUID already registered"). Builds moved to an ext4 out-of-source dir (`~/builds/openauto-prodigy`) with ccache — WSL drvfs relinking was the bottleneck; source tree untouched.

## 2026-07-06 — Overlay Framework (Phase E) — Implementation Execution (Fable)

**Branch:** `overlay-framework` off main (3aeb683). 5 code commits, ready to merge (final review verdict: Yes) — push/PR awaiting Matthew's go-ahead.

**What changed — all 4 plan tasks executed (subagent-driven development, per-task review + final whole-branch review):**
- **T1** `feat(ui)` (3bdec23 + fix 579a388): `OverlayService` — QAbstractListModel overlay registry with fixed z-bands (Notifications=1000/User=2000/SystemModal=3000/Gesture=4000), auto-registered `overlay.<id>.show/hide/toggle/move` actions (rail R4), 10 unit test slots.
- **T2** `feat(ui)` (0ec9096): `OverlayHost.qml` — root-Repeater of lazy Loaders in Shell (root IS the Repeater so delegate z competes in Shell's stacking context); re-pinned all five legacy overlays to band constants — **fixes the IncomingCall(1000)-above-Gesture(999) inversion** and the 998 three-way collision. Dim fixture at 3500.
- **T3** `refactor(ui)` (340e645): PairingDialog rides the framework (descriptor `pairing`, SystemModal, visibility synced from `BluetoothManager::pairingActiveChanged` through the action path); removed from Shell; `IHostContext::overlayService()` exposed to plugins (set before plugin init).
- **Final-review fix wave** (e6baa49): **pairing-state authority guard** — any `overlay.pairing.show` while pairing is inactive (e.g. from a paired External-API client) is immediately corrected via `overlay.pairing.hide`; without this, a buggy client could raise an unrecoverable full-screen modal. Reentrancy contract (setVisible mutates before emitting → corrective dispatch terminates) pinned by `testReentrantCorrectiveHide`. Plus SystemModal tie-rule comment in Shell + `id: overlayHost`.

**Deviations from the plan (all uphold plan constraints over its example code, recorded per handbook):**
1. Plan's verbatim intra-band z-count had an unregister→reregister collision — replaced with band-bounded renormalization (z = band base + index among live same-band entries; z can never leave its band). Regression-tested.
2. `OverlayService::actions_` is `QPointer` (QObject children destroy in construction order; ActionRegistry dies before OverlayService — raw pointer in the dtor would dangle).
3. `bluetoothManager->isPairingActive()` (real getter; plan said `pairingActive()`).
4. Plan's `grep ": public IHostContext"` missed the namespaced `oap::IHostContext` mocks in test_plugin_manager/test_plugin_model — both gained the nullptr override.
5. OverlayHost geometry documented as all-or-nothing (x/y-only maps self-anchor; position ignored) rather than changing plan behavior.

**Tracked follow-ups (final review, none merge-gating):** migrate IncomingCallOverlay into the framework BEFORE registering any second SystemModal overlay (z-3001 would silently invert the tie — comment in Shell.qml records this); PluginManager sweep of overlays by `sourcePluginId` on plugin shutdown; `move` geometry validation/clamp (with the API-visible x/y-only wart, same fix site); `overlay.pairing.hide`-while-active is benign divergence (client dismiss until next edge) — revisit at incoming-call migration; 3-entry mid-band renorm test case.

**Status:** Complete and verified on WSL. **Pi deploy + on-device pairing flow NOT run** (hardware gate — Matthew wasn't around); that's the immediate post-merge step. Multi-dashboards plan (independent per handbook) starting next this session.

**Verification:** per-task `make` + full ctest at every step (102 baseline → 103 with test_overlay_service; final 103/103, 23.9s); offscreen runtime proof of the framework path (show→Loader-instantiates-PairingDialog→hide, dispatch true/visible true, zero QML/Loader/ReferenceError output; task-3-report §Runtime Framework-Path Evidence); `./cross-build.sh` clean at both 340e645 and e6baa49.

## 2026-07-06 — Multi-Dashboards (Phase E) — Implementation Execution (Fable)

**Branch:** `multi-dashboards` off main (3aeb683), independent of the sibling `overlay-framework` branch (same session, also ready). 10 code commits + handoff. Final review verdict: **Ready to merge** (contingent cross-build confirmed clean). Push/PR awaiting Matthew.

**What changed — plan tasks 1-6 executed (subagent-driven development, per-task review + fable whole-branch review):**
- **T1** `feat(config)` (8f7e0c5): widget_grid **v4 `dashboards[]`** + idempotent v3→v4 migration (merge-then-migrate; yaml-cpp node aliasing verified sound); flat v3 accessors deleted; grid coverage from test_yaml_config AND test_widget_config ported to v4.
- **T2** `feat(ui)` (03c81ba + fix 34a4271): **DashboardManager** — per-dashboard (WidgetGridModel + WidgetContextFactory) pairs, two-phase load-before-connect (spec §6.4), home seeding parity with the old main.cpp block, slug ids, cap 8, home/last-remove refusal, **debounced active-id persist on nav** (750ms, saveAll cancels, quit-flush) vs immediate saveAll on edits.
- **T3** `feat(widgets)` (ae47dfc): WebWidget contribution kind appended (order frozen), picker filter extended.
- **T4** `feat(ui)` (97eda5b): main.cpp rewired — call-time `activeModel()` resolution everywhere (no cached pointers), `app.dashboard.next/previous/select` actions, WidgetGridModel/WidgetContextFactory context properties re-pointed on switch.
- **T5** `feat(ui)` (49dcbbd + fix 5415d10): **DashboardSwitcher** edit-mode pills + modal manage sheet (add/rename/remove). Fix wave: wasHeld guard, deselect-on-switch, sibling-pattern modal Dialog, idle-timer guard, touchMin targets.
- **T6** `feat(ui)` (3c0584e + fix a90486e): picker size presets (roles 265-268, preset popup). Implementer caught the brief's popup-behind-Overlay compositing bug; fix wave added full dismissal hygiene (dual reset + tap-outside cancel).
- **Final-review fix wave** (1762349 + comment fix 9beb74d): **[CRITICAL] ~DashboardManager UAF on normal quit** — config shared_ptr ownership + aboutToQuit flush (the debounce flush could write through a freed YamlConfig into config.yaml); outgoing-model selection clear on switch (stale widgetSelected latch deferred grid remaps forever); configSheet/emptySpaceMenu close on API-driven switch (cross-dashboard setWidgetConfig collision); **migration gate widened** (flat-shape placements trigger the wrap regardless of pre-v4 version — v2/versionless flat configs no longer lose placements).

**Deviations from the plan (recorded per handbook, all reviewer-verified):** plan fixture used nonexistent saved_cols/saved_rows keys (production: grid_cols/grid_rows — test fixed); test_widget_config.cpp was an unlisted flat-accessor consumer (ported, fixture v2→3); nav persistence debounced (plan had full saveAll per switch); DashboardManager ctor takes shared_ptr<YamlConfig> (UAF fix — mandated test text adapted); sizePopup nested in Dialog contentItem (brief's placement rendered behind the modal).

**Immediate fast-follow (filed, not in this branch):** atomic YamlConfig::save (temp+fsync+rename) + try/catch on load with corrupt-file fallback — pre-existing, but all dashboards now live in one file and the debounce moves writes closer to ignition-off.

**Track-after bundle (final review Minors 6-14 + carried a-e, none merge-gating):** activeIndex NOTIFY on non-active removal; saveAll coalescing on dims fan-out; API rate-limiting for action dispatch (switch spam = Pi jank, no state-authority violation possible); duplicate display names from add-chip; pill-row overflow at 8 long names; loadFromConfig re-entry guard; empty-id dashboard qWarning; on-device rename keyboard check (shared with EQ preset naming); test hygiene (temp files, saveAll-cancels-pending interleave).

**Merge sequencing vs `overlay-framework`:** main.cpp hunks disjoint (near-clean auto-merge either order); CMakeLists both append (trivial adjacent conflicts, keep both). Whichever merges second re-runs the full gate. Semantic note filed in wishlist: QQuickOverlay modals composite above ALL Shell z-bands including incoming-call — pre-existing pattern, needs a doc rule or modal migration.

**Status:** Complete and verified on WSL. **Pi deploy + first-boot v3→v4 migration check NOT run** (hardware gate). The final review's on-device checklist for that pass: back up the Pi config first; verify version 4 + single home dashboard carrying ALL pre-migration placements + flat keys gone; reboot twice (idempotence, no reseed); add dashboard → place widget → reboot → both restored; kill -9 within 750ms of a pill switch → config intact.

**Verification:** per-task make + full ctest (102 baseline → 103 with test_dashboard_manager; controller-verified 103/103 at final HEAD, 24.1s); offscreen boot smokes at every integration task (incl. config-intact check at T4); `./cross-build.sh` clean at a90486e and 1762349.

## 2026-07-06 — Companion Support Batch (API v1.1 + hardening + atomic config) — Implementation Execution (Fable)

**Branch:** `develop` direct (per the single-develop-branch workflow), 4 commits off 2f17911: 7c86c1d / 5403d6f / dd2c68f / 079243f. No plan doc — task briefs written from the API v1 handoff + design docs (subagent-driven development, per-task review + fable whole-branch review).

**What changed:**
- **A** `feat(api)` (7c86c1d): **v1.1 additive batch** exactly per design §16 — `SystemStatus.display_width/height` (fields 6/7, sourced from `DisplayInfo` via new `ApiServiceRefs.display`, republished on `windowSizeChanged`), `ServerHello.server_id` (field 8, config-persisted UUID minted on first API start; new `identity.server_id` default), `TimeReport.timezone_id` (field 2, IANA-validated, applied via `timedatectl set-timezone` only when different from the current zone), `api_version_minor` 0→1. All proto3 `optional`, reserved ranges shrunk exactly, feature-detect by presence (no capability flags). 8 new tests across 4 suites.
- **B** `fix(api)` (5403d6f): **hardening docket** — WS pre-buffer cap (`setMaxAllowedIncomingMessageSize` in `WsApiTransport` ctor; app-level check kept as defense in depth), **all FOUR** `QRandomGenerator::global()` sites in `src/core/api/` → `::system()` (handoff said 3; the pairing PIN was the 4th), `ApiServer::start()` `started_` guard (idempotent re-entry, disabled path leaves retry possible, `stop()` clears). Double-start test asserts the duplicate-publisher symptom directly (true RED: one delta per update).
- **C** `feat(config)` (dd2c68f) + fix wave (079243f): **atomic `YamlConfig::save`** — serialize (now inside try/catch) → `<path>.tmp` → POSIX write (short-write+EINTR loop) → fsync → close → rename; `bool` return, never throws, temp unlinked on every failure branch. **`load()` never throws** — corrupt file renamed aside to `<path>.corrupt` (overwrite policy) + full defaults; missing file silent. Closes the multi-dashboards fast-follow (debounced persist near ignition-off + corrupt-config boot crash). 5 new tests.

**Deviations (all disclosed + reviewer-verified):** `DisplayInfo` has a single `windowSizeChanged` signal (brief guessed per-axis); RNG sites were 4 not 3; implementer added defensive `root_ = defaults;` in `load()`'s catch (verified beneficial — covers migrate-throws after merge; `mergeYaml` clones its base); WS oversized-message test cannot RED against the pre-buffer cap (old post-buffer check yields the same observable) — disclosed in-test.

**Final review (fable): "With fixes" → fix wave 079243f (yaml emission try/catch upholding the never-throws contract + explicit `<cstdio>`) → no Critical/Important remaining.**

**Tracked follow-ups (final-review triage, none merge-gating):** server_id mint ignores persist failure (atomic save now qWarns every branch, so no longer silent; residual = fresh UUID per boot on read-only FS); serverId stability test covers the in-memory layer only; failed-start retry path re-news listeners AND appends a duplicate publisher set (unreachable in prod — main.cpp calls start() once); legacy `CompanionListenerService` has the same 3 `global()` RNG sites (fix alongside or retire with 9876); rename-aside also eats an intact-but-unreadable (EACCES) config (arguably correct); `timedatectl` handlers (clock-step + new timezone) block the main thread up to 5s each — convert BOTH to async QProcess in one pass; save() write/fsync/close/rename failure branches need fault injection to test; `write()==0` not special-cased (theoretical).

**Proxy-route teardown — DECIDED + APPLIED (same session):** Matthew approved "route ownership follows the reporting session". Implemented as `44f1262` (task-reviewed, approved, zero findings above Minor): `ApiRequestHandlers::connectivityOwner_` — an active ConnectivityReport claims ownership; an inactive report or the owner's `sessionClosed` releases it; owner close emits `setConnectivity(false)` through the existing `proxyRouteChanged` wiring → `setProxyRoute(false)`. Legacy `clearClientSession()` parity restored for graceful drops; 4 new tests (owner-close clears / non-owner leaves / inactive releases / new-owner takeover); reviewer verified server-stop teardown is safe (single emit, no double-teardown) and no destruction path can dangle the owner pointer. The vanished-phone case (zombie session — no TCP FIN on a local AP) is deliberately NOT covered here; daemon-side auto-teardown after failed health checks is filed in the wishlist as the follow-up net. Also in wishlist from this session: legacy CompanionListenerService RNG sites, ApiServer failed-start retry hygiene, async timedatectl handlers.

**JS-runtime implementation plan authored** (stretch item): `docs/superpowers/plans/2026-07-06-js-runtime-implementation.md` — 9 tasks against the C2 design + delivered API v1.1 state (WebWidget enum/picker filter already exist from multi-dashboards; localhost sessions trusted; WS = bare ApiMessage frames). Design §6.4 desktop dev-auth branch deferred (recorded in the plan header). Plan only — no implementation.

**Status:** Complete, verified on WSL, **DEPLOYED TO PI (same session, 2026-07-06 ~22:16 CDT)**. Pi checkout moved from `main` @ 88a20c8 to **`develop` @ 90a118a** (per the single-develop workflow); binary rsync'd from the 44f1262 cross-build; service active, journal error-free. Verified on-device: `identity.server_id` minted once (`0470f673-8085-479c-9b2e-84989ca2ef6e`) and **stable across a second restart**; a hand-rolled loopback TCP client received `ServerHello{name="OpenAuto Prodigy", major=1, minor=1, server_id=<same>}` — the first time a real client has driven the API on hardware. Config backed up pre-deploy to `~/.openauto/config.yaml.pre-deploy-backup`. The corrupt-config recovery drill was NOT run on the live device (unit-tested; optional on-device check remains available).

**Deploy incident — the blackhole was LIVE:** the Pi had a stale `OPENAUTO_PROXY` iptables REDIRECT + running redsocks pointing at a long-gone phone proxy — all internet-bound TCP dead (curl → 000) while LAN worked; NTP (UDP) kept the clock fine, masking it. Exactly the failure mode the owner-teardown fix addresses, pre-dating the fix. Cleared cleanly via the system daemon (`set_proxy_route {active:false}` over `/run/openauto/system.sock`, newline-JSON) → curl 200. This is hard field evidence for the wishlisted daemon-side auto-teardown watchdog: the route had been silently killing head-unit internet for an unknown period.

**Verification:** per-task make + full ctest at every step (104/104 throughout — no new test binaries, 19 new test functions inside existing suites incl. Task D's 4); `sg docker -c ./cross-build.sh` clean at dd2c68f, 079243f, and 44f1262.

## 2026-07-07 — JS-Runtime / Web Widgets (Phase C2) — Implementation Execution (Fable)

**Branch:** `develop` direct (single-develop-branch workflow), 12 commits bc8051e → 24f900a: 41c4189 / c9a2ef6 / 37bd150 / 9009496 / b49aaf8 / 2b66be5 / 229d1ee / 0c1704e / 93ab619 / 9f0a446 / 8e9aa5a / 24f900a. Plan: `docs/superpowers/plans/2026-07-06-js-runtime-implementation.md` (9 tasks, subagent-driven development, per-task review + fable whole-branch review). Design §6.4 desktop dev-auth stayed DEFERRED per the plan header.

**What shipped (all 9 tasks):**
- **T1** (41c4189): optional `HAS_WEBENGINE` gate (mirrors HAS_BLUETOOTH), `prodigy://` scheme registered before QGuiApplication, installer + prebuilt packages, **cross-build Docker image rebuilt with qt6-webengine-dev:arm64** — config log prints "WebEngineQuick found", aarch64 binary links libQt6WebEngineQuick.
- **T2** (c9a2ef6 + fix 37bd150): `WebWidgetManifest` widget.yaml parser. Review found 2 real defects in the plan's own code (lib-verified): scalar `size:` threw and discarded the whole manifest (fixed: IsMap guard → defaults) and the id regex `$` accepted trailing-newline ids (fixed: `\z` anchor). Controller decision per project precedent — fixes uphold the plan's stated URL-safety/graceful-fallback intent.
- **T3** (9009496): `WebWidgetContentResolver` (canonical-path jail; reviewer verified sibling-prefix/dir-self/symlink/traversal/percent-encoding angles against real Qt) + thin `WebWidgetSchemeHandler` shell (gated).
- **T4** (b49aaf8): `WebWidgetScanner` → WidgetRegistry + main.cpp wiring (profile handler install + scan, ordering verified vs QML engine construction). Settled the URL question: **`qrc:/OpenAutoProdigy/WebWidgetHost.qml`** (built rcc uses basename-only aliases; the design doc's `qml/widgets/` path form was wrong).
- **T5** (2b66be5): `ThemeService::themeTokenMap()` — 42-token vocabulary moved verbatim from ApiSerializers (single source; wire parity reviewer-traced; serializer + QML bootstrap both consume).
- **T6** (229d1ee): protobufjs toolchain — `tools/gen-proto-js.sh` (**deviation: `-p proto`** — protos import `api/...` relative to `proto/` root; brief's literal command ENOENTs), committed `prodigy-proto.js` (pbjs static-module, root `prodigy-api`) + vendored `protobuf.min.js` v7.6.5 minimal (sets `window.protobuf` unconditionally — byte-verified). qrc: `qrc:/web/*.js`.
- **T7** (0c1704e): `prodigy.js` shim — exact R3 surface, all 7 wire-fact groups verified against proto (ClientKind top-level enum, request_id uint64 = the two brick-risks), reconnect/correlation traced correct (monotonic IDs + pending sweep).
- **T8** (93ab619): `WebWidgetHost.qml` — lazy latch (D4), crash recovery 2s/4s/8s + error card (D5), lockdown incl. **fullScreenSupportEnabled: false (design-mandated; plan sample omitted it)**, same-origin nav policy, 4 user scripts at DocumentCreation/MainWorld, span push via `_updateContext`. All 5 flagged API shapes verified against qmltypes/source before writing.
- **T9** (8e9aa5a + fix 9f0a446): hello-theme reference package, docs/development.md + INDEX.md, **WSLg live check (adapted: config-preseed instead of picker clicks) that caught two real integration bugs pre-hardware**: WebWidgetHost.qml was the only widget QML missing its `set_source_files_properties` alias block (scanner URL unresolvable → placement load failed + teardown SIGSEGV), and prodigy.js threw at DocumentCreation (`documentElement` null → `window.prodigy` never defined; fixed with a MutationObserver that still themes before first paint, D6 preserved). Live evidence: "Registered 1 web widget(s)", prodigy:-scheme renderer, ESTABLISHED loopback WS on 9811, zero scheme/404/JS errors, config restored md5-identical.

**Design erratum (Matthew veto point):** final review found design §3's mandated scheme flags (`SecureScheme | LocalAccessAllowed`) contradict §7's security model — `LocalAccessAllowed` let widget pages load `file:`/`qrc:` subresources, bypassing the resolver jail, and nothing in the shipped architecture needs it (injected scripts are host-read; ws:// rides SecureScheme + loopback). Dropped in 24f900a with a live re-check gate: full pipeline still works, Chromium-level proof `prodigy:hsL` → `prodigy:hs`. Erratum noted in design §3; re-adding the flag is a one-liner if vetoed.

**Final review (fable): "With fixes" → 24f900a → closing verdict READY TO MERGE, zero Critical/Important remaining.** Cross-task seams all verified (token chain end-to-end incl. example HTML, URL/alias chain, injection contract, wire fields, frozen surfaces byte-identical, scan-once thread-reachability airtight, build matrix consistent).

**Tracked follow-ups:** wishlist "From JS-runtime execution (2026-07-07)" — shim v1 hardening block, widget-author known-limitations doc, api.enabled zombie widgets, field-debugging logging, hygiene batch (resolver freeze comment, URL latch, entry `..` over-reject, gen-script comment+pins), persistent-profile decision.

**Pi checklist (needs Matthew at the touchscreen; deploy + headless items done this session — see Status):**
1. Add "Hello Theme" from the widget picker → themed card renders (CSS vars pre-connect), status flips to "connected as org.openauto.example.hello-theme".
2. BT playback → media title/artist update live; Play/Pause button dispatches (action round-trip).
3. Crash recovery: `pkill -x QtWebEngineProcess` on the Pi → card auto-reloads within ~4s, launcher unaffected. Also note behavior if the widget's page is not the current dashboard page (crash-reload-while-hidden is expected/accepted).
4. Day/night flip in settings → card re-colors within a frame (themechange path).
5. Resize the widget in edit mode → spans update (contextchange; devtools or temporary on-page readout).
6. Renderer/memory: expect a second renderer+WS from the initial about:blank document (it also runs the DocumentCreation scripts — final-review explanation; QML double-instantiation ruled out). Confirm the about:blank WS/renderer tears down rather than leaking; QtWebEngineProcess PSS within the spike budget (≤350 MB total).
7. Confirm a widget with an https subresource loads it (nav policy must not block subresources) — optional, needs a test widget with an https img.
8. Narrowed scheme flags (24f900a) re-confirmed on-device implicitly by items 1-2.

**Status:** Complete and verified on WSL (107/107 per task and at head; WSLg live checks ×2 — Task 9 + flag-fix). Cross-builds clean at 41c4189, 8e9aa5a; 24f900a cross-build + Pi deploy recorded below when done.

**Verification:** TDD per task (RED evidence in .superpowers/sdd reports); per-task reviews (sonnet/opus) all approved, two with fix waves (T2 manifest hardening, final-review flag drop); suite 104 → 107 binaries (manifest/resolver/scanner), 108th not added (QML host is Pi-checklist by design §9).

**Deploy record (same session, 2026-07-07 ~11:16 CDT):** develop pushed @ ee50dc3; cross-build at 24f900a clean ("WebEngineQuick found"); binary rsync'd; Pi pulled ee50dc3; `qml6-module-qtwebengine`/`libqt6webenginequick6` 6.8.2 confirmed present (spike install); config backed up to `~/.openauto/config.yaml.pre-webwidget-deploy-backup`; hello-theme scp'd to `~/.openauto/webwidgets/`. Post-restart: **journal shows "Registered 1 web widget(s)"**, pre-flight 4/4, API listening on *:9810/*:9811, NRestarts=0, AA reconnected and hardware-decoding — service fully healthy. Touchscreen checklist above (picker add → themed card → connected-as → media/dispatch → crash-kill → day/night → span → renderer/memory) is the only remaining verification and needs Matthew in the car/at the bench.

**Touchscreen checklist results + fix wave (same day, 2026-07-07 afternoon):** Items 1 (picker/theming), 2 (Play/Pause dispatch), 4 (day/night) PASS. Item 3: `pkill -f QtWebEngineProcess` kills the 3 zygote fork-servers too — no recovery possible (beyond D5's renderer-crash scope); controller re-test killing ONLY a renderer PID on the Pi: recovery WORKS (fresh renderer + WS reconnect ~15s). **Corrected crash drill: `kill -KILL $(pgrep -f "type=renderer" | head -1)`** — never pkill (-x can't match >15 chars; -f kills zygotes). Item 6: renderers 44+41 MB PSS, app 293 MB with AA streaming — in budget. Item 5 FAILED and was root-caused: the selection long-press detector is a z:-1 MouseArea behind widget content; a WebEngineView consumes every touch, so web widgets had no edit-mode entry. **Double-WS root cause found — the final review's about:blank hypothesis was DISPROVEN by live instrumentation** (about: documents never connect): the real bug is SwipeView page pre-rendering instantiating the full unfiltered widget repeater per page — the `isCurrentPage` binding was true in every page-Loader copy, so a foreign invisible delegate also latched a second WebEngineView (renderer + WS). Fix wave (Matthew-approved, opus-reviewed "Ready to deploy", zero Critical/Important): `4ffdd32` prodigy.js bails in `about:` documents (defense-in-depth, kept narrow so the deferred desktop-dev story survives); `e72d979` passive-grab TapHandler (500ms) gated by a new `isWebWidgetHost` marker — gives web widgets the selection entry without stealing the view's touches, natives zero-change; `346c33d` `isCurrentPage` gated on page-Loader ownership — foreign pre-render copies permanently dormant; decisive WSLg evidence: 1 WS + 1 renderer at the reproducing `page_count: 2` (was 2+2). On-device re-check for Matthew: long-press a web widget → selects (then drag/resize via the existing handles); `ss -tn | grep 9811` → one connection; eyeball the minor web-vs-native divergence (long-press another widget while one is selected switches selection instead of deselecting). Wishlist: per-page filtered widget model (foreign copies still instantiate as cheap QML items).

**Edit-mode entry saga — RESOLVED on-device (2026-07-07 afternoon, develop @ d5088f7):** the TapHandler entry (e72d979) never fired on real touch (WebEngineView takes the exclusive grab; DragThreshold TapHandler cancels), and its replacement PointHandler (a417380) SIGSEGV'd the app on every long-press (3/3, silent UI-process death). Codex read-only investigation ranked the cause: the 500ms timer mutated the scene (scale/lift/selectWidget/interceptor-enable) MID-touch-stream while the view owned the exclusive grab; runner-up = Qt 6.8.2 passive-grab bookkeeping over WebEngineView. Fix (d5088f7, Codex-recommended shape, opus-reviewed "Ready to deploy"): NO Qt pointer handler — `resources/web/host-gestures.js` (5th injected script, observe-only capture pointer listeners, no preventDefault) detects ≥500ms/<12px holds and on pointer-UP navigates to sentinel `prodigy://host/longpress`; WebWidgetHost intercepts it in onNavigationRequested (IgnoreRequest, before the same-origin guard) and emits `longPressed()`; HomeMenu Connections (ignoreUnknownSignals — natives unaffected, verified across all 11 registry widgets) runs the guarded select-flash strictly AFTER the touch stream ends (renderer→browser IPC round-trip = structural crash immunity; worst case is silent no-gesture). Also added `onTouchSelectionMenuRequested: accepted=true` (suppress text-selection menus in widgets). **Matthew confirmed on the touchscreen: long-press (hold, then lift) selects; drag/resize work; page buttons unaffected.** Deliberate UX divergence: web widgets select on finger-lift after the hold, natives at the 500ms mark. Never re-try Qt pointer handlers over WebEngineView — the QML comments + this entry record why. Checklist final: items 1-6 PASS (3 with the corrected renderer-PID drill; 6 = 1 WS/1 renderer), 7 n/a (no https-subresource widget yet), 8 was a note. **Deploy speed note:** targeted app-only cross-build (`cmake --build . --target openauto-prodigy` in the container against the warm build-pi cache) took ~4 min vs ~20 for the full build — scripted adoption in cross-build.sh is wishlisted.

## 2026-07-08 — SESSION WRAP: media player stage 1 bench-ready — Handoff to fresh session

**State:** develop @ d25430d, 23 commits ahead of origin, NOT pushed (push gates on
bench pass). Pi @ 192.168.1.149 runs the CURRENT build (includes launcher widget,
all final-review fixes): service active, clean journal. Fixtures at ~/Music/fixtures/
(two 0.5s test tones — real music recommended for the audible rows).

**Immediate next step (Matthew + fresh session):** the 12-row bench checklist in the
2026-07-08 deploy entry above, PLUS row 13 (start local while AA music playing —
expect local audible, AA muted at HU mixer) and row 11 addendum (corrupt restored
track at boot → silence + paused UI). First: add the Media Player launcher tile via
dashboard edit mode → widget picker (picker-visible, not seeded — bench finding #1:
apps launch ONLY via dashboard launcher widgets; the plan had assumed the v0.4.5-deleted
nav strip; CLAUDE.md corrected).

**After bench passes:** push develop, then plan stage 2 (library scanner + udisks2
automount). Stage-2 planning inputs are in .superpowers/sdd/progress.md (machine-local)
and the final-review triage: shared-EQ-engine coexistence, main-thread PCM watch,
plugin-ABI policy, restorePaused unit test, fmtTime hours, case-insensitive sort
fixture, AA focus-push bench experiment (Task 11 sketch). Lesson recorded: verify
launch/UX surfaces against the live shell, not doc phrasing.

## 2026-07-08 — Media Player Stage 1: final review + fix batch + redeploy — Arc execution complete (bench pending)

Final whole-branch review (a85096d..e758cf4, 18 commits): **With fixes** — zero Critical.
Fix batch landed + verified (suite 114/114): Local Media stream priority 51 (focus tie
vs AA Media at 50 would have MUTED the newest player by creation order — display/audio
contradiction; spec §13.2 was assigned to no task, now resolved this way), restore-error
no-autoplay guard (`restoring_` flag — a corrupt restored track at boot now stays
stopped instead of auto-skipping into audible playback), CLAUDE.md deploy note corrected
(QML ships IN-BINARY via qmlcache; git pull alone will NOT update the UI), `.superpowers/`
now in tracked .gitignore. Redeployed to Pi @ fcfff3c: active, NRestarts=0, plugin
initialized, zero QML/binding-loop warnings.

**Bench checklist status:** rows await Matthew (see 2026-07-08 deploy entry above for
the 12-row table). **NEW ROW 13:** start local playback while AA music is playing —
expected: local audible + AA music muted at the HU mixer (phone still thinks it's
playing until paused phone-side; the unprompted focus-push to fix that properly is
investigated + sketched, deferred — see Task 11 entry + wishlist). Row 11 addendum:
also test the corrupt-restored-track case (replace the saved track's file with garbage,
restart service — expect silence, warning log, paused UI).

**Not pushed** — push gates on Matthew's bench pass per workflow. Stage 2 planning
(library scanner + udisks2 automount) starts after bench. Stage-2 planning inputs
recorded in the SDD ledger: shared-EQ-engine coexistence, main-thread PCM path watch
(bufferMs hardcoded 50), plugin-ABI policy for provider widening (apiVersion unbumped),
restorePaused unit test, fmtTime hour rollover, case-insensitive sort fixture.

## 2026-07-08 — AA Audio-Focus Push Investigation (Task 11) — Investigation

**Scope:** Spec §6 plan-verify item (b) — can the HU tell an AA-playing phone to
pause its media when local (BT/MediaPlayer) playback starts? This is the reverse of
Task 8(d)'s pause-others policy (landed, `src/main.cpp:696-711`), which only pauses
LOCAL playback when AA reports playing. Grep-and-read investigation against
`libs/prodigy-oaa-protocol/` (read-only submodule, untouched) and `src/core/aa/`; no
Pi/phone bench available this session, so nothing was wired in per controller
constraint — investigation + sketch only. `src/` and `libs/` are unmodified.

**Findings:** The phone→HU direction (`AudioFocusRequest`, control channel msg
`0x0012`) is fully handled with a grant-all policy in
`libs/prodigy-oaa-protocol/src/Session/AASession.cpp:64-93` — every request gets an
immediate `AudioFocusResponse` (msg `0x0013`) plus an `audioFocusChanged(int)` signal
that `AndroidAutoOrchestrator.cpp:369-393` uses to duck/gain local PipeWire streams.
The reverse (HU-initiated, unprompted) direction has the wire plumbing already
in place — `ControlChannel::sendAudioFocusResponse(QByteArray)`
(`ControlChannel.hpp:27` / `.cpp:229`) is a generic, non-request-gated send of msg
`0x0013`, and `AASession::controlChannel()` is a public accessor — but **zero
call sites exist for it** outside the request-driven reply in `AASession.cpp:89`.
No orchestrator-level "push a focus state to the phone now" method exists in `src/`.

**Verdict: implementable, ~20 lines** (a `notifyAudioFocusLoss()`/
`notifyAudioFocusGain()` pair on `AndroidAutoOrchestrator`, reusing the existing
`session_->controlChannel()->sendAudioFocusResponse()` + `AudioFocusResponse`/
`AudioFocusState` proto types with zero protocol-library changes) — clears the
brief's ≤30-line bar comfortably. **Deferred, not wired in**, because the decision
gate's second condition (same-day bench test on Pi + phone) can't be satisfied
without hardware access this session. Full ready-to-apply code sketch (header +
cpp + the `main.cpp` Step-3 wiring, unchanged from the brief) is in
`.superpowers/sdd/task-11-report.md`, ready for a future bench session to apply and
test directly — no further investigation needed first.

**Risks flagged for that bench session:** whether the phone actually honors an
*unprompted* `AudioFocusResponse` (the proto's gold-confidence trace covers the
*reply* path, not an unsolicited push — inferred but not proven); per-phone-model
variance (test at least Moto G Play 2024 + Samsung S25 Ultra, both already
characterized for other AA quirks); session-wedge safety around AA
connect/disconnect races; correct trigger timing for `notifyAudioFocusGain()` on
local stop (avoid ping-pong on pause/resume scrubbing); avoid clobbering an
in-flight phone-side `GAIN_TRANSIENT`/`GAIN_TRANSIENT_MAY_DUCK` (nav-guidance duck)
with an unconditional `LOSS` push. Full detail in the report.

**Wishlist:** "AA focus push-to-phone for local playback coexistence" entry added,
pointing at the report. Shipping stage 1 without this remains explicitly acceptable
per spec §6 ("annoying, not broken").

**Session wrap (2026-07-07 ~15:30 CDT, context limit):** Post-checklist all green (see entries above; Pi verified end-to-end). Started two follow-on streams, both handed to next session: (1) **web-widget quality mini-batch** — Task QA (cross-build fast app-only default + --full flag) was mid-flight at wrap (subagent commits autonomously; verify + review + push next session); QB shim-hardening trio / QC logging / QD widget-author-limitations doc not yet dispatched (briefs from the wishlist JS-runtime section; ledger has the task letters). (2) **Theme-upload endpoint brainstorm** — exploration complete, notes at `docs/superpowers/specs/2026-07-07-theme-upload-context-notes.md` (open questions Q1-Q4 inside; Q1 auth posture was asked and not yet answered). EQ design sprint remains the next big roadmap item after these.

## 2026-07-09 — Tiered workflow adoption + Codex pre-push gate shakedown — Complete, push pending go-ahead

**What changed:**
- Adopted concepts from the `fabletieredworkflow` review repo into the superpowers loop (spec: `docs/superpowers/specs/2026-07-09-tiered-execution-codex-gate-design.md`): plan-time tier tags (`opus`/`sonnet`/`main`) + Definition of Ready, model-pinned dispatch, Opus→Codex(GPT-5.5)→Fable escalation ladder, per-feature pre-push Codex review gate. Deliberately NOT adopted: handoffs/ dir, RUN-STATE.md, /tier command, autonomous architect (duplicate ceremony).
- New `scripts/codex-review.sh` (read-only sandbox, stdin prompt + `-o` verdict, exit contract 0/1/2/4, artifacts in gitignored `reviews/`), TDD'd against a 10-check fake-codex harness. Workflow documented in AGENTS.md §Tiered Execution Workflow; CLAUDE.md pointer.
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
