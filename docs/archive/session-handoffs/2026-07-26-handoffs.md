# Session Handoffs

Newest entries first.

---

## 2026-07-26 — GAL 5.0–6.0 production compatibility planning

**What changed:** added an ACTIVE design and staged implementation plan for
restoring the released OAA consumer boundary, moving GAL selection out of the
CLUSTER lab, and promoting evidence-backed GAL 5.0, 5.1, and 6.0 support. The
roadmap and index now identify this as the current promoted work.

**Why:** open-android-auto `v1.5` now publishes the audited schemas through
minimal `dist` commit `5ff4aa218dd33913237993f2968bf70e16dc464e` from tagged
main `61eab61c5f9968154ff1a80faa8c0a427b208479`. Prodigy's current temporary
main pin is descriptor-identical, so the dependency boundary can be corrected
before adding higher-version behavior. The user also clarified that accepted
higher GALs must become production capabilities and the highest accepted
version—not GAL 1.7—must be the final default.

**Status:** PLANNING COMPLETE; implementation and new hardware validation have
not started. The plan requires sequential 5.0, 5.1, and 6.0 hardware-accepted
SHAs. Final scope ends at GAL 6.0 and explicitly excludes 6.1, semantic use of
unresolved MediaOptions, EV UI, outgoing media stats, integrated overlays,
AUXILIARY, and third-display work.

**Verification:** the OAA remote refs were fetched explicitly: annotated tag
`v1.5` dereferences to `61eab61c`, remote `dist` is `5ff4aa2`, and its tree has
248 protos plus README and LICENSE. Planning-only verification is the tracked
live doc-link check and `git diff --check`; application/ARM/hardware gates
begin with Task 0.

**Next 1–3 steps:** (1) commit the approved planning baseline and record its
SHA; (2) execute Task 0's exact `v1.5` `dist` gitlink update; (3) extract the
session-wide policy and production 4.3 default before implementing GAL 5.0.

---

## 2026-07-26 — GAL 4.3 display compatibility COMPLETE

**What changed:** the completed GAL 4.3 design and implementation plan moved to
`docs/archive/plans/` with `Status: COMPLETED 2026-07-26`. The roadmap now
records the shipped default-off 4.3 lab under Done, the index points only to
the archived artifacts, the resolved GAL-obligation backlog item is removed,
and three evidence-led review follow-ups are recorded without widening the
completed phase.

**Why:** the exact post-bench tree and accepted Pi/Pixel behavior were green,
and the single required major review found no supported-production blocker.
The active artifacts therefore no longer represent pending execution guidance.

**Review gate:** Fable pass 1 reviewed
`ba63f9faf9bed0455c875bd0f0c20273429e339a..8ecc514ef1e145bde4b2d6a108012314d369a9d7`
and reported `BLOCKER=0`, `MAJOR=0`, `MINOR=3`. Adjudication totals are
confirmed=3, dismissed=0, deferred=3. The confirmed minors are: possible
warning/`unknownMessage` noise for audited AV IDs with no production consumer
or repeated accepted-live occurrence; missing metric/imperial hardware
comparison for the audited navigation distance-unit suffix map; and a
diagnostic-only chance parse of opaque version-response trailing bytes. All
three are nonblocking research under the accepted-tree rule and are recorded
in `docs/engineering-backlog.md`. No remediation review or source churn was
run.

**Status:** COMPLETE. Accepted hardware code remains
`d06fa40a2d9141f4a62155ce75e3bb3d2d2550f3`; the Pi remains on
`ALPHA-26-07-24-01-97-gd06fa40`, executable SHA-256
`4b73d69a40f7e5be4701f1775af53a11fa1d3a7863cb0ddb3d379afad0fded48`,
GAL 4.3/native false. Task 7C is documentation-only, so it does not require a
build, cross-build, deployment, or hardware rerun. ADB/logcat unavailability
remains a documented evidence-source limitation, not an unresolved defect.

