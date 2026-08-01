# Session Handoffs

Newest entries first.

---

## 2026-07-31 — Native dashboard richer-trip design

**What changed:** promoted the live-proven richer native navigation data into
an implementation-ready design. One complete Samsung S25+ GAL 6.0 mock route
produced synchronized notification/position snapshots, exact rerouting edges,
ordered action cues, time to step, next-destination distance, formatted ETA,
and remaining duration. A focused Maps 26.30.05/Gearhead 17.3 source trace
established replacement-snapshot semantics, corrected the misleading local
road/cue field names, proved Maps current-step-only and current-road-absent
behavior, and established destination index-zero policy.

**Status:** DESIGN APPROVED; PLAN WRITTEN; READY TO EXECUTE. The design is
grounded on `7c47172` at
`docs/plans/2026-07-31-native-dashboard-richer-trip-data-design.md`; the
implementation plan is grounded on `e70a012` at
`docs/plans/2026-07-31-native-dashboard-richer-trip-data-plan.md`. They keep
the accepted maneuver/lane hierarchy and make live lanes replace the complete
trip footer. Current road, Maps lookahead, roundabout presentation, EV data,
and ambiguous multi-stop numeric duration remain excluded.

**Evidence:** navigation-only capture SHA-256
`61a145a0ba3a3c2612007215e78d8b92cdca2b3885236815311a16a0e6262f7e`;
external Maps research response SHA-256
`cc49376c83e1575527a66216ef945fa820734a7274397c47c988736383d46c7d`.
The capture setting was returned to disabled and broader temporary protocol
traffic was removed after the navigation artifact was verified.

**Verification:** documentation links and whitespace are checked on the final
docs tree. No application build is required for this documentation-only step.

**Next 1–3 steps:** execute handler snapshot tests and implementation; execute
provider freshness/presentation tests; complete QML, native/ARM, live hardware,
and the one bounded Fable review gate.

---

## 2026-07-31 — Native dashboard turn-card hardware acceptance

**What changed:** completed the native semantic dashboard turn card on the
existing AUXILIARY navigation-map connection. Local Map/Turn card switching no
longer reconnects AA. The accepted card has exhaustive maneuver/lane mappings,
compound physical-lane composition, shared hero/lane geometry, stable lane
updates, theme-aware in-dash typography, a lane-priority destination footer,
and the approved final hierarchy: wider label-free maneuver tile,
right-aligned distance block, centered road row, and whole-mile formatting
above 9.9 miles. The hands-off OAA v1.5 proto tree was not edited.

**Status:** COMPLETED and HARDWARE ACCEPTED at
`5a2b1b52864220e4b62e4efabdccc748ccd73ba0`. The deployed
`ALPHA-26-07-24-01-134-g5a2b1b5` ARM executable matched locally and remotely at
SHA-256 `bd75d15e350bb8c5b8ed7cc5aec349a956467a684ce1f42e7e978e5be313d13e`;
the service was active at PID 343022 with `NRestarts=0` and no QML startup
errors. The operator accepted the final live card at installed resolution and
viewing distance. The one-screen rig validated Map and card sequentially.

**Verification:** focused behavior was developed red-first. The final native
build, explicit `openauto-prodigy` app target, complete offscreen CTest suite,
ARM `./cross-build.sh`, exact binary-hash comparison, clean service restart,
and live Pi/phone route checks passed. Pixel and Samsung runs covered local
Map/card switching, maneuver and lane presentation, friendly states, audio,
route transitions, and readability. A Samsung S25+ stationary route repeatedly
proved destination-address delivery; trip time, ETA, and destination distance
remain gated because it did not deliver `NavigationNextTurnDistanceEvent`.

**Review:** the single high-effort Fable pass reviewed immutable range
`4439c29922971af9fbd3ce2bce9b15acab82f9be..5a2b1b52864220e4b62e4efabdccc748ccd73ba0`
with the accepted anchor. BLOCKER=0, MAJOR=0, MINOR=4. All four minor findings
were confirmed and deferred under the accepted-tree rule: projection enum
literals, delivery-dependent empty modern distance text, C-locale mile
parsing, and a possible transient null-provider QML warning. They are recorded
in `docs/engineering-backlog.md`; no remediation pass was needed.

