# Session Handoffs

Newest entries first.

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

## 2026-07-25 — Android Auto AUXILIARY turn-card experiment

**What changed:** a disposable two-commit artifact temporarily advertised the
existing secondary display ID 1 and channels 12/13 as AUXILIARY. The first pass
omitted AV field 8 for an UNKNOWN-content negative control. After reviewing the
new Maps 26.30.05 analysis, the second pass advertised the already-defined
`KEYCODE_TURN_CARD` value 65544. No protocol-submodule or External API protobuf
was changed. A final restoration commit returned the descriptor, diagnostic,
test expectation, and Pi binary to the normal CLUSTER role; the only lasting
code change is an explicit regression assertion that CLUSTER omits field 8.

**Why:** the 24-case CLUSTER matrix always selected Maps, so AUXILIARY needed a
separate bounded live test rather than inheriting that negative result. Maps
26.30.05 publishes distinct CLUSTER and AUXILIARY services and identifies AV
field 8 as the AUXILIARY initial-content selector. Its NAVIGATION value 65538
is absent from the hands-off local enum and its available AA trace stops at
16.4, so that mode remains upstream research rather than a local proto patch.

**Status:** COMPLETE and Pi/Pixel live-validated on local `dev`. AA 17.3
accepted MAIN+AUXILIARY and opened both secondary channels. AUXILIARY/UNKNOWN
sent only the codec header and no decodable frame. AUXILIARY/TURN_CARD stayed
idle without a route, both with media paused and with YouTube Music playing,
then rendered a compact maneuver card during an active Maps route. Media never
populated or replaced AUXILIARY. The session-bit-16 A/B produced the same
turn-card service, confirming issue #10's correction that no AA 17.3 consumer
reads that bit. Issue #10 also confirmed that message 26 cannot replace AV,
cluster power mode is phone policy, geometry is post-selection, and Prodigy's
GAL 1.1 request ignores the newer hidden-UI list. The remaining 17.3 field-8
trace and `KEYCODE_NAVIGATION` enum/provenance question is tracked in
open-android-auto issue #14.

**Verification:** the experimental and restoration descriptor changes each
used focused red/green `test_service_discovery_builder` runs. The explicit
native `openauto-prodigy` target, `QT_QPA_PLATFORM=offscreen ctest
--output-on-failure`, and `./cross-build.sh` passed for the final restored tree.
The final aarch64 hash matched on host and Pi; systemd was active; logs showed
`role=CLUSTER display=1 video_ch=12 input_ch=13`, channel open/start, the first
800×480 decoded frame, and Rendering. The restored dashboard showed the active
Google Maps route. Captures are retained at
`E:\claude\personal\openautopro\auxiliary-display-captures-2026-07-25`.
The initial Opus review reported zero blockers, one major, and three minors;
the major required restoration before publication and was satisfied. Mixed
local CLUSTER labels were accepted only for the disposable capture, the reverse
test was strengthened, and the stale protocol-submodule comment remained
upstream-only. The remediation review reported zero blockers, one major, and
two minors. All three were confirmed and fixed directly: the live source/API
consumer claim, settings reconnect warning, and multi-display wishlist evidence
now match the corrected trace. The two-pass review budget is exhausted.
Direct Claude companion invocations again failed to return an observable
verdict before the repository review gate succeeded; the existing companion
runtime investigation remains open.

**Next 1–3 steps:** (1) wait for open-android-auto issue #14's AA 17.3 field-8
trace and enum update; (2) audit GAL 4.3+ obligations before touching the
requested version or hidden UI features; (3) if #14 confirms NAVIGATION, design
a bounded runtime AUXILIARY content-selector phase before attempting a third
simultaneous display.

---

## 2026-07-25 — Android Auto runtime CLUSTER lab implementation

**What changed:** the default-off projected CLUSTER experiment now owns one
validated process-lifetime runtime profile for 480p/720p carrier resolution,
DPI, centered content dimensions, and an experimental session value 16 toggle.
Debug Settings exposes editable controls plus 300-square/full-frame presets,
apply/reset actions, generation, and result text. The same controller is
reachable through `aa.cluster.applyProfile` and `aa.cluster.resetProfile`, so
External API v1 clients can iterate by JSON action dispatch. Accepted active
changes stage one snapshot and use the existing graceful AA-only reconnect;
the matching descriptor, decoded-frame validation, and aspect-aware widget crop
activate together before the replacement session. Overrides do not edit YAML
or restart Prodigy.

