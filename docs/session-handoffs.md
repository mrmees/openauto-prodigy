# Session Handoffs

Newest entries first.

---

## 2026-07-25 — Android Auto runtime CLUSTER lab implementation

**What changed:** the default-off projected CLUSTER experiment now owns one
validated process-lifetime runtime profile for 480p/720p carrier resolution,
DPI, centered content dimensions, and AA 17.3's session-wide turn-data bit 16.
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

**Status:** COMPLETE on local `dev`; Pi/phone behavior is not yet
live-validated. The initial major review used the user-authorized Opus fallback
after Fable produced no observable progress. It reported zero blockers, two
majors, and three minors. All five findings were adjudicated: four were
confirmed and fixed, and the write-only External API result was confirmed and
deferred. Within the bit-16 finding, the "unattested" subclaim was dismissed
using pinned AA 17.3 `ity.d`/`xmm` evidence; the missing citation, session
scope, and stale protocol-submodule enum were confirmed and documented. A
question about the current bit-16 consumer and stale enum was added to
open-android-auto issue #10. The remediation review reported zero blockers,
zero majors, and one minor; its stale square-crop documentation statement was
confirmed and fixed. The two-pass gate is exhausted. Fable invocation
observability remains a recorded tooling follow-up.

**Verification:** focused profile, descriptor, orchestrator, settings, and
widget tests passed. The native build, explicit `openauto-prodigy` target,
`QT_QPA_PLATFORM=offscreen ctest --output-on-failure`, documentation link
check, frozen-proto boundary check, `git diff --check`, and
`./cross-build.sh` passed. The cross-build compiled the updated QML into the
aarch64 application. No deployment or service restart was performed.

**Next 1–3 steps:** (1) deploy the new aarch64 binary to the Pi and exercise
baseline, full-frame, 720p, and turn-data profiles with an active Maps route;
(2) capture the resulting descriptor/frame/log evidence; (3) use issue #10's
Maps/YouTube Music findings to scope the subsequent AUXILIARY phase.

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

Older entries are archived under `docs/archive/session-handoffs/`.
