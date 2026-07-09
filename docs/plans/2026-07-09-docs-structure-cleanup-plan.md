# Docs & Repo Structure Cleanup — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

Status: ACTIVE
Date: 2026-07-09
Spec: `docs/plans/2026-07-09-docs-structure-cleanup-design.md` (approved by Matthew; Codex-reviewed, 9 findings adjudicated)

**Goal:** Restructure the repo's documentation and agent instructions per the approved spec: Diátaxis-lite docs tree, single `docs/archive/`, status headers on every plan, AGENTS.md as agent-instruction SSOT with nested subsystem files, and a contributor-ready public face.

**Architecture:** Three phases — (1) mechanical moves + status marking, (2) agent-instruction consolidation + new orientation docs, (3) public polish — followed by the standard pre-push Codex gate. Phase 1 is scripted `git mv` work; phases 2–3 are content authoring in the main session.

**Tech Stack:** git, bash, python3 (rotation + link-check scripts), markdown. No application code changes except two comment-line updates and one path constant in `tools/aa_proto_graph.py`.

## Global Constraints

- **Never touch** `libs/prodigy-oaa-protocol/proto/` (community submodule), `docs/private/` (untracked, intentional), `reviews/` (gitignored), `.superpowers/` (tool scratch), `build/`, `build-pi/`.
- **Archive over delete.** Only permitted deletions: `docs/.gitkeep` and emptied directory shells (`docs/plans/active/`, `docs/plans/archive/`, `docs/baselines/`, `docs/validation/`, `docs/session-handoffs/`, `docs/superpowers/`).
- Use `git mv` for every move (history preservation). Never `mv` + `git add`.
- Every moved/new plan file carries a `Status:` header (vocabulary: `ACTIVE`, `COMPLETED <YYYY-MM-DD>`, `PARKED — <reason>`, `ABANDONED — <reason>`). Exception: files under `docs/archive/plans/protobuf-source-docs/` were already archived before this cleanup; the directory-level archive rule covers them — do not header-edit them.
- Docs never state exact test counts — state the command (`ctest --output-on-failure`) instead.
- Commit per task with the message given in the task. **No pushes** — push happens only after the gate (Task 16) with Matthew's explicit go-ahead.
- One commit already sits unpushed on `dev` ahead of these (plus the two spec commits). Do not rebase or amend anything already committed.
- Archive content may keep historical references to old paths — that is acceptable and expected. Only **live** docs (everything outside `docs/archive/`) must have working links.

---

## Phase 1 — Move & mark

### Task 1: Directory scaffolding and straight moves

**Tier:** sonnet
**Files:**
- Create dirs: `docs/reference/`, `docs/aa-protocol/`, `docs/how-to/`, `docs/archive/plans/`, `docs/archive/session-handoffs/`, `docs/archive/validation/`, `docs/archive/research/`
- Move: 25 doc files + 2 screenshots (exact commands below)
- Delete: `docs/.gitkeep`

**Interfaces:**
- Produces: the new directory skeleton every later task assumes. No file content changes in this task.

- [ ] **Step 1: Create directories and run all moves**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
mkdir -p docs/reference docs/aa-protocol docs/how-to \
         docs/archive/plans docs/archive/session-handoffs \
         docs/archive/validation docs/archive/research

# AA protocol docs (8)
git mv docs/aa-apk-deep-dive.md docs/aa-protocol/
git mv docs/aa-display-rendering.md docs/aa-protocol/
git mv docs/aa-phone-side-debug.md docs/aa-protocol/
git mv docs/aa-troubleshooting-runbook.md docs/aa-protocol/
git mv docs/aa-video-resolution.md docs/aa-protocol/
git mv docs/android-auto-protocol-cross-reference.md docs/aa-protocol/
git mv docs/apk-indexing.md docs/aa-protocol/
git mv docs/apk-proto-reference.md docs/aa-protocol/

# Reference (7)
git mv docs/config-schema.md docs/reference/
git mv docs/plugin-api.md docs/reference/
git mv docs/widget-developer-guide.md docs/reference/
git mv docs/web-widget-authoring.md docs/reference/
git mv docs/settings-tree.md docs/reference/
git mv docs/state-matrix.md docs/reference/
git mv docs/release-packaging.md docs/reference/

# How-to (2)
git mv docs/debugging-notes.md docs/how-to/
git mv docs/testing-reconnect.md docs/how-to/

# Research history (2)
git mv docs/hfp-stack-spike.md docs/archive/research/
git mv docs/proto-validation-report.md docs/archive/research/

# Validation history (3) — then remove emptied dirs
git mv docs/baselines/2026-02-26-video-pipeline-baseline.md docs/archive/validation/
git mv docs/validation/phase18-hardware-validation-20260316-093435.log docs/archive/validation/
git mv docs/validation/phase18-revalidation-20260316-104756.log docs/archive/validation/
rmdir docs/baselines docs/validation

# Vestigial per-session handoff files (3)
git mv docs/session-handoffs/2026-03-10-home-screen-polish.md docs/archive/session-handoffs/
git mv docs/session-handoffs/2026-03-10-widget-polish-continued.md docs/archive/session-handoffs/
git mv docs/session-handoffs/2026-03-11-longpress-back-wip.md docs/archive/session-handoffs/
rmdir docs/session-handoffs

# OAP archive rename (whole tree, content unchanged; needs-review/ triage happens in Task 7)
git mv docs/OpenAutoPro_archive_information docs/archive/openauto-pro

# Pre-existing plan archive
git mv docs/plans/archive/protobuf-source-docs docs/archive/plans/protobuf-source-docs
rmdir docs/plans/archive

# Root screenshots
git mv pi-screenshot.png assets/
git mv pi-screenshot2.png assets/