**Why:** the accepted fixed square proved the second projected display, but
iterating on its descriptor still required edits, builds, and service restarts.
This provides durable lab infrastructure while keeping display type fixed to
CLUSTER and avoiding unsupported `ServiceDiscoveryUpdate` behavior. The phone
still chooses map versus turn card according to navigation availability and
policy; geometry is not documented as a content selector.

**Status:** COMPLETE and Pi/Pixel live-validated on local `dev`. The initial
major review used the user-authorized Opus fallback
after Fable produced no observable progress. It reported zero blockers, two
majors, and three minors. All five findings were adjudicated: four were
confirmed and fixed, and the write-only External API result was confirmed and
deferred. The bit-16 finding prompted a current-consumer question in
open-android-auto issue #10. Its later complete trace corrected the original
interpretation: `ity.d` maps an `AdditionalVideoConfig` UI element, while
`session_configuration` value 16 has no AA 17.3 consumer. The remediation
review reported zero blockers,
zero majors, and one minor; its stale square-crop documentation statement was
confirmed and fixed. The two-pass gate is exhausted. Fable invocation
observability remains a recorded tooling follow-up.

**Verification:** focused profile, descriptor, orchestrator, settings, and
widget tests passed. The native build, explicit `openauto-prodigy` target,
`QT_QPA_PLATFORM=offscreen ctest --output-on-failure`, documentation link
check, frozen-proto boundary check, `git diff --check`, and
`./cross-build.sh` passed. The aarch64 binary was deployed to the Pi and its
service returned `READY=1`. External API dispatch then captured the same eight
profiles under active navigation, no navigation, and no navigation with
YouTube Music playing. Logs confirmed every requested generation and decoded
carrier, zero navigation events in the latter two sweeps, and one AA media
stream start before each media screenshot. All 24 confirmed captures showed
Google Maps; route activity only added route UI, while turn-data bit 16 and
media playback produced no alternate CLUSTER content. Captures and manifests
are retained outside the repository at
`E:\claude\personal\openautopro\cluster-lab-captures-2026-07-25`. The Pi was
left on the compiled baseline profile; the prior binary remains available as
`openauto-prodigy.pre-cluster-lab-f2d3f676`.

**Next 1–3 steps:** (1) attach the CLUSTER live result to the open-android-auto
17.3 analysis as useful corroboration; (2) wait for issue #10's current
Maps/YouTube Music trace; (3) scope a separate bounded AUXILIARY phase without
assuming CLUSTER's negative result applies to that display type.

---

## 2026-07-25 — Agentic workflow simplification COMPLETE

**What changed:** the repository now uses a lean trivial/standard/major work
classification instead of mandatory model tiers, per-task subagents, per-task
full builds, and stacked review gates. `scripts/review-gate.sh` selects one
independent reviewer from the author, records immutable SHAs and duration,
pins `high` effort, permits one initial plus one remediation pass, rejects
tracked dirty trees and duplicate/third passes, and requires an explicit
user-authorized reset before changing feature bases. `codex-review.sh` is now a
documented stateful legacy entry point rather than an extra review. The doc
link checker gained its previously documented tracked-live mode so untracked
user files can be excluded without moving or editing them.

Matching global operating contracts were installed for Linux and Windows
Codex and Claude. They explicitly prevent generic skills from multiplying
specs, plans, worktrees, subagents, or reviews beyond user/repository policy.
Both Codex environments now default to `high` reasoning, both Claude
environments default to Opus, and Fable is reserved for major/multi-repository
work. Credential values were not modified or printed again.

**Why:** PR #40 spent nearly five hours and 22 automated verdicts optimizing
for reviewer silence. The previous workflow was designed around a permanent
Fable main session, then combined with Superpowers rules that independently
required per-task and final reviews. Prose-only rerun limits did not stop a
test-only reentrancy review loop. The replacement makes reviewer independence,
production reachability, cost, and stopping conditions explicit and
mechanically bounded.

**Status:** COMPLETE on local `dev`; repository commits are not yet pushed.
Global instruction/model changes are active immediately. The user-owned
untracked AA display baseline remains untouched. Application build, ARM
cross-build, deployment, and hardware testing were intentionally skipped
because no application source, build definition, runtime configuration, or
Pi artifact changed.