**Next 1–3 steps:** capture moving-route trip-summary delivery before
promoting richer Stage 2 fields; write a focused plan only for live-proven
semantics; keep the accepted primary maneuver and lane hierarchy unchanged.

---

## 2026-07-28 — Native dashboard turn-card Stage 1 guidance

**What changed:** reconciled current Android Auto documentation with the Stage
1 native dashboard behavior. The auxiliary descriptor is now invariantly
`AUXILIARY`/`KEYCODE_NAVIGATION`; `video.secondary_display_content` immediately
selects local map or native turn-card presentation without reconnecting AA.
Map mode uses the live decoded map, while the native card keeps that decoder
live and renders NavigationProvider semantics with a continuous lane band.
Stage 2 data remains explicitly evidence-gated.

**Historical status (superseded by the 2026-07-31 acceptance above):**
DOCUMENTATION RECONCILED; Stage 1 then awaited hardware acceptance. The
runtime anchor at this checkpoint was
`8f2df93219dbfbb3c80823a0f3e32d513999b43f`.
The completed projected-dashboard fallback-wording item was removed from the
engineering backlog; the settings-striping and CLUSTER/AUXILIARY terminology
entries remain separate follow-up leads.

**Verification:** `cmake --build /home/matt/builds/openauto-prodigy --target
test_aa_cluster_widget -j$(nproc)` and `QT_QPA_PLATFORM=offscreen ctest
--test-dir /home/matt/builds/openauto-prodigy --output-on-failure -R
'^test_aa_cluster_widget$'` passed. The full native/app/CTest/ARM gate is run
from the final documentation commit before deployment or publication.

**Next 1–3 steps:** record the final gate artifact hash; obtain focused Stage
1 Pi acceptance; only propose Stage 2 from captured phone delivery evidence.

---

## 2026-07-28 — Native dashboard turn-card design

**What changed:** promoted and documented the approved staged replacement for
the phone-rendered dashboard turn card. The design keeps the phone auxiliary
provider on the accepted map path, makes `map`/`turn_card` an immediate local
presentation switch, and defines a theme-aware instrument card with explicit
1024x600 typography floors. Stage 1 now includes an intentional mapping for
every defined maneuver, transport and rendering for every lane shape, multiple
directions per lane, and a continuous roadway-style lane band with no
button-like cells. Stage 2 retains explicit rerouting, distinct road and
instruction text, roundabout detail, ETA/destination data, and lookahead.

**Historical status (superseded by the 2026-07-31 acceptance above):** DESIGN
APPROVED; STAGE 1 PLAN WRITTEN; AWAITING EXECUTION AUTHORIZATION. The design is
grounded on `15a45c1` and is archived at
`docs/archive/plans/2026-07-28-native-dashboard-turn-card-design.md`. The
executable Stage 1 plan is grounded on design commit `4439c29` and is archived
at `docs/archive/plans/2026-07-28-native-dashboard-turn-card-stage-1-plan.md`. It stops
after Stage 1 hardware acceptance and preserves Stage 2 as evidence-gated work.
No runtime, configuration, QML, protocol-library, or proto-submodule files
changed in this documentation step.

**Verification:** design and Stage 1 plan self-review found no placeholders,
contradictory requirements, unresolved choices, or Stage 2 scope leakage.
`python3 scripts/check-doc-links.py --scope tracked-live`, `git diff --check`,
and the final staged-diff whitespace check passed. No application build was
run for this documentation-only step.

**Next 1–3 steps:** review the Stage 1 plan with the user; execute it red-first
after authorization; write the Stage 2 plan only from recorded live-delivery
evidence.

---

## 2026-07-28 — Selectable projected dashboard navigation