# Dead weight
git rm docs/.gitkeep
```

- [ ] **Step 2: Verify the moves**

```bash
git status --short | grep -c "^R" # expect: 25+ renames (R lines)
ls docs/aa-protocol | wc -l       # expect: 8
ls docs/reference | wc -l         # expect: 7
ls docs/how-to | wc -l            # expect: 2
ls docs/baselines docs/validation docs/session-handoffs docs/OpenAutoPro_archive_information 2>&1 | grep -c "No such" # expect: 4
test -f docs/.gitkeep && echo FAIL || echo OK   # expect: OK
```

- [ ] **Step 3: Commit**

```bash
git add -A && git commit -m "docs: restructure docs tree — reference/, aa-protocol/, how-to/, archive/ (moves only)"
```

### Task 2: Plan dispositions and status headers

**Tier:** sonnet
**Files:**
- Move: all `docs/plans/2026-02-*.md`, `docs/plans/2026-03-*.md`, `docs/plans/milestone-*.md` → `docs/archive/plans/`
- Move: `docs/plans/active/*` → `docs/plans/`; dissolve `docs/superpowers/`
- Modify: first lines of every moved plan file (status header insertion)

**Interfaces:**
- Consumes: directory skeleton from Task 1.
- Produces: `docs/plans/` containing ONLY the 6 live files listed below; everything else statused + archived.

The status table below was verified against `docs/session-handoffs.md`, `docs/roadmap-current.md`, and the milestone docs on 2026-07-09 by the main session. Do not re-derive it; apply it. The two files marked VERIFY need a one-line evidence check (grep the named source); if evidence is absent, use `ABANDONED — unverified, archived 2026-07-09`.

- [ ] **Step 1: Move the plan files**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
# Legacy Feb–Mar plans + milestone docs → archive (38 files; the 2026-07-09 cleanup design/plan stay put)
git mv docs/plans/2026-02-*.md docs/plans/2026-03-*.md docs/plans/milestone-*.md docs/archive/plans/

# Formerly-"active" pair → live plans dir (PARKED)
git mv docs/plans/active/2026-02-21-config-contract-overhaul-design.md docs/plans/
git mv docs/plans/active/2026-02-21-config-contract-overhaul-plan.md docs/plans/
rmdir docs/plans/active

# Superpowers dissolution — exceptions first
git mv docs/superpowers/specs/2026-07-08-media-player-design.md docs/plans/
git mv docs/superpowers/plans/2026-07-05-phase-f-light-plans.md docs/plans/
git mv docs/superpowers/specs/2026-07-05-webengine-spike-results.md docs/archive/research/
# Everything remaining → archive/plans (incl. README-executor-handbook.md — its content is merged in Task 6)
git mv docs/superpowers/specs/*.md docs/archive/plans/
git mv docs/superpowers/plans/*.md docs/archive/plans/
rmdir docs/superpowers/specs docs/superpowers/plans docs/superpowers
```

- [ ] **Step 2: Write the status map and apply headers**

Save as `/tmp/status_map.tsv` (TAB-separated: path, status line). Then run the script below.

```text
docs/plans/2026-02-21-config-contract-overhaul-design.md	Status: PARKED — needs re-triage (approved 2026-02-21, never executed)
docs/plans/2026-02-21-config-contract-overhaul-plan.md	Status: PARKED — needs re-triage (approved 2026-02-21, never executed)
docs/plans/2026-07-08-media-player-design.md	Status: ACTIVE (stage 1 shipped 2026-07-09; stages 2+ pending)
docs/plans/2026-07-05-phase-f-light-plans.md	Status: ACTIVE (media player done; EQ parity audit, 0x8012 experiment, key-event nav remain)
docs/archive/research/2026-07-05-webengine-spike-results.md	Status: COMPLETED 2026-07-05
docs/archive/plans/2026-07-05-fable-work-program-design.md	Status: COMPLETED 2026-07-07 (program executed; see roadmap-current.md)
docs/archive/plans/2026-07-05-extensibility-architecture-design.md	Status: COMPLETED 2026-07-07 (rails distilled into AGENTS.md 2026-07-09)
docs/archive/plans/2026-07-05-dashboards-overlays-design.md	Status: COMPLETED 2026-07-06
docs/archive/plans/2026-07-05-hfp-call-audio-design.md	Status: COMPLETED 2026-07-06 (live checks L3–L5 tracked in roadmap-current.md)
docs/archive/plans/2026-07-05-hfp-call-control-decision.md	Status: COMPLETED 2026-07-05 (decision record)
docs/archive/plans/2026-07-06-external-api-v1-design.md	Status: COMPLETED 2026-07-06
docs/archive/plans/2026-07-06-js-runtime-design.md	Status: COMPLETED 2026-07-07
docs/archive/plans/2026-07-07-theme-upload-design.md	Status: COMPLETED 2026-07-07
docs/archive/plans/2026-07-07-theme-upload-context-notes.md	Status: COMPLETED 2026-07-07
docs/archive/plans/2026-07-07-web-surface-strategy-design.md	Status: COMPLETED 2026-07-07 (slice 1 verified on Pi; later slices tracked in roadmap-current.md)
docs/archive/plans/2026-07-09-tiered-execution-codex-gate-design.md	Status: COMPLETED 2026-07-09 (distilled into AGENTS.md § Tiered Execution Workflow)
docs/archive/plans/2026-07-05-hfp-call-audio-implementation.md	Status: COMPLETED 2026-07-06
docs/archive/plans/2026-07-05-multi-dashboards-implementation.md	Status: COMPLETED 2026-07-06
docs/archive/plans/2026-07-05-overlay-framework-implementation.md	Status: COMPLETED 2026-07-06
docs/archive/plans/2026-07-06-external-api-v1-implementation.md	Status: COMPLETED 2026-07-06
docs/archive/plans/2026-07-06-js-runtime-implementation.md	Status: COMPLETED 2026-07-07
docs/archive/plans/2026-07-07-theme-upload-implementation.md	Status: COMPLETED 2026-07-07
docs/archive/plans/2026-07-07-widevine-enablement.md	Status: COMPLETED 2026-07-07 (verified on Pi; roadmap-current.md "Widevine enablement")
docs/archive/plans/2026-07-08-media-player-stage1.md	Status: COMPLETED 2026-07-09 (bench complete, Pi-deployed)
docs/archive/plans/2026-07-09-tiered-execution-codex-gate.md	Status: COMPLETED 2026-07-09
docs/archive/plans/README-executor-handbook.md	Status: COMPLETED 2026-07-09 (superseded by docs/plans/README.md)
docs/archive/plans/milestone-01-foundation.md	Status: COMPLETED 2026-02-18
docs/archive/plans/milestone-02-aa-integration.md	Status: COMPLETED 2026-02-21
docs/archive/plans/milestone-03-companion-system.md	Status: COMPLETED 2026-02-23
docs/archive/plans/milestone-04-protocol-correctness.md	Status: COMPLETED 2026-02-25
docs/archive/plans/milestone-05-av-optimization.md	Status: COMPLETED 2026-02-26
```

Every OTHER `docs/archive/plans/2026-02-*.md` and `2026-03-*.md` file (the ~33 legacy design/plan files) gets `Status: COMPLETED — archived 2026-07-09 (see milestone docs + session-handoffs archive)`. Two VERIFY exceptions:
- `2026-02-21-architecture-extensibility-plan.md` — evidence: milestone-02 doc mentions its outcome. If `grep -il "extensib" docs/archive/plans/milestone-02-aa-integration.md` is empty AND session-handoffs archive has no match, use `ABANDONED — unverified, archived 2026-07-09`.
- `2026-03-15-proxy-routing-fix-change-request.md` — evidence: `grep -in "proxy" docs/session-handoffs.md docs/archive/plans/milestone-05-av-optimization.md`. Same fallback.

Header-application script — save as `/tmp/apply_status.py`, run `python3 /tmp/apply_status.py`:

```python
#!/usr/bin/env python3
"""Insert or replace a 'Status:' line in plan files, right after the first heading."""
import sys, re, pathlib

REPO = pathlib.Path("/mnt/e/claude/personal/openautopro/openauto-prodigy")
DEFAULT = "Status: COMPLETED — archived 2026-07-09 (see milestone docs + session-handoffs archive)"

explicit = {}
for line in (pathlib.Path("/tmp/status_map.tsv").read_text().strip().splitlines()):
    path, status = line.split("\t")
    explicit[path.strip()] = status.strip()

targets = set(explicit)
for f in (REPO / "docs/archive/plans").glob("2026-0[23]-*.md"):
    targets.add(str(f.relative_to(REPO)))

for rel in sorted(targets):
    p = REPO / rel
    if not p.exists():
        print(f"MISSING: {rel}"); continue
    status = explicit.get(rel, DEFAULT)
    lines = p.read_text().splitlines()
    # replace an existing Status: line within the first 10 lines, else insert after first heading
    for i, l in enumerate(lines[:10]):
        if re.match(r"^\**Status\**\s*:", l, re.IGNORECASE):
            lines[i] = status; break
    else:
        for i, l in enumerate(lines):
            if l.startswith("#"):
                lines.insert(i + 1, ""); lines.insert(i + 2, status); break
        else:
            lines.insert(0, status)
    p.write_text("\n".join(lines) + "\n")
    print(f"OK: {rel} -> {status[:60]}")
```

- [ ] **Step 3: Verify**

```bash
ls docs/plans/         # expect EXACTLY: 2026-02-21-config-contract-overhaul-design.md,
                       # 2026-02-21-config-contract-overhaul-plan.md, 2026-07-05-phase-f-light-plans.md,
                       # 2026-07-08-media-player-design.md, 2026-07-09-docs-structure-cleanup-design.md,
                       # 2026-07-09-docs-structure-cleanup-plan.md   (README.md arrives in Task 6)
grep -L "^Status:" docs/archive/plans/*.md | grep -v protobuf-source-docs  # expect: empty
grep -c "^Status:" docs/plans/*.md   # expect: 1 per file (design+plan for this cleanup already have one)
```

- [ ] **Step 4: Commit**

```bash
git add -A && git commit -m "docs: consolidate plans into docs/plans + docs/archive/plans with status headers"
```

### Task 3: Session-handoffs rotation

**Tier:** sonnet
**Files:**
- Modify: `docs/session-handoffs.md` (keep 2026-07 entries)
- Create: `docs/archive/session-handoffs/2026-02--2026-03-handoffs.md`

**Interfaces:**
- Consumes: `docs/archive/session-handoffs/` dir from Task 1.
- Produces: a live handoff log containing only `## 2026-07-*` entries plus a pointer line to the archive.

**CRITICAL:** the file is NOT chronologically ordered — Feb/Mar and Jul entries are interleaved (Jul at both top and bottom). Rotation MUST select entries by their `## YYYY-MM-DD` header date, never by line ranges.

- [ ] **Step 1: Run the rotation script**

Save as `/tmp/rotate_handoffs.py`, run `python3 /tmp/rotate_handoffs.py`:

```python
#!/usr/bin/env python3
import re, pathlib

REPO = pathlib.Path("/mnt/e/claude/personal/openautopro/openauto-prodigy")
SRC = REPO / "docs/session-handoffs.md"
DST = REPO / "docs/archive/session-handoffs/2026-02--2026-03-handoffs.md"

text = SRC.read_text()
parts = re.split(r"(?m)^(?=## )", text)          # parts[0] = preamble, rest = entries
preamble, entries = parts[0], parts[1:]

keep, rotate = [], []
for e in entries:
    m = re.match(r"## (\d{4}-\d{2})-\d{2}", e)
    (rotate if m and m.group(1) in ("2026-02", "2026-03") else keep).append(e)

assert rotate, "nothing matched Feb/Mar — aborting"
pointer = ("> Older entries (2026-02 / 2026-03) are archived in "
           "`docs/archive/session-handoffs/2026-02--2026-03-handoffs.md`.\n\n")
SRC.write_text(preamble + pointer + "".join(keep))
DST.write_text("# Session Handoffs — 2026-02 / 2026-03 (archived 2026-07-09)\n\n"
               "Status: COMPLETED — rotated out of docs/session-handoffs.md\n\n" + "".join(rotate))
print(f"kept {len(keep)} entries, rotated {len(rotate)}")
```

- [ ] **Step 2: Verify**

```bash
grep -c "^## 2026-0[23]" docs/session-handoffs.md                                   # expect: 0
grep -c "^## 2026-07" docs/session-handoffs.md                                      # expect: 16
grep -c "^## " docs/archive/session-handoffs/2026-02--2026-03-handoffs.md           # expect: 30
wc -l docs/session-handoffs.md                                                      # expect: roughly 500–600
```
(16 + 30 = the 46 entries currently in the file; re-count with `grep -c "^## " docs/session-handoffs.md` BEFORE rotating and confirm kept+rotated equals that number — the script's print line reports both.)

- [ ] **Step 3: Commit**

```bash
git add docs/session-handoffs.md docs/archive/session-handoffs/
git commit -m "docs: rotate Feb-Mar session handoffs to archive"
```

### Task 4: Reference sweep — fix every live pointer to a moved path

**Tier:** sonnet
**Files:**
- Modify: `AGENTS.md`, `docs/design-philosophy.md`, `docs/development.md`, `docs/roadmap-current.md`, `docs/session-handoffs.md`, `docs/wishlist.md`, `docs/reference/web-widget-authoring.md`, `scripts/codex-review.sh`, `src/core/services/PhoneStateService.hpp`, `src/main.cpp`, `src/plugins/media_player/MediaPlayerPlugin.hpp`, `docs/aa-protocol/aa-troubleshooting-runbook.md`, `tools/aa_proto_graph.py`
- Do NOT edit: anything under `docs/archive/` (historical references stay), `docs/INDEX.md` (rewritten wholesale in Task 6), `CLAUDE.md` (stubbed in Task 12), the two cleanup design/plan files (self-referencing spec text is fine).

**Interfaces:**
- Consumes: final locations from Tasks 1–3.
- Produces: zero live references to any pre-move path.

- [ ] **Step 1: Apply the path rewrites**

Old → new mappings (apply in both `docs/…` absolute and bare-relative link forms; check each file listed above):

```text
docs/superpowers/specs/2026-07-08-media-player-design.md   -> docs/plans/2026-07-08-media-player-design.md
docs/superpowers/plans/2026-07-05-phase-f-light-plans.md   -> docs/plans/2026-07-05-phase-f-light-plans.md
docs/superpowers/specs/2026-07-05-webengine-spike-results.md -> docs/archive/research/2026-07-05-webengine-spike-results.md
docs/superpowers/specs/<anything else>                     -> docs/archive/plans/<same filename>
docs/superpowers/plans/<anything else>                     -> docs/archive/plans/<same filename>
docs/web-widget-authoring.md                               -> docs/reference/web-widget-authoring.md
docs/widget-developer-guide.md                             -> docs/reference/widget-developer-guide.md
docs/config-schema.md                                      -> docs/reference/config-schema.md
docs/plugin-api.md                                         -> docs/reference/plugin-api.md
docs/settings-tree.md                                      -> docs/reference/settings-tree.md
docs/state-matrix.md                                       -> docs/reference/state-matrix.md
docs/release-packaging.md                                  -> docs/reference/release-packaging.md
docs/debugging-notes.md                                    -> docs/how-to/debugging-notes.md
docs/testing-reconnect.md                                  -> docs/how-to/testing-reconnect.md
docs/hfp-stack-spike.md                                    -> docs/archive/research/hfp-stack-spike.md
docs/aa-<name>.md, docs/apk-<name>.md, docs/android-auto-protocol-cross-reference.md
                                                           -> docs/aa-protocol/<same filename>
docs/OpenAutoPro_archive_information/                      -> docs/archive/openauto-pro/
docs/plans/milestone-0N-<name>.md                          -> docs/archive/plans/milestone-0N-<name>.md
```

Specific known edits (verify each with grep before editing; contexts may have shifted):
1. `AGENTS.md` line ~21: design link → `docs/archive/plans/2026-07-09-tiered-execution-codex-gate-design.md`.
2. `scripts/codex-review.sh` line ~5 (comment): same design path update.
3. `src/core/services/PhoneStateService.hpp`, `src/main.cpp`, `src/plugins/media_player/MediaPlayerPlugin.hpp`: comments citing `docs/superpowers/...` spec paths → new archive/plans (or docs/plans for media-player design) paths. Comment-only changes; no code.
4. `docs/reference/web-widget-authoring.md`: links to External API / JS-runtime designs → `docs/archive/plans/...` and annotate the link text with "(design history)".
5. `docs/aa-protocol/aa-troubleshooting-runbook.md` line ~6: replace the dead `docs/aa-protocol-reference.md` pointer with: `**Protocol reference:** generate with \`python3 tools/aa_proto_graph.py\` → \`docs/aa-protocol/protocol-reference.md\` (untracked, generated); protocol definitions live in the open-android-auto repo.`
6. `tools/aa_proto_graph.py` line ~30: `OUTPUT_MD = PROJECT_ROOT / "docs" / "aa-protocol-reference.md"` → `OUTPUT_MD = PROJECT_ROOT / "docs" / "aa-protocol" / "protocol-reference.md"`; also update the docstring path at line ~14. Add `docs/aa-protocol/protocol-reference.md` to `.gitignore` (generated artifact).
7. `docs/session-handoffs.md` (live July entries only): update `superpowers/...` relative links per the mapping (as links relative to `docs/`, e.g. `archive/plans/...`).
8. `docs/roadmap-current.md`, `docs/wishlist.md`, `docs/development.md`, `docs/design-philosophy.md`: apply mapping wherever grep hits.

- [ ] **Step 2: Verify zero live stale references**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git grep -nE "superpowers/(specs|plans)" -- ':!docs/archive' ':!docs/plans/2026-07-09-docs-structure-cleanup*' ':!CLAUDE.md'   # expect: empty
git grep -n "OpenAutoPro_archive_information" -- ':!docs/archive'                                                             # expect: empty
git grep -n 'docs/aa-protocol-reference' -- ':!docs/archive'                                                                  # expect: empty
for f in web-widget-authoring widget-developer-guide config-schema plugin-api settings-tree state-matrix release-packaging debugging-notes testing-reconnect hfp-stack-spike; do
  git grep -n "docs/$f" -- ':!docs/archive' ':!docs/plans/2026-07-09-docs-structure-cleanup*' ':!CLAUDE.md'; done              # expect: empty
python3 tools/aa_proto_graph.py --help >/dev/null 2>&1 || python3 -c "import ast; ast.parse(open('tools/aa_proto_graph.py').read())"  # expect: parses
```
(`CLAUDE.md` is excluded because Task 12 replaces it wholesale; the cleanup design/plan quote old paths as spec content.)

- [ ] **Step 3: Commit**

```bash
git add -A && git commit -m "docs: fix all live references to relocated docs; retarget aa_proto_graph output"
```

### Task 5: needs-review triage checkpoint (Matthew)

**Tier:** main (requires Matthew via AskUserQuestion)
**Files:**
- Move/delete per Matthew's decisions: `docs/archive/openauto-pro/needs-review/{aa-proxy-rs-connection-troubleshooting.md, codex_architecture_corrections.md, miata-hardware-reference.md, wireless-aa-initial-handshake-findings.md}`

**Interfaces:**
- Consumes: `docs/archive/openauto-pro/` from Task 1.
- Produces: a final disposition for each of the 4 files; `needs-review/` dir gone (contents either kept-in-archive proper, moved out of repo, or deleted with approval).

- [ ] **Step 1: Present each file to Matthew** — one AskUserQuestion covering the four files with options: keep in `docs/archive/openauto-pro/` (promote out of needs-review), move out of this repo (for `miata-hardware-reference.md`, suggest `E:\claude\personal\miata\`), or delete. Summarize each file's content in 1–2 lines first (read them).
- [ ] **Step 2: Apply decisions** — `git mv` / `git rm` / filesystem copy per answer; `rmdir docs/archive/openauto-pro/needs-review` once empty. If a file leaves the repo, note its new location in the commit message.
- [ ] **Step 3: Commit**

```bash
git add -A && git commit -m "docs: triage needs-review archive files per Matthew's dispositions"
```

### Task 6: INDEX.md rewrite + docs/plans/README.md

**Tier:** main
**Files:**
- Rewrite: `docs/INDEX.md`
- Create: `docs/plans/README.md` (merging refreshed executor-handbook content)

**Interfaces:**
- Consumes: final tree from Tasks 1–5.
- Produces: the two navigation/convention docs everything else points at.

- [ ] **Step 1: Rewrite `docs/INDEX.md`** with this structure (fill file lists from the actual final tree — `ls` each dir; one line + em-dash description per doc, matching the current INDEX's style):

```markdown
# Documentation Index

## Start Here
README.md (root) · architecture.md · development.md · wireless-setup.md

## Project Management
project-vision.md · roadmap-current.md · session-handoffs.md (recent; older → archive/session-handoffs/) · wishlist.md · design-decisions.md · design-philosophy.md

## Reference (reference/)
[the 7 files]

## AA Protocol (aa-protocol/)
[the 8 files] + note: protocol-reference.md is generated by tools/aa_proto_graph.py (untracked); protocol definitions live in open-android-auto

## How-To (how-to/)
[the 2 files]

## Plans (plans/)
plans/README.md — conventions + executor guidance. Live plans listed with status. Convention: only ACTIVE files are current guidance.

## Pi Configuration (pi-config/)

## Archive (archive/)
Everything under archive/ is history, not guidance. Subdirs: plans/ (incl. milestone-01..05 history), session-handoffs/, validation/, research/, openauto-pro/.
```

Note: architecture.md is referenced but created in Task 9 — acceptable within the same pre-push batch; the Task 15 link check runs after it exists.

- [ ] **Step 2: Create `docs/plans/README.md`** — plan conventions section (status vocabulary verbatim from the spec; "completion flips the header and moves the file to `docs/archive/plans/` in the same commit"; new plans/specs from brainstorming/writing-plans are saved HERE, not docs/superpowers/) followed by the executor-handbook content **refreshed** — carry over: canonical-docs table, precedence rule, picking-up-a-plan steps, verification workflow, when-things-go-sideways, wishlist-then-promote. Mandatory freshness fixes while merging (source: `docs/archive/plans/README-executor-handbook.md`):
  - "~88 tests" → state the command, never a count.
  - "QML changes go via `git pull` on the Pi (not in the binary)" → **wrong**; QML ships inside the binary (verified 2026-07-08) — QML changes require cross-build + binary rsync.
  - "sprint output lives on `fable-design-sprint`" → the working branch is `dev` (single-branch workflow since 2026-07-06).
  - "Gotchas … `CLAUDE.md`" → "root `AGENTS.md` + the nested `AGENTS.md` beside the code you're editing" (lands in Tasks 10–12, same batch).
  - Sprint-specific §5 dependency map: drop (historical; lives in the archived original).
  - Standing guardrails §4 (frozen proto/api, HF/AG roles, no-ofono, rails R1–R5, frozen numerics): do NOT copy here — they migrate into AGENTS.md in Task 10. Leave a pointer line.
- [ ] **Step 3: Verify** — every path named in the new INDEX resolves: `grep -oE '[a-zA-Z0-9_./-]+\.md' docs/INDEX.md | sort -u | while read f; do test -f "docs/$f" -o -f "$f" || echo "MISSING $f"; done` → only `architecture.md` (created Task 9) may appear.
- [ ] **Step 4: Commit**

```bash
git add docs/INDEX.md docs/plans/README.md
git commit -m "docs: rewrite INDEX for new tree; add plans README with conventions + refreshed executor guidance"
```

---

## Phase 2 — Agent instructions & orientation docs

### Task 7: docs/architecture.md

**Tier:** main
**Files:**
- Create: `docs/architecture.md`
- Source material: `README.md` §Architecture + §Runtime Data Flow (move, don't duplicate — README's copy is deleted in Task 13); `CLAUDE.md` §Architecture, §Hardware table

**Interfaces:**
- Produces: the system map that AGENTS.md, README, and INDEX point to.

- [ ] **Step 1: Write `docs/architecture.md`** with sections: **Main Components** (composition root `src/main.cpp`; core services incl. ApiServer; plugin layer incl. media_player — enumerate from `ls src/plugins/`, don't trust the spec's list of 3; protocol library; UI layer; web config), **Boundaries** (protocol ASIO threads never touch Qt UI — `QMetaObject::invokeMethod(Qt::QueuedConnection)` bridge; plugins reach services only via `IHostContext`; web config writes only through `IpcServer`; External API binds providers/services, never EventBus topics, D-Bus paths, or AA protocol internals; all mutation through ActionRegistry or explicit invokables), **Runtime Data Flow** (wireless AA session path, 7 steps from README), **Threading Model** (Qt main / ASIO protocol / decode worker / EvdevTouchReader QThread), **Target Hardware** (table from CLAUDE.md), **External Systems** (BlueZ D-Bus, PipeWire, hostapd/dnsmasq, phone's AA client).
- [ ] **Step 2: Verify claims against code** — for each named class/file, `git grep -l "<ClassName>"` must hit; fix any that moved.
- [ ] **Step 3: Commit** — `git add docs/architecture.md && git commit -m "docs: add architecture.md — components, boundaries, data flow, threading, hardware"`

### Task 8: Root AGENTS.md rewrite (SSOT)

**Tier:** main
**Files:**
- Rewrite: `AGENTS.md` (keep ≤ ~150 lines)
- Source material: current `AGENTS.md` (workflow content survives), `CLAUDE.md` (commands, constraints), archived executor handbook §4 (guardrails)

**Interfaces:**
- Produces: the instruction file every agent (Claude, Codex) reads; nested files from Task 9 are listed in it by exact path.

- [ ] **Step 1: Rewrite `AGENTS.md`** in this order:
  1. **Hard constraints** (top): `libs/prodigy-oaa-protocol/` submodule is hands-off; `proto/api/` is FROZEN additive-only since `875feaf` (field numbers never reused, new capability = new field + capability flag); wireless-only AA (no USB/libusb); Qt 6.8 system packages (WSL2 Trixie dev = Pi target); Pi is HFP Hands-Free 0x111e never AG 0x111f; no ofono — telephony via `org.pipewire.Telephony`; External API rails (binds providers/services only; mutation only through ActionRegistry/invokables; additive proto; JS shim ≤ public API); frozen numerics (ICallStateProvider values, overlay z-bands, DashboardContributionKind order — append, never renumber).
  2. **Overview + tech stack** (3–4 lines, from CLAUDE.md intro).
  3. **Commands**: local build/test (`cd build && cmake --build . -j$(nproc)`, `ctest --output-on-failure`), the ctest-doesn't-compile-main.cpp warning, cross-build (`./cross-build.sh`, not toolchain file directly), Pi deploy rsync + service restart + force-kill fallback (from CLAUDE.md §Pi Deployment; QML ships in-binary — UI changes need rebuild+rsync).
  4. **Workflow** (existing §Project Management Loop, §Tiered Execution Workflow, §Review gate — carry over verbatim minus the CLAUDE.md-inheritance line, which becomes "workers read this file and the nested AGENTS.md nearest their working files").
  5. **Nested instructions**: list `src/AGENTS.md`, `src/core/aa/AGENTS.md`, `libs/prodigy-oaa-protocol/AGENTS.md`, `qml/AGENTS.md` — "read the nearest one before editing that subsystem" (explicit because not all tooling auto-loads nested files).
  6. **Docs conventions**: plan status vocabulary; new plans/specs → `docs/plans/`; only ACTIVE = guidance; archive rule; handoff-rotation rule (>~300 lines → rotate oldest month); doc-update rule; no test counts in docs; wishlist-then-promote.
- [ ] **Step 2: Verify** — `wc -l AGENTS.md` ≤ ~160; `git grep -n "CLAUDE.md" AGENTS.md` → empty.
- [ ] **Step 3: Commit** — `git add AGENTS.md && git commit -m "docs: rewrite AGENTS.md as agent-instruction SSOT"`

### Task 9: Nested AGENTS.md files (4)

**Tier:** main
**Files:**
- Create: `src/AGENTS.md`, `src/core/aa/AGENTS.md`, `libs/prodigy-oaa-protocol/AGENTS.md`, `qml/AGENTS.md`
- Source material: `CLAUDE.md` §Gotchas, §AA Protocol: Touch Events, §AA Protocol: Video Resolutions

**Interfaces:**
- Consumes: the root AGENTS.md list from Task 8 (paths must match exactly).
- Produces: subsystem rule files.

Content mapping (migrate each bullet, **verifying freshness against the current Qt 6.8 / current-code environment** — do not copy blindly; drop or reword stale items and list any dropped ones in the commit message):

- [ ] **Step 1: `src/AGENTS.md`** — QTimer needs `#include <QTimer>`; QTimer only works on Qt-event-loop threads (ASIO → `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`); Q_OBJECT header-only classes need a .cpp in CMakeLists for MOC; QColor links `Qt6::Gui`; QVideoFrame is ref-counted — never reuse buffers; QDBusArgument can't extract QVariantMap — manual `beginMap()/endMap()` with QDBusVariant; PipeWire playback always outputs full periods (`chunk->size = maxSize`, silence-fill); `SPA_DICT_INIT_ARRAY` inline → use named arrays.
- [ ] **Step 2: `src/core/aa/AGENTS.md`** — touch coords in VIDEO resolution space (1280x720) not touch_screen_config; ALL active pointers in every message; `action_index` = array index, `pointer_id` = stable slot id; UP events include lifted finger at last position; `touch_screen_config` must equal video resolution; SPS/PPS arrives as AV_MEDIA_INDICATION (no timestamp) — forward to decoder; AnnexB note (flag: marked "may be outdated" — verify against open-androidauto before relying); phone sends `config_index=3` regardless of our list; FFmpeg `thread_count=1` for real-time decode; accept both YUV420P and YUVJ420P; `margin_width/height` works, locked at session start; EVIOCGRAB toggles with AA connection state; sidebar touch = evdev hit zones, QML visual-only; TCP dead-peer detection via `tcp_info` `tcpi_backoff >= 3`; only `<netinet/tcp.h>`; ASIO sockets need explicit FD_CLOEXEC; SO_REUSEADDR before bind (separate open/set_option/bind/listen); Boost.Log truncates multiline — `ShortDebugString()`; AA fixed resolutions list; 3-finger gesture semantics.
- [ ] **Step 3: `libs/prodigy-oaa-protocol/AGENTS.md`** — `proto/` subdirectory is a hands-off community submodule (note needed changes, never edit here); protocol behavior gotchas live in `src/core/aa/AGENTS.md`; proto generation happens from `proto/oaa/*.proto` at build time.
- [ ] **Step 4: `qml/AGENTS.md`** — `loadFromModule` ban: **verify first** (gotcha dates from Qt 6.4/aqt era; env is 6.8 — if it works now, drop the ban and say so in the commit); NEVER pointer-handlers over WebEngineView; QML ships in-binary (qt_add_qml_module + qmlcache) — changes require rebuild + binary rsync, git pull won't update UI; sidebar MouseArea visual-only during AA.
- [ ] **Step 5: Verify** — each file ≤ ~60 lines; paths match Task 8's list exactly; commit message lists any gotchas dropped/reworded as stale.
- [ ] **Step 6: Commit** — `git add src/AGENTS.md src/core/aa/AGENTS.md libs/prodigy-oaa-protocol/AGENTS.md qml/AGENTS.md && git commit -m "docs: add nested AGENTS.md subsystem rule files"`

### Task 10: CLAUDE.md stub + CLAUDE.md-reference sweep

**Tier:** main
**Files:**
- Rewrite: `CLAUDE.md` (~10 lines)
- Modify: `src/core/aa/NightModeProvider.cpp:2`, `src/ui/DashboardManager.hpp:108` (comment text only)

**Interfaces:**
- Consumes: AGENTS.md + nested files (Tasks 8–9) must exist first.

- [ ] **Step 1: Replace `CLAUDE.md`** with exactly this content:

```markdown
# CLAUDE.md

**`AGENTS.md` is the source of truth for this repo** — constraints, commands,
workflow, and conventions live there. Read it first.

Subsystem rules live in nested AGENTS.md files — read the nearest one before
editing: `src/AGENTS.md`, `src/core/aa/AGENTS.md`,
`libs/prodigy-oaa-protocol/AGENTS.md`, `qml/AGENTS.md`.

Documentation map: `docs/INDEX.md`. Architecture: `docs/architecture.md`.
```

- [ ] **Step 2: Sweep remaining CLAUDE.md references** — `git grep -n "CLAUDE.md" -- ':!docs/archive' ':!CLAUDE.md' ':!docs/plans/2026-07-09-docs-structure-cleanup*'`; update each hit (known: the two source comments → "See src/AGENTS.md gotchas" / "src/AGENTS.md gotcha: QTimer needs a real #include"; any hit in docs/plans/README.md from Task 6). Expect zero hits after.
- [ ] **Step 3: Build check** (comments only, but prove it): `cd build && cmake --build . -j$(nproc) 2>&1 | tail -3` → success.
- [ ] **Step 4: Commit** — `git add -A && git commit -m "docs: reduce CLAUDE.md to AGENTS.md pointer stub; retarget gotcha references"`

### Task 11: tests/, scripts/, tools/ READMEs + wishlist entries

**Tier:** main
**Files:**
- Create: `tests/README.md`, `scripts/README.md`, `tools/README.md`
- Modify: `docs/wishlist.md` (3 new entries)

- [ ] **Step 1: `tests/README.md`** (~25 lines) — how to run (`cd build && ctest --output-on-failure`; single test: `ctest -R <name> -V`); **the trap in bold: ctest does NOT compile `main.cpp` — always build the app target (`cmake --build . --target openauto-prodigy`) before claiming green**; what's host-runnable vs hardware-dependent (BT/Pi); where tests live and naming convention (survey `ls tests/` while writing).
- [ ] **Step 2: `scripts/README.md`** (~20 lines) — `codex-review.sh`: reviews `@{upstream}..HEAD` read-only, findings → gitignored `reviews/`, exit codes 0 ok / 1 usage / 2 codex missing / 4 codex failed, explicit range arg; `validate-resolutions.sh` (read it, describe in one line); `check-doc-links.py` (added in Task 15); note all are safe/read-only.
- [ ] **Step 3: `tools/README.md`** (~30 lines) — proto tooling map: `aa_proto_graph.py` → generates untracked `docs/aa-protocol/protocol-reference.md` + `aa_proto_graph.json`; parser/validator chain (`proto_parser.py`, `proto_comparator.py`, `match_loader.py`, `validate_protos.py`, outputs `proto_matches.json`, `proto_validation_report.json`); `descriptor_decoder.py`, `proto_decoder.py`; `package-prebuilt-release.sh` (writes `dist/`); `gen-proto-js.sh`, `proto-usage-report.sh`, `token-preview.sh`; `eme-probe/`, `spike-qmp-tap/` (spike dirs); `test_*.py` run via pytest. Mark inputs vs generated outputs; everything safe to re-run.
- [ ] **Step 4: Wishlist entries** — append to `docs/wishlist.md`: (1) fix 0.1.0/0.3.0 version mismatch (CMakeLists.txt + src/main.cpp vs YamlConfig default); (2) author `docs/reference/external-api.md` distilled from the archived External API v1 design (shipped public feature, currently design-doc-only); (3) re-triage the PARKED config-contract overhaul (`docs/plans/2026-02-21-...`).
- [ ] **Step 5: Commit** — `git add tests/README.md scripts/README.md tools/README.md docs/wishlist.md && git commit -m "docs: add tests/scripts/tools READMEs; wishlist follow-ups from structure cleanup"`

---

## Phase 3 — Public polish

### Task 12: README rewrite

**Tier:** main
**Files:**
- Rewrite: `README.md`

- [ ] **Step 1: Rewrite** with: title + license badge + one-paragraph pitch (open-source wireless AA head unit for Raspberry Pi, clean-room OpenAuto Pro successor); both screenshots (`assets/pi-screenshot.png`, `assets/pi-screenshot2.png`); **date-free** feature list (wireless AA end-to-end — discovery/video/audio/touch/reconnect; plugin architecture; multi-dashboards + widgets incl. HTML/JS web widgets; web config panel; External API v1; BT audio A2DP/AVRCP; phone/HFP; theming day/night); hardware needed (Pi 4, display, the BT/WiFi expectations); Quickstart — prebuilt path (`bash install.sh`, mention `--list-prebuilt`) then source path (clone --recurse-submodules, cmake, ctest command without counts); short repo layout block; documentation pointers (docs/INDEX.md, docs/architecture.md, docs/development.md, CONTRIBUTING.md); license. **Removed** (verify absent): "Current State (February 26)" section, all test counts, version-inconsistency notes, architecture deep-dive (now docs/architecture.md), release-packaging internals (now docs/reference/release-packaging.md — keep a one-line pointer).
- [ ] **Step 2: Verify** — `grep -in "february\|48/48\|0\.3\.0" README.md` → empty; image paths exist.
- [ ] **Step 3: Commit** — `git add README.md && git commit -m "docs: rewrite README as contributor-facing intro"`

### Task 13: CONTRIBUTING.md + .github templates

**Tier:** main
**Files:**
- Create: `CONTRIBUTING.md`, `.github/pull_request_template.md`, `.github/ISSUE_TEMPLATE/bug_report.md`, `.github/ISSUE_TEMPLATE/feature_request.md`

- [ ] **Step 1: `CONTRIBUTING.md`** (~40 lines) — welcome line; how to file issues (use the templates); dev setup pointer (docs/development.md); commands live in AGENTS.md — read it before contributing (agents AND humans); branch flow: work lands on `dev`, PRs to `main`; PRs must pass local build + `ctest --output-on-failure`; style: match the surrounding code, no new production dependencies without justification; proto submodule is hands-off (changes go to open-android-auto); where to talk: GitHub issues.
- [ ] **Step 2: `.github/pull_request_template.md`** — sections: `## Summary` (what changed), `## Why`, `## Testing` (checkboxes: local build, ctest, cross-build, on-Pi verification, not applicable; commands block with `cmake --build . && ctest --output-on-failure`), `## Deployment notes` (config/systemd/install.sh changes?), `## Risk` (what could break).
- [ ] **Step 3: Issue templates** — `bug_report.md`: front-matter (`name: Bug report`, `about: Something broken`), sections: What happened / Expected / Pi model + OS / Phone model + Android version / Connection type (wireless AA, BT audio, HFP) / Logs (`journalctl -u openauto-prodigy.service -n 200`). `feature_request.md`: front-matter, sections: Problem / Proposed solution / Alternatives considered.
- [ ] **Step 4: Commit** — `git add CONTRIBUTING.md .github && git commit -m "docs: add CONTRIBUTING and GitHub PR/issue templates"`

---

## Phase 4 — Verification & gate

### Task 14: link-check script + full verification

**Tier:** sonnet
**Files:**
- Create: `scripts/check-doc-links.py`

- [ ] **Step 1: Create `scripts/check-doc-links.py`**:

```python
#!/usr/bin/env python3
"""Check that relative markdown links in live docs resolve. Archive dirs are exempt."""
import re, sys, pathlib

REPO = pathlib.Path(__file__).resolve().parent.parent
SKIP_DIRS = {"docs/archive", "build", "build-pi", "libs/prodigy-oaa-protocol/proto",
             "reviews", ".git", ".superpowers", "node_modules"}
LINK = re.compile(r"\[[^\]]*\]\(([^)#\s]+)(?:#[^)]*)?\)")

def live_md_files():
    for p in REPO.rglob("*.md"):
        rel = p.relative_to(REPO).as_posix()
        if not any(rel == d or rel.startswith(d + "/") for d in SKIP_DIRS):
            yield p

bad = 0
for md in live_md_files():
    for target in LINK.findall(md.read_text(errors="replace")):
        if target.startswith(("http://", "https://", "mailto:")):
            continue
        resolved = (md.parent / target).resolve()
        if not resolved.exists():
            print(f"BROKEN: {md.relative_to(REPO)} -> {target}")
            bad += 1
print(f"{'FAIL' if bad else 'OK'}: {bad} broken links")
sys.exit(1 if bad else 0)
```

- [ ] **Step 2: Run it and fix every broken link it reports** (fix the referencing live doc; archive content is exempt by design). Re-run until `OK: 0 broken links`.
- [ ] **Step 3: Full stale-path sweep** — re-run every grep from Task 4 Step 2 (now WITHOUT the `:!CLAUDE.md` exclusion) → all empty.
- [ ] **Step 4: Build + tests**

```bash
cd build && cmake --build . --target openauto-prodigy -j$(nproc) && ctest --output-on-failure
```
Expected: app target builds; full suite passes (compare pass count to the pre-cleanup run — must be identical).

- [ ] **Step 5: Commit** — `git add scripts/check-doc-links.py && git commit -m "docs: add markdown link checker; verification sweep green"` (plus any link fixes).

### Task 15: Codex review gate + handoff + (with go-ahead) push

**Tier:** main

- [ ] **Step 1:** `bash scripts/codex-review.sh` over the full range (explicit base = the commit before this cleanup's first commit). Handle exit 2/4 per AGENTS.md (degrade to Fable-only review, note it).
- [ ] **Step 2:** Adjudicate every finding — confirmed → fix; dismissed → stated reason. No silent drops. Substantial fixes → one gate re-run.
- [ ] **Step 3:** Append session-handoffs entry (what/why/status/verification incl. link-checker OK, build+ctest results, adjudication counts).
- [ ] **Step 4:** Flip this plan and its design doc to `Status: COMPLETED 2026-07-XX` and `git mv` both to `docs/archive/plans/` in the same commit (per the convention they establish). Update the two INDEX/plans-README "live plans" mentions accordingly.
- [ ] **Step 5:** Ask Matthew for push go-ahead. Push only on explicit yes.

---

## Self-review notes (writing-plans checklist)

- **Spec coverage:** every spec section maps to a task — tree/moves (1), statuses + blanket rule (2), rotation (3), reference sweep incl. aa_proto_graph + relative links (4), needs-review triage (5), INDEX + plans README + handbook merge with freshness fixes (6), architecture.md (7), AGENTS SSOT (8), nested files + gotcha freshness (9), CLAUDE stub + reference sweep (10), sub-READMEs + wishlist follow-ups (11), README (12), CONTRIBUTING/.github (13), link-check + verification (14), gate (15). Spec's "no test counts" rule is a Global Constraint.
- **Known sequencing quirk:** INDEX (Task 6) references architecture.md before Task 7 creates it — the Task 14 link check is the enforcement point; all within one pre-push batch.
- **Type consistency:** nested AGENTS.md paths identical in Tasks 8, 9, 10; status vocabulary identical in Tasks 2, 15 and the spec.