**Review gate:** Opus pass 1 reviewed the immutable implementation range in
257 seconds and returned zero blockers, five majors, and five minors. Eight
findings were confirmed: missing `jq` preflight; silent base-reset bypass;
tracked-dirty coverage; incomplete runtime-failure guidance; divergent Claude
review instructions; doc-checker robustness; ambiguous legacy-wrapper
semantics; and lack of a real Codex invocation smoke test. They were resolved
in one remediation batch or, for the invocation gap, by a successful real
Codex model/provider smoke run. Two findings were dismissed: the handoff/plan
were intentionally awaiting final adjudication, and application CTest should
not acquire host agent-tool dependencies.

Opus pass 2 reviewed only the remediation delta in 198 seconds and returned
zero blockers, zero majors, and five minors. Three maintenance notes were
confirmed and deferred: remove the Claude-focus template coupling, improve the
no-git doc-checker error, and clarify that exit 4 means no verdict was recorded
rather than necessarily no model execution. Two were dismissed: base drift
already stops until an explicit user-authorized reset, and an uncommitted
submodule pointer cannot be pushed or enter either committed review range. The
gate is now exhausted at pass 2; no third review was run.

**Verification:** `bash tests/test_review_gate.sh`, Bash syntax checks,
ShellCheck, Python byte-compilation, the tracked-live documentation link check,
and `git diff --check` passed. The four global policy-file hashes match; both
Codex effort keys report `high`, and both Claude model keys report `opus[1m]`.
The real Codex smoke invocation resolved `gpt-5.6-sol` through the OpenAI
provider in read-only mode at `high` effort and returned the required sentinel.

**Next:** rotate the exposed Home Assistant and Namecheap credentials; use the
new metrics/state record across the next five PRs; push/PR these repository
changes when authorized.

---

## 2026-07-25 — Android Auto CLUSTER square viewport COMPLETE

**What changed:** the experimental projected CLUSTER path now asks Android
Auto to render a centered 300×300 content region inside its standard 800×480
H.264 carrier by advertising total margins of 500×180. One immutable C++
geometry contract supplies discovery, role-safe session properties, decoded
frame validation, QML crop offsets, and focused tests. The dashboard widget is
fixed at 3×3 and uses one clipped, uniformly scaled and offset `VideoOutput`;
it does not add another decoder, frame rewrite, shader, stretch, projected
input, arbitrary resolution, or public setting. A mismatched CLUSTER carrier
terminates only that display generation before sink delivery, while MAIN keeps
its existing frame policy and rendering behavior.

**Why:** the completed 2×2 feasibility spike proved that the Pixel would open
and stream an independent CLUSTER display, but it still showed the full
landscape carrier inside a square widget. This bounded follow-up tests the
phone-rendered-margin approach at the desired small cluster shape before any
generalized multi-display or production-settings work is promoted.

**Status:** COMPLETE on local `dev` and staged in draft PR #40. The default-off
product policy remains intact. The accepted Pi bench corresponds to the
implementation range `b7f6451..7ccdfc4`; it keeps
`experimental_cluster_display` enabled and the existing
`org.openauto.aa-cluster-16` placement intentionally migrated from 2×2 to 3×3
on dashboard page 1. The Pixel opened CLUSTER channels 12/13, decoded the
required 800×480 carrier, reached Rendering, and produced no geometry-mismatch
terminal error. Matthew accepted the visible basic square implementation.
Later review-hardening commits keep the stricter reopen and closed-channel
lifecycle rules confined to CLUSTER channels 12/13; established legacy-channel
behavior is unchanged. Those commits have repository and ARM cross-build
coverage but are not claimed as an additional Pi/Pixel bench pass. The
pre-deploy binary and configuration remain
recoverable under stamp `20260725-103337`; the installed and staged binary
SHA-256 is
`f2d3f6765f741014e25a43bd1a6475f382202b7586075e6af68dca60ad15dd03`.

**Review gate:** Opus reviewed the design and plan before execution; their
final amended forms passed. The Task 3 independent review found one important
fixed-square contract issue and one minor coverage issue; both were fixed. Its
first re-review exposed one conflicting all-six-property coverage requirement,
which was fixed without weakening the square crop, and the second re-review
approved the result. The repository Codex gate found one P2 in the Pi rollback
runbook: trapped signals could restore and then resume deployment. It was
confirmed and replaced with guarded EXIT cleanup covering SSH hangup and
termination signals; zero findings were dismissed. The required whole-range
rerun returned `LGTM — no issues found.` A later live verification command
exposed Linux's 15-character `pgrep -x` comm-name limit; the runbook now checks
systemd's MainPID and exact executable path instead.