**What changed:** hardware probe `76d631e` proved that AA accepts
AUXILIARY/NAVIGATION on the existing secondary display and renders the map.
Production code anchor `1b1086480608ca7a58dcbc1c36f18d8ca8f6afa9` adds the
durable `video.secondary_display_content` setting, a visible Map/Turn card
picker when the projected dashboard is enabled, connection-time
`KEYCODE_NAVIGATION`/`KEYCODE_TURN_CARD` descriptor selection, automatic AA
renegotiation on a real change, and a local fallback while a phone sends no
frames. The hands-off protocol submodule remains unchanged at OAA v1.5.

**Status:** COMPLETED and HARDWARE ACCEPTED. The Pi ran
`ALPHA-26-07-24-01-120-g1b10864`, executable SHA-256
`1b6b0ee2064e930a4253cf8c47e9b1224920e892f51a41f40714ed2b2f2f9f98`.
The operator independently confirmed Map mode, Turn card mode after the
automatic reconnect, the phone-rendered no-route Maps icon, its immediate
transition to a compact maneuver card when a route started, and restoration
to the full map after selecting Map again. Final runtime state was GAL 6.0,
720p, `secondary_display_content: map`, app PID 91361, and `NRestarts=0`.

**Verification:** focused tests were written red-first and passed. Then
`cmake --build /home/matt/builds/openauto-prodigy -j$(nproc)`,
`cmake --build /home/matt/builds/openauto-prodigy --target openauto-prodigy -j$(nproc)`,
`QT_QPA_PLATFORM=offscreen ctest --output-on-failure`, and
`./cross-build.sh` passed on the accepted code tree. The exact local/remote ARM
hashes matched. `python3 scripts/check-doc-links.py --scope tracked-live` and
`git diff --check` passed for documentation.

**Review:** the standard Opus pass reviewed immutable range
`fff66b261de74b6e96809e44c91c6a141e78cf2a..b08e0ccd844de523cfd5d4a1e1e03b02b3f4ca91`
with accepted anchor `1b10864`: BLOCKER=0, MAJOR=1, MINOR=2. All three findings
were confirmed but nonblocking and deferred under the accepted-tree rule: one
fallback-copy issue, one hidden-row striping issue, and one internal naming
cleanup. They are recorded in `docs/engineering-backlog.md`; no remediation
pass was needed.

**Evidence and rollback:** pre-probe accepted binary:
`/var/backups/openauto-prodigy/20260728-073214-pre-aux-navigation/openauto-prodigy`;
pre-production-selector probe binary:
`/var/backups/openauto-prodigy/20260728-074813-pre-dashboard-selector/openauto-prodigy`.

**Next 1–3 steps:** publish the completed branch only with user authorization;
select any deferred UI/naming cleanup as a separate follow-up feature; keep the
current phone-rendered idle surface unchanged.

---

## 2026-07-27 — GAL 6.0 compatibility program closure

**What changed:** Fable pass 2 reviewed immutable remediation range
`e28ef88eee1d7a0a9d9a200485a2c7b42c18d208..a59c0656d66b450646db6d26080c2bac0242db2f`
and returned BLOCKER=0, MAJOR=0, MINOR=0. Across both passes, pass 1 began at
BLOCKER=0, MAJOR=1, MINOR=3; one MAJOR and two MINOR findings were confirmed,
while one MINOR was deferred because the tested single-key string-preservation
case has no second real consumer. Both compatibility design/plan records are
now `COMPLETED 2026-07-27` and archived with reciprocal links intact.

**Status:** COMPLETED. The accepted production code anchor remains
`c362ac62df56a99f1509b872bd3d385f719c22cd`. The full native build, explicit
app target, offscreen CTest, and ARM cross-build gate passed on `e28ef88` before
the docs-only pass-1 remediation and this archival closure. No push, tag, or
release was performed.

**Verification:** `python3 scripts/check-doc-links.py --scope tracked-live` and
`git diff --check` passed. The corrected nested gitlink checks
`git -C libs/prodigy-oaa-protocol/proto rev-parse HEAD` and
`git -C libs/prodigy-oaa-protocol/proto status --short` confirmed the protocol
submodule clean at `5ff4aa218dd33913237993f2968bf70e16dc464e`.

**Next 1–3 steps:** inspect the closure commit in the primary session; keep any
push, tag, or release as separate user-authorized publication work; select the
next promoted roadmap priority separately.

