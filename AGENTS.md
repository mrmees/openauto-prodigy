# AGENTS.md

Source of truth for agent instructions in this repo. Read this before any work; read the nested AGENTS.md nearest the code you're editing (list below).

## Hard Constraints

- **`libs/prodigy-oaa-protocol/proto/` is hands-off** — community submodule ([open-android-auto](https://github.com/mrmees/open-android-auto)). Note needed proto changes; never edit them here.
- **`proto/api/` is FROZEN additive-only** (since `875feaf`): field numbers never reused, messages never renamed, semantics never silently changed. New capability = new field + capability flag.
- **Wireless-only AA.** No USB/libusb transport — BT discovery → WiFi AP → TCP.
- **Qt 6.8 system packages.** WSL2 Debian Trixie dev environment = Pi target; no CMAKE_PREFIX_PATH, no vendored Qt.
- **HF/AG roles:** the Pi is the HFP Hands-Free (0x111e); the phone is the Audio Gateway. If you're registering profile 0x111f on the Pi, stop.
- **No ofono, no `provide-ofono`** — telephony goes through `org.pipewire.Telephony` directly.
- **External API rails:** the API binds providers/services, never EventBus topics, D-Bus paths, or AA protocol internals; all mutation through ActionRegistry or explicit invokables; additive proto only; the JS shim gets no capability the public API lacks.
- **Frozen numerics:** `ICallStateProvider` values (`Idle=0, Ringing=1, Active=2`), overlay z-bands (1000/2000/3000/3500/4000), `DashboardContributionKind` order, YAML placement field names. Append, never renumber.

## Overview

Clean-room open-source rebuild of OpenAuto Pro (BlueWave Studio, defunct): a Raspberry Pi 4 wireless-only Android Auto head unit. Qt 6 + QML shell, plugin architecture (AA projection, BT audio, phone/HFP, media player, equalizer), PipeWire audio, BlueZ D-Bus, Flask web-config panel, External API v1 (protobuf over TCP/WS). Architecture map: `docs/architecture.md`.

## Commands

```bash
# Local build + tests (WSL2 Debian Trixie, Qt 6.8 system packages).
# Build dir lives on the Linux filesystem — the repo sits on a Windows drive
# (9p mount) and object churn there is painfully slow. Never build in the
# in-repo build/ dir. If the build dir is missing, configure it first:
#   cmake -S . -B ~/builds/openauto-prodigy
cd ~/builds/openauto-prodigy && cmake --build . -j$(nproc)
ctest --output-on-failure
```

**ctest does NOT compile `main.cpp`** — a cached object file masked an app-target break on 2026-07-09. Always build the app target explicitly before claiming green or gating:

```bash
cmake --build . --target openauto-prodigy -j$(nproc)
```

```bash
# Cross-compile for Pi (Docker, aarch64) — never use toolchain-pi4.cmake directly
./cross-build.sh                              # app target only (~4-6 min)
./cross-build.sh --full                       # all targets incl. ARM test binaries

# Deploy to Pi + restart
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.149:~/openauto-prodigy/build/src/
ssh matt@192.168.1.149 'sudo systemctl restart openauto-prodigy.service'
ssh matt@192.168.1.149 '~/openauto-prodigy/restart.sh --force-kill'   # stuck processes
```

QML ships **inside the binary** (qt_add_qml_module + qmlcache) — UI changes require cross-build + binary rsync; a `git pull` on the Pi will NOT update the UI.

## Project Management Loop

For behavior-changing work in this repository:

1. Check alignment with `docs/project-vision.md` before implementation.
2. Update `docs/roadmap-current.md` when priorities or sequencing change.
3. Before claiming completion, run the local build, the app-target build, and the test suite (commands above).
4. Append a handoff entry to `docs/session-handoffs.md` including:
   - what changed
   - why
   - status
   - next 1-3 steps
   - verification commands/results

## Tiered Execution Workflow

Full design: `docs/archive/plans/2026-07-09-tiered-execution-codex-gate-design.md`.
Planning stays interactive in the main (Fable) session via superpowers
brainstorming + writing-plans; the conventions below govern how plans are
tagged, executed, and reviewed.

### Tier tags (plan time)

Every task in a plan carries a `Tier:` field:

| Tier | Model | Used for |
|------|-------|----------|
| `opus` | Opus | Default for implementation: real code, logic, cross-module changes |
| `sonnet` | Sonnet | Test scaffolding, mocks/fixtures, mechanical pattern-following edits, doc updates |
| `main` | Fable (main session) | Protocol-critical or judgment-heavy work (AA protocol internals, threading) |

**Definition of Ready** — do not dispatch a task unless: exact files are
named; acceptance criteria are testable ("returns 422 on empty id", not
"works"); scope is bounded with an explicit out-of-scope line; a test command
is given; no open questions remain. A task that fails this checklist goes back
to the human as a question, not to a worker.

**Small-work bypass:** trivial single-file fixes skip tiering entirely; the
main session just does them.

### Dispatch (execution time)

Dispatch each task as a subagent with the tier's model pinned (Agent tool
`model` parameter). Workers read this file and the nested AGENTS.md nearest
their working files. Workers own the build/fix/test loop and report
**synthesized results only**: files changed (one line each), test command +
pass/fail counts, deviations. Raw logs stay out of the main session's context.
Workers commit per task; nobody pushes mid-execution.

### Escalation ladder: Opus → Codex (gpt-5.6-sol) → Fable

1. **Opus worker** — owns the task. Two focused attempts at green, then
   escalate instead of grinding.
2. **Codex** — gets a prompt file containing the task (files, acceptance
   criteria, test command) plus the worker's failure report, with write
   access so it can apply the fix:

   ```bash
   codex exec -C <repo-abs-path> -s workspace-write -o <scratchpad>/codex-task-verdict.txt - < <scratchpad>/codex-task-prompt.txt
   ```

   Afterwards the main session reads the verdict file AND `git diff`, verifies
   the result against the task's acceptance criteria, and commits if it
   passes. Codex is trusted to write, not to self-certify.
3. **Fable (main session)** — takes the task over directly if Codex also
   fails.

### Review gate (per feature, pre-push)

After a plan's tasks are done, **the `openauto-prodigy` app target builds**,
and tests are green.

1. Run `bash scripts/codex-review.sh` — reviews `@{upstream}..HEAD` in a
   read-only sandbox and saves structured P1/P2/P3 findings to `reviews/`
   (gitignored). Explicit range: `bash scripts/codex-review.sh <base-ref>`.
   Exit codes: 0 ok/empty, 1 usage, 2 codex not installed, 4 codex failed.
   On exit 2/4 the gate degrades to a Fable-only review — note that in the
   session-handoffs entry; the gate never silently passes.
2. The main Fable session adjudicates **every** finding: confirmed → fix
   (inline if small, Opus worker if substantial); dismissed → stated reason.
   No silent drops. Substantial fixes trigger one re-run of the gate on the
   new range.
3. Record the adjudication summary (confirmed/dismissed counts, blockers
   fixed) in the session-handoffs entry. Only then push — with the user's
   go-ahead.

## Nested Instructions

Not all tooling auto-loads nested files — read the nearest one before editing that subsystem:

- `src/AGENTS.md` — Qt/D-Bus/PipeWire build & runtime gotchas
- `src/core/aa/AGENTS.md` — AA protocol rules (touch, video, sockets)
- `libs/prodigy-oaa-protocol/AGENTS.md` — protocol library + submodule boundary
- `qml/AGENTS.md` — QML/UI rules and deployment

## Docs Conventions

- Plan status vocabulary: `ACTIVE`, `COMPLETED <YYYY-MM-DD>`, `PARKED — <reason>`, `ABANDONED — <reason>`. Only ACTIVE files are current guidance.
- New plans/specs are saved to `docs/plans/` (conventions: `docs/plans/README.md`). Completion flips the header and moves the file to `docs/archive/plans/` in the same commit.
- Everything under `docs/archive/` is history, not guidance — never edit archived content to "fix" it.
- `docs/session-handoffs.md` over ~300 lines → rotate the oldest month into `docs/archive/session-handoffs/`.
- Behavior changes update the docs that describe them in the same commit (`docs/INDEX.md` is the map).
- Docs never state exact test counts — state the command (`ctest --output-on-failure`) instead.
- **Wishlist-then-promote:** new feature ideas go to `docs/wishlist.md`, not into scope. Plans don't grow features mid-execution.

## Versioning

- Alpha scheme: **`ALPHA-YY-MM-DD-NN`** ANNOTATED git tags (date from
  `date +%y-%m-%d`, NN = build number of the day, two digits). Tags are
  created ONLY when Matthew declares a milestone — never per deploy or per
  build. Mint the next one with `bash scripts/tag-alpha.sh` (NN = today's
  max + 1; deleting the day's newest tag frees its number — never delete a
  tag that shipped).
- **Official tags ship a Pi release** (adopted 2026-07-14): after tagging and
  pushing, cross-build (`./cross-build.sh`), package
  (`tools/package-prebuilt-release.sh --build-dir build-pi --output-dir dist
  --version-tag <TAG>`), and publish
  (`gh release create <TAG> dist/<asset>.tar.gz --prerelease` — alphas are
  always prereleases). The packager requires the patched libspa deb in
  `tools/pipewire-msbc/out/` (not in git; canonical copy lives on the Pi at
  `~/pipewire-msbc/`).
- The binary derives its version at CMake **configure time**
  (`git describe --match "ALPHA-*" --dirty`, annotated tags only, output
  format-validated → `OAP_VERSION` compile definition on `openauto-core`).
  After tagging, reconfigure + rebuild or the binary keeps the previous
  string. Untagged builds report `ALPHA-<tag>-<n>-g<hash>` /
  `ALPHA-untagged-<hash>`.
- Every user-visible surface reads `OAP_VERSION` (Qt applicationVersion and
  `--version`, QML `Qt.application.version`, IPC status, External API
  ServerHello + SystemStatus, AA ServiceDiscovery `sw_build`/`sw_version`).
  Never hardcode a version string. `identity.sw_version` was removed
  2026-07-09.
- Beta transition checklist (all five together): `PREFIX` in
  `scripts/tag-alpha.sh`; the `--match` pattern AND validation regex in
  top-level `CMakeLists.txt`; the regex in `tests/test_oap_version.cpp`; this
  section.

## Scope Note

This file defines repo-specific workflow expectations. Platform-level safety and skill instructions still apply.
