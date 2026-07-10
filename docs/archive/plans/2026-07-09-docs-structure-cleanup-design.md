# Docs & Repo Structure Cleanup — Design

Status: COMPLETED 2026-07-09
Date: 2026-07-09
Author: Fable session with Matthew (brainstorming skill)
Guiding reference: `E:\claude\personal\github\ai_repo_layout.md` (workspace, not in repo)

## Goal

Make the repo self-orienting for humans and AI agents: fresh docs over stale ones,
one source of truth for agent instructions, every plan carrying an explicit status,
and a public face suitable for recruiting contributors. Fix the drift accumulated
since February ("loosey-goosey" era).

## Problems Being Solved

1. **Stale docs presented as current.** README's "Current State" is dated Feb 26
   (claims 48 tests; the suite is 115 as of 2026-07-09; predates dashboards,
   widgets, External API v1, HFP, media player). CLAUDE.md references
   `docs/aa-protocol/` which does not exist, and states two different test
   counts (47 and 88) — both wrong. Rule going forward: docs never state exact
   test counts; they state the command that reports them.
2. **Agent-instruction split-brain.** CLAUDE.md (254 lines) holds build commands,
   architecture, and the Gotchas section; AGENTS.md (102 lines) holds workflow.
   Codex reads only AGENTS.md and never sees the gotchas.
3. **Plan sprawl.** Four plan homes (`docs/plans/`, `docs/plans/active/`,
   `docs/plans/archive/`, `docs/superpowers/{specs,plans}/`, plus archive-of-archive
   under `OpenAutoPro_archive_information/plans/`) and no status headers — an agent
   cannot tell finished plans from current guidance.
4. **Unbounded history.** `session-handoffs.md` is 1,527 lines and grows forever;
   raw validation logs sit in `docs/validation/`; a vestigial `docs/session-handoffs/`
   directory holds three March files.
5. **Missing orientation docs.** No `docs/architecture.md`, no READMEs for
   `tests/`, `scripts/`, `tools/`.
6. **Root clutter.** Two screenshots (one 547 KB) at repo root; `docs/.gitkeep`.

## Decisions (settled during brainstorming)

| Question | Decision |
|---|---|
| Scope | Full repo structure (docs + agent instructions + root hygiene + public face) |
| Agent-instruction model | AGENTS.md as SSOT with nested per-subsystem AGENTS.md files; CLAUDE.md becomes a pointer stub |
| Plans home | Single `docs/plans/` for live plans; everything historical under `docs/archive/`; status headers everywhere |
| History retention | Rotate + archive (nothing deleted except trivia); rotation rule documented in AGENTS.md |
| Public face | Full polish: README rewrite + `docs/architecture.md` + CONTRIBUTING.md + `.github/` templates |
| Execution | Staged three-phase cleanup on `dev`, Codex gate before push |

## Target Tree