**Verification:** on the final Task 7C documentation tree,
`python3 scripts/check-doc-links.py --scope tracked-live` reports no broken
links, `git diff --check` passes, and the hands-off proto worktree remains clean
at `cabe46ec9c5e1628264427aa77d910b1f574bb34`. Both archived artifacts carry
the completed status, and no live document links point to their former
`docs/plans/` paths.

**Next 1–3 steps:** (1) re-research the three deferred backlog leads only if
fresh live evidence promotes them; (2) continue routine hardware regression
coverage; (3) select a separately promoted roadmap item before starting new
feature work. No current execution plan remains.

---

## 2026-07-26 — GAL 4.3 final hardware evidence and Task 7A record

**What changed:** current guidance now records the corrected modern descriptor
contract: whenever GAL 4.3 creates field 11, field-1 display insets accompany it
and mirror the unchanged legacy total margins. MAIN retains total margins
0x58 and uses top/bottom 29, left/right 0 plus CLOCK; native-true CLUSTER
retains totals 500x180 and uses left/right 250, top/bottom 90 plus enum 5.
Fields 2–4 and 6–8 remain absent. GAL 1.7, Navbar-off, and native-false paths
remain field-11-free. The resolved validation blocker was cleared, and the
2026-07-25 handoffs were rotated verbatim.

**Why:** direct operator evidence disproved the hidden-only field-11 premise.
AA selects side versus bottom chrome from the usable aspect ratio. Candidate
46c5be9 produced bottom chrome at y=489..599; d06fa40 companion insets restored
side chrome and the exact 1.7 Navbar boundary at y=541..599. Field 1 accompanies
the legacy margin fields; it does not replace them.

**Status:** hardware validation is complete and the active plan is review
pending. The full chain covers the Task 0 1.7 baseline, request-only
`9501bbef239f86301536c7445e090a34f2c77203` 4.3 → 6.0/MATCH checkpoint,
failing `46c5be998470188c1ef62ac048db5b831d0d753f` field-11 matrix, and the
`d06fa40a2d9141f4a62155ce75e3bb3d2d2550f3` A/C/D/E remediation rerun.
Task 0 used repository HEAD
`ba63f9faf9bed0455c875bd0f0c20273429e339a` and deployed SHA-256
`6a8a6573ba72b366b87234b5a6e692c61ba3df4b6c77f606dc6fa72abd637e6e`;
request-only used `ALPHA-26-07-24-01-91-g9501bbe` / SHA-256
`df2b6b475be4d9ddc99ba1e6e7773a279dcbb37351e7fcf412b3bb6e3d6071c1`;
and the failing candidate used `ALPHA-26-07-24-01-95-g46c5be9` / SHA-256
`9b6ed4e3d65c1dc8ed9d44819734cc80e39eaccb1f853ae24d2c75fa9e61081b`.
CLUSTER false, true, and
restored false retained identical 364x364 dashboard geometry; the phone
maneuver banner was
present, absent with enum 5, then present again with field 11 absent. The Pi is
healthy on ALPHA-26-07-24-01-97-gd06fa40, executable SHA-256
4b73d69a40f7e5be4701f1775af53a11fa1d3a7863cb0ddb3d379afad0fded48,
GAL 4.3/native false, with unchanged config SHA-256
afd7f1a8cdb1bd2e067563bf16361965a59b841ad767edd05fea9b5f024985a7.
Rollback is `/var/backups/openauto-prodigy/20260726T185152Z-pre-companion-inset`.
Evidence is retained beside the repository in
`gal-4-3-captures-2026-07-26`,
`gal-4-3-remediation-captures-2026-07-26`,
`gal-final-matrix-captures-2026-07-26`, and
`gal-companion-inset-remediation-captures-2026-07-26`. ADB/logcat was
unavailable for the final rerun; that is an evidence-source limitation, not an
unresolved defect.