---

## 2026-07-27 — GAL 6.0 Fable pass-1 documentation remediation

**What changed:** adjudicated the bounded Fable pass-1 review of immutable
range `25cd71f..e28ef88`. The initial result was BLOCKER=0, MAJOR=1, MINOR=3.
One MAJOR and two MINOR findings were confirmed: the stale plugin-reference
GAL list/default and codec-fallback documentation gap were corrected, while the
late-initial-BlueZ registration recovery gap was code-confirmed as nonblocking
and recorded only as an engineering-backlog lead. The remaining one MINOR—the
hard-coded `connection.gal_version` string-preservation key—is deferred with no
edit or backlog item: it is correct and tested for the only current
numeric-looking string consumer, and no second consumer justifies an
abstraction.

**Status:** PASS-1 FINDINGS REMEDIATED; AWAITING THE ONE ALLOWED REMEDIATION
REVIEW. Pass 1 reported no blocker. The accepted code anchor remains unchanged
at `c362ac62df56a99f1509b872bd3d385f719c22cd`; this is documentation-only
remediation pending Fable pass 2 in the primary session.

**Verification:** `python3 scripts/check-doc-links.py --scope tracked-live` and
`git diff --check` passed; the protocol submodule remained clean at
`5ff4aa218dd33913237993f2968bf70e16dc464e`.

**Next 1–3 steps:** inspect the bounded four-file diff in the primary session;
run the one allowed Fable remediation review without changing its immutable
feature base; adjudicate pass 2 before completing and archiving the active
plans.

---

## 2026-07-27 — GAL 6.0 live-document reconciliation

**What changed:** reconciled the eight live AA policy, rendering, protocol,
configuration, Settings, roadmap, index, and handoff documents against the
hardware-accepted production tree. Current guidance now describes durable
session-wide `connection.gal_version`, requested-version authority, the exact
1.7/4.3/5.0/5.1/6.0 list and thresholds, audio/video/AVInput flow-control
boundaries, diagnostic-only modern messages, and the accepted H.265-first
codec policy with H.264 fallback. Historical 4.3/5.0 defaults are explicitly
superseded rather than presented as live configuration.

**Status:** DOCUMENTATION RECONCILED; AWAITING FINAL VERIFICATION/REVIEW. The
active design and implementation plan remain active and in place until the
primary session completes the repository/ARM gate and bounded final review.

**Anchors:** planning base `25cd71f`; OAA v1.5 main/tag
`61eab61c5f9968154ff1a80faa8c0a427b208479`; exact dist/submodule
`5ff4aa218dd33913237993f2968bf70e16dc464e`; accepted GAL 5.0 `a2b8aa8`, GAL
5.1 `ce08f8f`, and GAL 6.0/H.265
`c362ac62df56a99f1509b872bd3d385f719c22cd`. The acceptance entries below
retain the evidence paths and artifact identities without duplicating their
raw logs. The one-screen visual limitation is preserved: independent ch3/ch12
counters prove concurrent streams, while MAIN and the homescreen CLUSTER
widget were inspected sequentially.

**Verification:** `python3 scripts/check-doc-links.py --scope tracked-live` and
`git diff --check` passed; the protocol submodule was clean at
`5ff4aa218dd33913237993f2968bf70e16dc464e`.

**Next 1–3 steps:** run the full native/app/CTest/ARM gate in the primary
session; run the one bounded final review against the immutable planning and
accepted anchors; only after a clear review, complete/archive the two active
plans and refresh their links.

---

## 2026-07-27 — GAL 6.0 H.265 hardware acceptance

**What changed:** Task 7 hardware-accepted implementation
`c362ac62df56a99f1509b872bd3d385f719c22cd` (`fix(aa): restore H.265
as the video default`) with the clean protocol gitlink
`5ff4aa218dd33913237993f2968bf70e16dc464e`. The Pi ran
`ALPHA-26-07-24-01-112-gc362ac6`; the deployed and running executable
SHA-256 was
`c5d1ca03b9ef609457cae88c752702203cb2f030a417ec5009f91b78d8e97d46`.