```text
repo root
├─ README.md                  ← rewritten (phase 3)
├─ AGENTS.md                  ← SSOT, rewritten (phase 2)
├─ CLAUDE.md                  ← ~10-line pointer stub (phase 2)
├─ CONTRIBUTING.md            ← new (phase 3)
├─ .github/                   ← new: PR template + issue templates (phase 3)
├─ assets/                    ← pi-screenshot.png, pi-screenshot2.png move here
├─ docs/
│  ├─ INDEX.md                ← rewritten to match new tree
│  ├─ architecture.md         ← new: components, boundaries, data flow, threading, target hardware
│  ├─ project-vision.md
│  ├─ roadmap-current.md
│  ├─ wishlist.md
│  ├─ design-decisions.md     (stays — decision rationale)
│  ├─ design-philosophy.md    (stays — explanation)
│  ├─ session-handoffs.md     ← rotated: 2026-07 entries only
│  ├─ development.md          (stays — getting-started)
│  ├─ wireless-setup.md       (stays — getting-started)
│  ├─ reference/
│  │  ├─ config-schema.md
│  │  ├─ plugin-api.md
│  │  ├─ widget-developer-guide.md
│  │  ├─ web-widget-authoring.md
│  │  ├─ settings-tree.md
│  │  ├─ state-matrix.md
│  │  └─ release-packaging.md
│  ├─ aa-protocol/            (8 docs)
│  │  ├─ aa-phone-side-debug.md
│  │  ├─ android-auto-protocol-cross-reference.md
│  │  ├─ aa-apk-deep-dive.md
│  │  ├─ aa-display-rendering.md
│  │  ├─ aa-video-resolution.md
│  │  ├─ aa-troubleshooting-runbook.md
│  │  ├─ apk-proto-reference.md
│  │  └─ apk-indexing.md
│  ├─ how-to/
│  │  ├─ debugging-notes.md
│  │  └─ testing-reconnect.md
│  ├─ plans/
│  │  ├─ README.md            ← plan conventions + executor-handbook content
│  │  └─ (ACTIVE / PARKED plans only)
│  ├─ pi-config/              (unchanged)
│  └─ archive/                ← rule: everything here is history, not guidance
│     ├─ plans/               (completed plans, milestone docs, completed superpowers specs+plans;
│     │                        keeps protobuf-source-docs/ subdir intact)
│     ├─ session-handoffs/    (rotated entries + 3 vestigial per-session files)
│     ├─ validation/          (phase18 logs + 2026-02-26 video-pipeline baseline)
│     ├─ research/            (hfp-stack-spike.md, proto-validation-report.md)
│     └─ openauto-pro/        (OpenAutoPro_archive_information/ renamed, content unchanged)
├─ tests/README.md            ← new (phase 2)
├─ scripts/README.md          ← new (phase 2)
└─ tools/README.md            ← new (phase 2)
```

## Disposition Rules

- **Move, don't rewrite** (phase 1 is `git mv` + headers only; content edits happen
  in phases 2–3 or not at all).
- **Archive over delete.** Only deletions: `docs/.gitkeep`, and the emptied
  `docs/plans/active/` + `docs/plans/archive/` directory shells after content moves.
- **Untouched:** `docs/private/` (deliberately untracked), `reviews/` (gitignored),
  `.superpowers/` (tool state), `libs/prodigy-oaa-protocol/proto/` (hands-off
  submodule), build dirs.
- Every plan/spec/design file gets a status header (see vocabulary below) as the
  first body line: `Status: ...`.

### Blanket rule: legacy plans (Codex finding, P1)

Every immediate `docs/plans/*.md` dated 2026-02-* / 2026-03-* (all ~33
design/plan/change-request files, plus the five milestone docs) →
`docs/archive/plans/`, `Status: COMPLETED <date>` — **after** verifying
completion against `docs/session-handoffs.md` / milestone docs during
execution. Any file whose completion cannot be confirmed gets
`Status: ABANDONED — unverified, archived 2026-07-09` instead; none are
silently guessed. The July `docs/superpowers/{specs,plans}/*` files follow the
same verify-then-archive rule, with the exceptions below.

### Known non-obvious dispositions