**Verification:** live evidence includes raw version exchange, descriptor
goldens, service/channel/media/ACK and resource logs, direct MAIN screenshots,
and direct CLUSTER false/true/restored-false screenshots. The exact
post-bench documentation commit must pass the full native build, explicit
openauto-prodigy target, offscreen CTest, ./cross-build.sh, tracked-live
documentation link check, git diff --check, and clean proto-worktree check
before the one bounded major review. Exact post-commit results and ARM identity
are recorded in the Task 7A execution report.

**Next 1–3 steps:** (1) run the single Codex-authored major review on the exact
Task 7A head; (2) adjudicate within the two-pass limit and rerun only affected
gates; (3) complete and archive the ACTIVE design/plan in Task 7C after no
supported-production blocker remains.

---

## 2026-07-26 — Task 6 final GAL matrix blocked before deployment

**What/why:** The final candidate `46c5be998470188c1ef62ac048db5b831d0d753f`
was checked for the requested Pi/Pixel display matrix. Its local aarch64
artifact is `ALPHA-26-07-24-01-95-g46c5be9`, SHA-256
`9b6ed4e3d65c1dc8ed9d44819734cc80e39eaccb1f853ae24d2c75fa9e61081b`.

**Status:** BLOCKED before deployment. `adb devices -l` returned an empty
device list, so the required phone logcat/provider evidence and route-active /
route-inactive screenshots for cases C–E cannot be collected. The task brief
forbids inferring those results from descriptors or Pi logs. No Pi artifact,
configuration, rollback snapshot, source, or earlier capture was changed.

**Current safe state:** The Pi remains on the known-good checkpoint binary
`df2b6b475be4d9ddc99ba1e6e7773a279dcbb37351e7fcf412b3bb6e3d6071c1`
with config `afd7f1a8cdb1bd2e067563bf16361965a59b841ad767edd05fea9b5f024985a7`;
`openauto-prodigy.service` is active/running with `MainPID=78782`,
`NRestarts=0`, exact executable
`/home/matt/openauto-prodigy/build/src/openauto-prodigy`, and startup GAL 1.7.
The Task 6 report is in the plan workspace.

**Verification:** `python3 scripts/check-doc-links.py --scope tracked-live`
and `git diff --check` are required for this docs-only blocker record.

**Next 1–3 steps:** (1) reconnect/authorize the Pixel in ADB; (2) rerun the
full Task 6 A/C/D/E matrix with phone logcat, route transitions, and
screenshots; (3) only then create the guarded candidate deployment snapshot.


---

## 2026-07-26 — GAL 4.3 display compatibility guidance and pre-bench candidate

**What changed:** current AA rendering, settings, plugin-action, phone-debug,
roadmap, and index guidance now records the GAL 1.7/4.3 policy, bounded
version-response diagnostics, per-video field-11 scope, removed session bit
16, and process-lifetime one-reconnect profile semantics. It also records the
Task 0 deployed 1.7 → 1.7/MATCH baseline and the corrected request-only
4.3 → 6.0/MATCH checkpoint, with requested 4.3 authoritative locally.

**Why:** live guidance still carried the retired bit-16 lab control and some
historical GAL 1.1 wording. The native-turn-card flag is now documented as an
HU availability declaration only, not a content-selection or rendering claim.

**Status:** the pre-bench documentation candidate
`9c0b7ee38a871a94052d3ca4f6a3f9c795d4388e` completed its repository and ARM
gate. Its ARM artifact was `ALPHA-26-07-24-01-94-g9c0b7ee`, SHA-256
`9b4fc2d4e559e9c794d87a2287378d7e8930047d07409ede9915f102e5fd5107`.
The active design/plan remain ACTIVE. The final hidden-UI/field-11 hardware
matrix has not passed and no feature completion is claimed.

**Verification:** the full native build, explicit `openauto-prodigy` target,
and `QT_QPA_PLATFORM=offscreen ctest --output-on-failure` passed. The fast ARM
`./cross-build.sh` application build passed; `python3
scripts/check-doc-links.py --scope tracked-live` reported zero broken links;
`git diff --check` passed; and the frozen proto worktree was clean at
`cabe46ec9c5e1628264427aa77d910b1f574bb34`. A first report recorded the
previous artifact hash from an earlier checkpoint; a later re-hash identified
the artifact as rebuilt after that gate and corrected the candidate hash above.