**Status:** ACCEPTED. The restored final configuration is GAL 6.0,
`[h265,h264]`, 720p, and 30 fps, SHA-256
`0e4008a0dd5217bc662c3b5ae2ce3f23b77f1700eb9f2dc79eb3df44f32f5073`.
The operator intentionally chose 720p because 480p placed Android Auto's
navigation chrome at the bottom rather than at the side. The final session
requested 6.0, received 6.0/MATCH, advertised one H.265 configuration for
each MAIN and CLUSTER display, and selected codec 7 on both.

**Hardware evidence:** both displays used FFmpeg `hevc` with a DRM hardware
context and the Pi V4L2 stateless request decoder at `/dev/video19`. The
decoder produced DMABuf-backed DRM_PRIME format 178 frames and logged hardware
first frames at 1280x720 MAIN and 800x480 CLUSTER without a software fallback.
The one physical screen cannot show both surfaces at once: protocol capture
proved concurrent independent ch3/ch12 receive and video-ACK traffic, while
the operator correctly checked MAIN projection and the homescreen CLUSTER
widget sequentially.

**Live matrix:** independent Pixel 8 checks passed MAIN and CLUSTER visual
health, music, navigation start/change/stop with duck/recovery, two Assistant
cycles, MAIN touch plus Back/Home/TEL/direct-dialer return, and focus
exit/return. Three manual Pixel reconnects passed without restarting the app.
The second project phone was operator-identified as an S25+ and exposed by
BlueZ as `MATTHEW's S25 Ultra`; its initial connection/media/input smoke and
Bluetooth-off/Bluetooth-on wireless BT discovery -> WiFi -> TCP reconnect
passed. Explicit Pixel 1.7, 4.3, 5.0, and 5.1 connection/media smokes passed
before the final 6.0 restoration. GAL 1.7/4.3 retained per-frame audio and
video ACKs; GAL 5.0/5.1/6.0 emitted no audio ACKs while retaining video ACK
parity. The explicit GAL 6.0 H.264 fallback remains proven on `2bc574e`.

**Evidence limits and diagnosis:** system audio opened and completed setup but
the phone did not deliver its conditional stream, matching the accepted GAL
5.1 run; synthetic coverage remains the evidence. Optional `0x8014` and
`0x8008` were not delivered live and retain typed synthetic coverage. The
initial S25 pairing was non-seamless and the operator restarted Prodigy once.
The old process received no S25 pairing, RFCOMM, WiFi, or TCP attempt, while
its listeners had just passed the Pixel reconnects; the new process then
handled a new S25 pairing through the normal wireless chain. This is a
nonblocking single-active-phone/startup-transition observation, not evidence
of a supported TCP/session defect. Debug Settings' separate automatic-decoder
label defect is recorded in the engineering backlog; runtime HEVC hardware use
was conclusive.

