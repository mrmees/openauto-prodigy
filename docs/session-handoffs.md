# Session Handoffs

Newest entries first.

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
consumer of that signal, and direct non-null decoder sink mutation is outside
the ownership contract: production consumers use
`ProjectedDisplaySession::attachVideoSink` and `detachVideoSink`. Defensive
changes for the unsupported path repeatedly created new callback-order cases,
so they and their fault-injection tests were removed. The retained code covers
the supported public contract, including an observer that releases through
`detachVideoSink` during the availability notification. Publication requires
one bounded final review against that explicit production contract after the
full native and aarch64 gates.

**Verification:** the full native build, explicit `openauto-prodigy` target,
`QT_QPA_PLATFORM=offscreen ctest --output-on-failure`, `git diff --check`, and
the frozen-proto boundary check passed. The tracked live-document link scope
also passed; the repository-wide checker reported only the known external-tree
references in the user-owned untracked wishlist baseline, which remained
untouched. `./cross-build.sh` produced the deployed accepted aarch64
application with compiled-in QML. The latest production review-fix delta and
the last committed production tree were each cross-built successfully. The
contract-bounding cleanup must repeat the final-tree native and aarch64 gates
before publication; any future production change carries the same requirement.
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
