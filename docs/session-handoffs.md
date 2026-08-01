# Session Handoffs

Newest entries first.

---

## 2026-07-31 — Native dashboard PR and Opus pre-merge review

**What changed:** pushed the completed `dev` branch and opened draft PR #43,
`Add native Android Auto turn card and richer trip data`, against `main`. The
PR contains the complete hardware-accepted dashboard selector, native turn-card
foundation, and richer trip-data work. No application code changed after the
accepted and reviewed head `f8f1759c9f99756c6a9ddef39b7adc2cb7ede45c`.

**Status:** PR #43 is open and mergeable but remains a draft; it has not been
merged. One read-only Opus 4.6 review covered the immutable 26-commit
`origin/main..f8f1759` range and returned **MERGE** with no blocker and one new
minor. The confirmed minor is roadless guidance coupling the cue/time row to
road-text visibility; it is recorded in `docs/engineering-backlog.md` rather
than churning the hardware-accepted implementation.

**Verification:** the PR head retained the previously green focused, native,
explicit app, complete offscreen CTest, ARM cross-build, exact Pi hash, and
sequential live acceptance evidence. This docs-only review record passed
`python3 scripts/check-doc-links.py --scope tracked-live` and
`git diff --check`; no application rebuild was required.

**Next 1–3 steps:** push this docs-only review record to PR #43; merge only on
the user's authorization; re-research the roadless-guidance presentation lead
before promoting it into implementation scope.

---

## 2026-07-31 — Native dashboard richer-trip hardware acceptance

**What changed:** completed the live-proven richer native turn card at accepted
code SHA `e26e9406084f4162d7e4f3e39ee99a07967bebbc`. Exact modern navigation
snapshots now drive rerouting freshness, ordered distinct action cues,
next-step timing, and index-zero destination distance/ETA with
single-destination remaining duration. Live lanes replace the full trip footer;
otherwise its adaptive metrics share the accepted fixed-height destination
band and overflow-only address marquee. The older flat turn event remains the
compatibility fallback, and the hands-off OAA v1.5 proto tree was not edited.

**Status:** COMPLETED and HARDWARE ACCEPTED. The deployed ARM executable matched
locally and on the Pi at SHA-256
`7ca081c83d10a3d0de0908070af89b6822ca6d57b6b1d02ea83907cd5a686bec`,
and the Prodigy service was active. The completed design and plan are archived
at
`docs/archive/plans/2026-07-31-native-dashboard-richer-trip-data-design.md`
and
`docs/archive/plans/2026-07-31-native-dashboard-richer-trip-data-plan.md`.

**Verification:** the three focused navigation suites, native build, explicit
`openauto-prodigy` app target, complete offscreen CTest suite, and
`./cross-build.sh` passed. The one-screen Pi/Samsung run passed the projected
Map baseline, immediate Map→Turn with uninterrupted audio, no-lane adaptive
trip data and marquee, lane-footer replacement without pulsing, rerouting
placeholder and fresh restoration, route-end clearing, and immediate Map
restoration. Documentation closure passed
`python3 scripts/check-doc-links.py --scope tracked-live` and
`git diff --check`; no application rebuild was required for the docs-only
closure commit.

**Review:** the single high-effort Fable pass reviewed immutable range
`e70a012e7f2ff171f96954fd8d6e0e387ec9a670..e26e9406084f4162d7e4f3e39ee99a07967bebbc`
with the accepted anchor and returned BLOCKER=0, MAJOR=1, MINOR=3. All four
facts were confirmed, none dismissed, and three deferred: notification-order
NOTIFY mismatch, footer boundary-width fit math, and pre-existing navigation
EventBus connection accumulation. The fourth finding was accepted as intended:
removing derived shadow signals restores inherited `INavigationProvider`
notifications and External API v1 navigation pushes without changing the
payload; freshness-gated rerouting clears stale route fields by design. No
remediation review was needed.

**Next 1–3 steps:** publish only with the user's authorization; re-research the
three deferred findings before promotion; retain multi-stop, roundabout,
other-provider lookahead, and current-road questions as unclaimed validation.

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

**Historical status (superseded by the acceptance above):** DESIGN APPROVED;
PLAN WRITTEN; READY TO EXECUTE. The design was grounded on `7c47172` and is now
archived at
`docs/archive/plans/2026-07-31-native-dashboard-richer-trip-data-design.md`;
the implementation plan was grounded on `e70a012` and is now archived at
`docs/archive/plans/2026-07-31-native-dashboard-richer-trip-data-plan.md`.
They kept the accepted maneuver/lane hierarchy and made live lanes replace the
complete trip footer. Current road, Maps lookahead, roundabout presentation,
EV data, and ambiguous multi-stop numeric duration remained excluded.

**Evidence:** navigation-only capture SHA-256
`61a145a0ba3a3c2612007215e78d8b92cdca2b3885236815311a16a0e6262f7e`;
external Maps research response SHA-256
`cc49376c83e1575527a66216ef945fa820734a7274397c47c988736383d46c7d`.
The capture setting was returned to disabled and broader temporary protocol
traffic was removed after the navigation artifact was verified.

**Verification:** documentation links and whitespace are checked on the final
docs tree. No application build is required for this documentation-only step.

**Next 1–3 steps (historical):** execute handler snapshot tests and
implementation; execute provider freshness/presentation tests; complete QML,
native/ARM, live hardware, and the one bounded Fable review gate. All were
completed by the acceptance entry above.

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