**Verification:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc)`,
`cmake --build . --target openauto-prodigy -j$(nproc)`,
`QT_QPA_PLATFORM=offscreen ctest --output-on-failure`, and
`./cross-build.sh` passed before deployment. Final state retained app PID
32716 with the one operator-requested restart (`NRestarts=1`, active since
2026-07-27 19:35:58 CDT), BlueZ PID 4413 with `NRestarts=0` since 15:28:23
CDT, and boot ID `bf4f1de6-5201-480f-beab-00b91fc2f4a0`.

**Evidence:** H.264 and H.265 captures are under
`/home/matt/gal-6-0-captures-2026-07-27/20260727T222347Z-2bc574e/`;
the accepted GAL 5.1 rollback is
`/var/backups/openauto-prodigy/20260727T222245Z-pre-2bc574e-gal-6-0`.

**Next 1–3 steps:** perform Task 8 documentation reconciliation and its one
bounded final gate; re-research the decoder-label backlog item separately if
selected. Do not change the hardware-accepted Task 7 tree while closing docs.

---

## 2026-07-27 — GAL 5.1 hardware acceptance

**What changed:** Task 5 accepted implementation HEAD
`ce08f8ff1f057746007a33eae1209df222c2123e`. The Pi ran
`ALPHA-26-07-24-01-109-gce08f8f`, ARM executable SHA-256
`77aa748005212dcb16c1c2d8db9db98c94c9683000c590eff17a18cc25a90409`.

**Historical status (superseded by GAL 6.0 above):** ACCEPTED. GAL negotiation
requested 5.1 and received 6.0/MATCH, with one H.264 configuration per display
and concurrent independent MAIN+CLUSTER streams confirmed by their counters.
On the one-screen rig, the two surfaces were inspected sequentially. Active
ch4 emitted zero audio ACKs while MAIN and CLUSTER video ACKs advanced. The
full operator live matrix passed, as did three manual reconnect cycles, an
explicit GAL 5.0 regression, and the final GAL 5.1 restoration.
Messages `0x8014` and `0x8008` were not delivered during the live run; their
synthetic coverage passed, so this conditional non-delivery is nonblocking.

**Verification:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc)`,
`cmake --build . --target openauto-prodigy -j$(nproc)`,
`QT_QPA_PLATFORM=offscreen ctest --output-on-failure`, and
`./cross-build.sh` passed. Final state retained app PID 12397 with
`NRestarts=0`, boot ID `bf4f1de6-5201-480f-beab-00b91fc2f4a0`, matching
deployed/runtime executable hashes, and explicit GAL 5.1 persisted. BlueZ
retained PID 4413, `NRestarts=0`, and its 2026-07-27 15:28:23 CDT start time.

**Evidence:** capture
`/home/matt/gal-5-1-captures-2026-07-27/20260727T213912Z-ce08f8f`;
rollback
`/var/backups/openauto-prodigy/20260727T213826Z-pre-ce08f8f-gal-5-1`.

**Historical next step (completed above):** Task 6 was GAL 6 implementation.

---

## 2026-07-27 — GAL 5.0 hardware acceptance

**What changed:** this historical implementation and hardware checkpoint
completed at `a2b8aa8`, with the released protocol pin `5ff4aa2`. GAL became a
durable, session-wide Android Auto setting independent of the CLUSTER lab;
selectable 1.7/4.3/5.0 behavior became available and 5.0 was the accepted
default at this checkpoint. The later entries above supersede that default.

**Historical status (superseded by GAL 5.1/6.0 above):** ACCEPTED. The Pi runs
`ALPHA-26-07-24-01-105-ga2b8aa8`, executable SHA-256
`8ad6da18072ec97af00f8b8272ab99aaa137b0d909cd1367f593a9336e6cb30f`.
A BlueZ service restart rebuilt discovery and AA reconnected automatically
without restarting Prodigy; PID 4171 and `NRestarts=0` stayed unchanged. Three
consecutive manual phone disconnect/reconnect returns also passed. The operator
confirmed ch4/ch5/ch6 audio roles, Assistant mic/response, MAIN touch plus
Back/Home/TEL direct dialer, and exit/return. Independent counters confirmed
concurrent MAIN+CLUSTER video; their surfaces were inspected sequentially on
the one-screen rig.

**Verification:** `cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc)`,
`cmake --build . --target openauto-prodigy -j$(nproc)`,
`QT_QPA_PLATFORM=offscreen ctest --output-on-failure`, and
`./cross-build.sh` passed. The 4.3 regression restored two MAIN codecs and
audio ACKs. Final 5.0 restoration requested 5.0, received 6.0/MATCH, advertised
one H.264 configuration per display, emitted zero audio ACKs on active ch4,
and continued advancing MAIN and CLUSTER video ACKs.

**Evidence:** captures are in
`/home/matt/gal-5-0-captures-2026-07-27-bluez-recovery/20260727T202704Z-a2b8aa8`;
rollback is
`/var/backups/openauto-prodigy/20260727T202704Z-pre-a2b8aa8-bluez-recovery`.
Intermittent brief audio skips correlated with phone/UI activity and were also
seen at GAL 1.7; current evidence does not support a GAL 5.0 regression, a
blocker, or a Pi-side fix claim.

**Historical next step (completed above):** Task 4 was GAL 5.1 typed tolerance.

---