**Next 1–3 steps:** (1) complete the pending field-11 hardware matrix; (2)
retain requested 4.3 as the local policy input while evaluating its evidence;
(3) do not mark the active work complete before that matrix passes.

---

## 2026-07-26 — GAL 4.3 compatible-response policy remediation

**What changed:** corrected experimental GAL 4.3 version admission to require
status MATCH plus a phone-reported numeric tuple greater than or equal to the
requested 4.3 tuple. FSM coverage now locks reported 4.3 and 6.0 success and
reported 4.2 failure before TLS. The policy flag now names minimum-compatible
semantics throughout production and tests. Requested 4.3 remains authoritative
for local feature policy; descriptors, ACK behavior, and response diagnostics
are unchanged.

**Why:** the live Pixel answered a 4.3 HU request with 6.0/MATCH, and pinned AA
17.3 analysis shows downstream phone gates retain the HU request separately
from the compatible reported version. Exact response equality therefore
rejected a supported negotiation.

**Status:** IMPLEMENTED and native-verified. No proto-submodule, descriptor,
AdditionalVideoConfig, QML/action, cross-build, deployment, or hardware change
is part of this remediation.

**Verification:** the focused `test_session_config`, `test_session_fsm`, and
`test_service_discovery_builder` build and CTest commands passed. The explicit
`openauto-prodigy` target built successfully.
`python3 scripts/check-doc-links.py --scope tracked-live` reported zero broken
links, and `git diff --check` passed.

**Next 1–3 steps:** (1) complete the bounded independent review; (2) cross-build
and repeat the request-only GAL 4.3 Pi/Pixel checkpoint; (3) begin Task 3 only
after that checkpoint is accepted.

---

## 2026-07-26 — Android Auto GAL 4.3 display compatibility planning

**What changed:** promoted and documented the next protocol-critical phase in
an ACTIVE design and implementation plan. The scope preserves GAL 1.7 as the
default, pins only audited GAL 4.3 as the experimental upgrade, requires full
version-response diagnostics, and stages a request-only live checkpoint before
any modern descriptor field is enabled. The final feature boundary is limited
to version-gated MAIN clock metadata and a lab-only CLUSTER native-turn-card
declaration. The roadmap and documentation index now point to the active work.

**Why:** current source requests GAL 1.7 while historical capture notes say
1.1, and Android Auto 17.3 ignores per-video hidden-UI features below requested
GAL 4.3. Higher versions would also cross unrelated ACK and service behavior,
so 4.3 is the smallest useful compatibility target. The plan also treats the
audited open-android-auto schema pin and Prodigy's stale manual AV IDs as
prerequisites rather than hiding them inside the display experiment.

**Status:** PLANNING COMPLETE; implementation and hardware validation have not
started. The active artifacts are
`docs/plans/2026-07-26-aa-gal-4-3-display-compatibility-design.md` and
`docs/plans/2026-07-26-aa-gal-4-3-display-compatibility-plan.md`. Explicitly
excluded are GAL 5.x/6.x, ackless media, vehicle energy forecast, AUXILIARY,
third-display/generalized registry work, a native semantic turn-card widget,
and edits inside the community proto submodule.

**Verification:** `python3 scripts/check-doc-links.py --scope tracked-live`
reported zero broken links and `git diff --check` passed. No application build,
cross-build, Pi deployment, or live feature claim applies to this planning-only
change.

**Next 1–3 steps:** (1) approve the active design/plan for execution; (2)
capture the deployed raw 1.7 request and complete response before changing the
proto pin; (3) execute the schema compatibility and request-only 4.3 checkpoint
before adding field-11 UI metadata.

---

Older entries are archived under `docs/archive/session-handoffs/`.