| File | Disposition |
|---|---|
| `docs/plans/active/2026-02-21-config-contract-overhaul-{design,plan}.md` | → `docs/plans/`, `Status: PARKED — needs re-triage (approved 2026-02-21, never executed)`; add wishlist line |
| `docs/superpowers/specs/2026-07-08-media-player-design.md` | → `docs/plans/`, `Status: ACTIVE` (stage 1 shipped, stage 2+ pending) |
| `docs/superpowers/specs/2026-07-09-tiered-execution-codex-gate-design.md` | → `docs/archive/plans/` (durable content already distilled into AGENTS.md); update the AGENTS.md link |
| `docs/superpowers/plans/README-executor-handbook.md` | content merges into new `docs/plans/README.md`; original archived |
| `docs/proto-validation-report.md` | → `docs/archive/research/` (point-in-time Feb migration validation, not living reference) |
| `docs/hfp-stack-spike.md` | → `docs/archive/research/` (research complete; HFP shipped 2026-07) |
| `docs/baselines/2026-02-26-video-pipeline-baseline.md` | → `docs/archive/validation/` |
| `docs/plans/milestone-0[1-5]-*.md` | → `docs/archive/plans/`, `Status: COMPLETED` (milestone summaries; INDEX keeps a "Milestone History" section pointing at archive) |
| `pi-screenshot.png`, `pi-screenshot2.png` | → `assets/` |
| `docs/OpenAutoPro_archive_information/needs-review/*` (4 files) | NOT silently archived — phase 1 includes a triage checkpoint with Matthew per file; `miata-hardware-reference.md` likely moves out of this repo to `personal/miata/` |
| `tools/aa_proto_graph.py` generated output | tool's default output path changes to `docs/aa-protocol/protocol-reference.md`; fix the dead link in `aa-troubleshooting-runbook.md` (currently points at nonexistent `docs/aa-protocol-reference.md`) |

### Statuses requiring verification during execution (do not guess)

For each July superpowers spec/plan, confirm completion against
`docs/session-handoffs.md` and memory before assigning COMPLETED. Known-uncertain:
`2026-07-07-widevine-enablement.md`, `2026-07-05-phase-f-light-plans.md`. Anything
unverifiable gets flagged to Matthew, not silently archived.

## Status Vocabulary

```
Status: ACTIVE
Status: COMPLETED <YYYY-MM-DD>
Status: PARKED — <reason>
Status: ABANDONED — <reason>
```

Convention (stated in AGENTS.md): only ACTIVE files are current guidance; archive/
is history; a completed plan flips its header and moves to `docs/archive/plans/`
in the same commit that completes it.

## Agent-Instruction Architecture

### Root AGENTS.md (target ≤ ~150 lines)

- Hard constraints at top: hands-off proto submodule; wireless-only AA (no USB);
  Qt 6.8 target.
- Project overview (3–4 lines) + tech stack.
- Commands: local build/test, cross-build (`./cross-build.sh`), Pi deploy
  (rsync + service restart), force-restart — lifted from CLAUDE.md.
- Existing workflow content stays: management loop, tier tags, DoR, escalation
  ladder, Codex review gate.
- Short repo map pointing at `docs/architecture.md` (no duplication).
- **Explicit list of the four nested AGENTS.md files** with a "read the nearest
  one before editing that subsystem" instruction — so the scheme works even for
  tooling that does not auto-load nested instruction files (Codex finding, P2).
