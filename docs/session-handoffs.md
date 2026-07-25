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

**Status:** COMPLETE on local `dev` in the implementation range
`b7f6451..7ccdfc4`; not pushed. The default-off product policy remains intact.
The accepted Pi bench keeps `experimental_cluster_display` enabled and the
existing `org.openauto.aa-cluster-16` placement intentionally migrated from
2×2 to 3×3 on dashboard page 1. The Pixel opened CLUSTER channels 12/13,
decoded the required 800×480 carrier, reached Rendering, and produced no
geometry-mismatch terminal error. Matthew accepted the visible basic square
implementation. The pre-deploy binary and configuration remain recoverable
under stamp `20260725-103337`; the installed and staged binary SHA-256 is
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

**Verification:** the full native build, explicit `openauto-prodigy` target,
`QT_QPA_PLATFORM=offscreen ctest --output-on-failure`, `git diff --check`, and
the frozen-proto boundary check passed. `./cross-build.sh` produced the
deployed aarch64 application with compiled-in QML. Local, staged, and installed
artifact hashes matched. The guarded config edit changed only `col_span` and
`row_span`; the target 3×3 cells were collision-free. The service remained
active with zero restarts and one systemd-owned executable. Live journal
evidence recorded both CLUSTER channel opens, an exact 800×480 first decoded
frame, Rendering state, and no geometry mismatch. The user-owned untracked
wishlist baseline remained untouched.

**Next 1-3 steps:** (1) push `dev` and open a draft PR targeting `main` after
Matthew's go-ahead; (2) merge after the agreed review policy is satisfied; (3)
choose and promote the next bounded item from the qualified wishlist rather
than extending this completed experiment in place.

---

Older entries are archived under `docs/archive/session-handoffs/`.