The later whole-range Opus publication reviews ran with read-only repository
tools and no turn or wall-clock limit. Across the latest passes, confirmed
findings were addressed by separating live-bench claims from later hardening,
validating live-doc links, confining new reopen and closed-channel rules to
CLUSTER, adding event-driven sink recovery, rate-limiting only validated
CLUSTER reopen warnings, and documenting/asserting the direct frame callback's
thread contract. The request to treat an internal Messenger signal change as
an out-of-tree compatibility break was dismissed: the library is an in-tree
static target, all repository consumers were migrated, and the frozen
community proto submodule remained untouched. The earlier request to replace
fixed CLUSTER channel IDs with registration-time policy was also dismissed for
this experiment: IDs 12/13 are the explicit frozen `ChannelId` contract and a
generalized channel-policy API would expand the completed feature without
changing its behavior. A final unbounded Opus rerun remains the publication
gate.

The final pre-publication pair then reported one Codex P2 and five Opus
findings. Codex's sink-claim reentrancy issue was confirmed and fixed by
installing destruction tracking before observer-visible state and returning
failure if a synchronous observer releases the claim. Opus's blocker and its
documentation consequence were confirmed and fixed by limiting control-typed
close notifications to CLUSTER and locking legacy dispatch behavior in a
regression test. Three Opus findings were dismissed: the exact accepted
hardware range and later repository-only hardening are already disclosed; a
geometry mismatch intentionally terminates the affected display generation as
required by the completed fixed-geometry design; and the dormant private codec
failure seam predates this feature, cannot be armed by production code, and
its friend definitions live in separate test executables.

The incremental reruns also confirmed and corrected the broken AA-reference
table placement, missing compatibility diagnostics, and verification wording.
The legacy all-channel close request remained dismissed because it would
restore the shared lifecycle expansion the whole-range blocker required us to
remove; the temporary compatibility waiver is one-shot warned and documented
for separate protocol research.

The remaining review loop focused on hypothetical synchronous observers of
`VideoDecoder::videoSinkChanged`. A repository-wide search found no production
consumer that directly mutates the CLUSTER decoder sink: the cluster widget
uses `ProjectedDisplaySession::attachVideoSink` and `detachVideoSink`. The MAIN
display remains a separate exception that binds its own decoder sink directly
and never calls this ownership API. Defensive changes for unsupported direct
CLUSTER mutation repeatedly created new callback-order cases, so they and their
fault-injection tests were removed. The retained code covers the supported
public contract, including an observer that releases through `detachVideoSink`
during the availability notification.

The final bounded Codex and Opus reviews reported no supported production P1.
Their duplicate phantom-claim findings, plus derivative diagnostics and test
coverage findings, were dismissed because each requires unsupported direct
mutation of the CLUSTER decoder during a synchronous ownership callback. Their
shared finding that the ownership wording incorrectly included the MAIN
display was confirmed and corrected. Opus also correctly identified stale
verification wording below; the exact final production tree had already passed
the required gates, so that record is corrected here. No behavior changed in
this adjudication.

**Verification:** the full native build, explicit `openauto-prodigy` target,
`QT_QPA_PLATFORM=offscreen ctest --output-on-failure`, `git diff --check`, and
the frozen-proto boundary check passed. The tracked live-document link scope
also passed; the repository-wide checker reported only the known external-tree
references in the user-owned untracked wishlist baseline, which remained
untouched. `./cross-build.sh` produced the deployed accepted aarch64
application with compiled-in QML. The latest production review-fix delta and
the final contract-bounded production tree were each cross-built successfully.
Any future production change carries the same verification requirement.
For the accepted bench,
local, staged, and installed artifact hashes matched. The guarded config edit
changed only `col_span` and `row_span`; the target 3×3 cells were
collision-free. The service remained active with zero restarts and one
systemd-owned executable. Live journal evidence recorded both CLUSTER channel
opens, an exact 800×480 first decoded frame, Rendering state, and no geometry
mismatch.

**Next 1-3 steps:** (1) publish and merge PR #40 after the bounded final review
and CI are green; (2) verify the CLUSTER-only lifecycle changes during the next
routine Pi/Pixel session; (3) choose and promote the next bounded item
from the qualified wishlist rather than extending this completed experiment in
place.

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

Older entries are archived under `docs/archive/session-handoffs/`.