- Conventions: plan status vocabulary, archive rule, session-handoff rotation,
  doc-update rule ("behavior-changing PRs update the matching doc or state none
  applies"), superpowers spec/plan output location = `docs/plans/`.

### Nested AGENTS.md files (4)

| File | Contents (migrated from CLAUDE.md Gotchas/protocol sections) |
|---|---|
| `src/AGENTS.md` | QTimer include + event-loop rule; MOC needs .cpp listed; QColor→Qt6::Gui; QVideoFrame ref-counting; QDBusArgument beginMap/endMap; PipeWire full-period rule; SPA_DICT named-array syntax; ASIO→Qt thread bridging via QueuedConnection |
| `src/core/aa/AGENTS.md` | Touch coordinate space (video res, not touch_screen_config); action_index vs pointer_id; all-pointers-every-message; SPS/PPS via AV_MEDIA_INDICATION; config_index=3 quirk; FFmpeg thread_count=1; YUVJ420P acceptance; margin_width/height behavior; EVIOCGRAB lifecycle; tcp_info dead-connection polling; SOCK_CLOEXEC + SO_REUSEADDR ordering; netinet/linux tcp.h conflict; Boost.Log ShortDebugString |
| `libs/prodigy-oaa-protocol/AGENTS.md` | Hands-off submodule warning (proto/ dir); pointer to `src/core/aa/AGENTS.md` for protocol behavior gotchas |
| `qml/AGENTS.md` | No `loadFromModule`; never pointer-handlers over WebEngineView; sidebar MouseArea is visual-only during AA (evdev hit zones own touch); QML ships in-binary — UI changes need rebuild+redeploy, not git pull |

Pi/system operational gotchas (labwc mouseEmulation, WAYLAND_DISPLAY, DFRobot touch
hardware, pkill -f, udev) move to `docs/how-to/debugging-notes.md`.

### CLAUDE.md stub

~10 lines: "AGENTS.md is the source of truth for this repo" + pointer to
`docs/INDEX.md` + the same nested-AGENTS.md list. No duplicated commands or
gotchas.

**CLAUDE.md reference sweep (Codex finding, P2):** phase 2 greps the repo for
references to CLAUDE.md and its gotchas — known sites: `AGENTS.md` ("workers
inherit the repo CLAUDE.md"), `src/core/aa/NightModeProvider.cpp`,
`src/ui/DashboardManager.hpp`, the executor handbook — and updates each to the
appropriate root/nested AGENTS.md target.

### New subdirectory READMEs

- `tests/README.md` — layout, running a single test, the "ctest does not compile
  main.cpp — always build the app target before gating" trap, hardware-dependent
  vs host-runnable tests.
- `scripts/README.md` — codex-review.sh (usage, exit codes 0/1/2/4, output to
  gitignored `reviews/`), validate-resolutions.sh; safety notes.
- `tools/README.md` — proto tooling map: which scripts generate which JSON
  artifacts, inputs vs outputs, what is safe to re-run.

## New Public-Facing Docs

### docs/architecture.md (new, phase 2)

Extracted from README's architecture sections + CLAUDE.md architecture notes:
main components (composition root, core services, plugin layer, protocol library,
UI layer, web config), **boundaries** (e.g., protocol threads never touch Qt UI
directly — QueuedConnection bridge; plugins access services only via IHostContext;
web config writes only through IpcServer), runtime data flow (wireless AA session
path), threading model, target hardware table (from CLAUDE.md).

### README.md rewrite (phase 3)

One-paragraph pitch + screenshots (from `assets/`); date-free current feature
list (wireless AA, plugins, dashboards/widgets, web config, External API v1,
BT audio + HFP, theming); hardware needed; quickstart for both install paths
(prebuilt / source); pointers to INDEX, architecture, CONTRIBUTING. Removed:
dated "Current State" section, test-count claims, version-inconsistency notes
(0.1.0/0.3.0 mismatch becomes a wishlist entry to fix in code), architecture
deep-dive (→ architecture.md), release-engineering detail (→
`docs/reference/release-packaging.md`).

### CONTRIBUTING.md (phase 3)

Human workflow only; defers to AGENTS.md for commands. Issues/PR flow
(`dev` → PR → `main`), expectation of green build + ctest, one-line style rule
(match surrounding code), where to talk (GitHub issues).

### .github/ (phase 3)

- `pull_request_template.md`: Summary / Why / Testing checklist (build + ctest
  commands) / Pi deployment notes / Risk.
- `ISSUE_TEMPLATE/bug_report.md`: prompts for Pi model, phone model, logs.
- `ISSUE_TEMPLATE/feature_request.md`.

## Maintenance Rules (codified in AGENTS.md, phase 2)

1. Behavior-changing PRs update the matching doc or explicitly state none applies.
2. New plans start `Status: ACTIVE`; completion flips the header and moves the
   file to `docs/archive/plans/` in the same commit.
3. When `session-handoffs.md` exceeds ~300 lines, rotate the oldest month into
   `docs/archive/session-handoffs/`.
4. When a doc stops being true, fix it or delete it immediately.

## Execution Phases

- **Phase 1 — Move & mark (mechanical; sonnet-tier):** create new dirs; all
  `git mv`s per disposition rules; add status headers; rotate session-handoffs
  (keep 2026-07 entries; Feb–Mar entries → `docs/archive/session-handoffs/`);
  move screenshots; delete `.gitkeep`; repo-wide link-fix sweep (grep every
  moved path in BOTH absolute `docs/...` and relative `superpowers/...` forms,
  covering `install.sh`, `web-config/`, `tools/`, `scripts/`, tests, CMake,
  source comments, and the moved docs themselves); needs-review triage
  checkpoint with Matthew (4 files). **Explicit content-edit exceptions to
  "move, don't rewrite"** (Codex finding, P2): INDEX.md rewrite, new
  `docs/plans/README.md` (executor-handbook merge), status headers, link-path
  fixes, `tools/aa_proto_graph.py` output-path constant. Nothing else.
- **Phase 2 — Agent instructions (main-tier):** write `docs/architecture.md`;
  rewrite AGENTS.md as SSOT; create 4 nested AGENTS.md files; reduce CLAUDE.md
  to stub; add tests/scripts/tools READMEs.
- **Phase 3 — Public polish (main-tier):** README rewrite; CONTRIBUTING.md;
  `.github/` templates.
- **Gate:** app-target build + ctest (should be unaffected; gate rule is the gate
  rule), `bash scripts/codex-review.sh`, adjudicate all findings, session-handoffs
  entry, push only with Matthew's go-ahead.

### Verification

- Phase 1: markdown link check over **all live (non-archive) docs** — every
  relative and absolute link resolves (Codex finding, P1: grep-for-old-paths
  alone misses relative links like `superpowers/specs/...` in
  session-handoffs.md); `git grep` for every old path returns no live
  references (archive content may keep historical references — acceptable);
  working tree builds.
- Phase 2: AGENTS.md ≤ ~150 lines; no command/gotcha exists in two places;
  CLAUDE.md stub contains no operational content.
- Phase 3: README contains no dated state claims; all image links resolve.

### Archived-spec references from live docs/code (Codex finding, P2)

Live docs and source comments cite completed superpowers specs (e.g.
`docs/web-widget-authoring.md` → External API / JS-runtime designs;
`src/core/services/PhoneStateService.hpp`, `src/main.cpp`,
`src/plugins/media_player/MediaPlayerPlugin.hpp` cite spec paths in comments).
Resolution: phase 1 updates these references to the new archive paths,
annotated "(design history)" where a doc implies the target is current
guidance. Authoring living reference docs distilled from the shipped designs
(`docs/reference/external-api.md` first — it's a shipped public feature whose
only documentation is a design spec) is a wishlist follow-up, not part of this
cleanup.

## Out of Scope

- Fixing the 0.1.0 / 0.3.0 version mismatch in code (wishlist entry instead).
- Authoring `docs/reference/external-api.md` and similar living reference docs
  distilled from shipped designs (wishlist entry; see above).
- CHANGELOG.md (revisit at next tagged release).
- Restructuring `tools/` contents (README only).
- Any change under `libs/prodigy-oaa-protocol/proto/` (submodule).
- Workspace-level `E:\claude\CLAUDE.md` edits (separate follow-up: its OpenAuto
  section references paths this cleanup moves).

## Open Items

- Statuses for widevine-enablement and phase-f-light plans (verify during phase 1).
- Gotcha freshness: several CLAUDE.md gotchas date from the Qt 6.4/aqt era (e.g.
  the `loadFromModule` ban; the "H.264 already has AnnexB start codes" note is
  itself marked "may be outdated"). During phase 2 migration, verify each gotcha
  against the current Qt 6.8 environment instead of copying blindly; drop or
  re-word stale ones and note the change in the phase 2 commit message.
- Whether `docs/private/` should eventually be tracked or moved out of the repo
  entirely — explicitly not decided here.
- `.superpowers/sdd/` (untracked tool scratch) references old
  `docs/superpowers/...` paths. Decision: left as historical scratch; active
  work must not resume from those stale paths. Reset/clear it after the cleanup
  lands if it causes confusion.
