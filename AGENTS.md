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

## Lean Execution Workflow

This section overrides generic skills and plugins when they prescribe more
specs, plans, worktrees, subagents, reviews, or verification than this
repository requires. Skills are techniques, not permission to multiply gates.

### Classify the work first

| Class | Examples | Required process |
|---|---|---|
| `trivial` | Docs, comments, mechanical config, obvious single-file fix | Implement inline; focused verification; no spec, plan, worktree, subagent, or external review |
| `standard` | Bounded single-repo behavior change | Confirm intent; TDD where behavior is testable; inline execution by default; one independent review |
| `major` | Multi-repo, architectural, protocol-critical, threading, security-sensitive | Approved written spec/plan; optional bounded delegation; one Fable review when independent |

Direct answers, audits, diagnoses, and planning requests do not authorize code
changes. A user-approved design does not need to be re-approved because a
generic skill asks for another document gate.

### Planning and implementation

- Check `docs/project-vision.md` before user-visible feature work. Update
  `docs/roadmap-current.md` only when priority or sequencing actually changes.
- New written specs/plans live in `docs/plans/`; use them for major work or when
  the user asks, not as mandatory ceremony for small changes.
- Execute inline by default. Use subagents only when the user requests them or
  when tasks are genuinely independent, bounded, and non-overlapping. Do not
  add per-task reviewer subagents.
- Delegated tasks must name exact files, testable acceptance criteria, an
  explicit out-of-scope line, and a test command. Workers report synthesized
  results; the owning session verifies the diff.
- Two failed remediation attempts in the same area trigger a stop. Re-evaluate
  the premise and restore the last green/accepted design before doing more
  work. Never optimize for reviewer silence.
- Keep commits coherent and atomic. Do not split mechanical work merely to
  manufacture task boundaries. Never push mid-execution.

### Verification by change type

| Changed surface | Required final verification |
|---|---|
| Docs/tooling only | Targeted tool tests, syntax/lint where available, doc-link check when docs changed, `git diff --check` |
| C++/QML/CMake/runtime config | Native build, explicit `openauto-prodigy` app target, and `ctest --output-on-failure` |
| Pi artifact, embedded QML, or target-only behavior | Above plus `./cross-build.sh` before deploy/publication |
| Hardware behavior | Above plus the relevant live test when hardware is available |

Do not run application builds or Pi deployment for docs/tooling-only changes.
Before claiming completion, append a concise entry to
`docs/session-handoffs.md`: what changed, why, status, next 1–3 steps, and the
commands/results that apply to the changed surface.

### Accepted-tree anchor

Once the user accepts behavior on hardware, record the accepted SHA. After
that point, only a demonstrated supported-production `BLOCKER` may churn the
current PR. `MAJOR`, `MINOR`, speculative, and research findings go to a
follow-up PR or `docs/engineering-backlog.md` after adjudication.

### One bounded review gate

Normal work gets one reviewer from a different model family than the author:

| Author | Standard review | Major review |
|---|---|---|
| Codex | Opus | Fable |
| Claude/Opus/Fable | Codex | Codex |

Run the provider-aware gate after the required verification is green:

```bash
bash scripts/review-gate.sh --author codex --base <base-ref>
bash scripts/review-gate.sh --author codex --major --base <base-ref>
bash scripts/review-gate.sh --author claude --base <base-ref>
# Add --accepted <sha> after hardware acceptance.
```

The gate captures immutable SHAs, pins review effort to `high`, and stores its
state and verdicts in gitignored `reviews/`. It permits exactly:

1. One initial review of `<base>..HEAD`.
2. One remediation review of `<previous-reviewed-HEAD>..HEAD`.

It refuses duplicate runs, pass three, and silent base changes. Starting a new
feature requires an explicit user-authorized `review-gate.sh --reset`, after
which the new base starts a fresh gate. Reviews run without a wall-clock
autokill; scope and pass count, not premature termination, bound cost.

Every finding is adjudicated explicitly. A `BLOCKER` must demonstrate a
supported production entry point, reachable call chain, material impact, and
concrete evidence. Public-but-unused internals, unsupported direct mutation,
test-only observers, and hypothetical consumers are nonblocking research.
After pass two, only a supported-production `BLOCKER` can stop publication;
record other confirmed findings for follow-up. Never run concurrent independent
reviewers unless the user explicitly requests the experiment.

Record confirmed/dismissed/deferred counts in the handoff, then push only with
the user's go-ahead.

Exit 2 or 4 means the review did not run. Fix the reviewer runtime or use one
explicitly authorized independent fallback and record it in the handoff; a
non-zero gate exit never silently passes.

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
- **Wishlist-then-promote:** new user-facing capability ideas go to `docs/wishlist.md`, not into scope.
- Concrete technical findings go to `docs/engineering-backlog.md`. Backlog entries are leads, not executable tasks: re-research them against current code and turn confirmed findings into an approved design/plan before implementation.
- Unconfirmed milestone or hardware observations go to `docs/validation-current.md`. Delete them when disproven; promote confirmed defects to the engineering backlog.
- Plans don't grow features or technical follow-ups mid-execution.

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

This file defines repo-specific workflow expectations. Platform safety still
applies. Where generic skill ceremony conflicts with the lean workflow above,
this file is the explicit user-level override.
