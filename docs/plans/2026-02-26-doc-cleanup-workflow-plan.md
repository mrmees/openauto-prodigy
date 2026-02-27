# Documentation Cleanup & Structured Workflow — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Consolidate scattered documentation across 3 repos into a clean, navigable structure with a companion-app-style workflow loop in openauto-prodigy.

**Architecture:** Move all active project management into openauto-prodigy's `docs/` tree. Archive community repo content. Consolidate 57 completed plan docs into 5 milestone summaries. Rewrite CLAUDE.md and INDEX.md to match.

**Tech Stack:** Markdown, git, file operations only — no code changes.

---

### Task 1: Create AA Protocol Subdirectory

Move AA-specific docs from `docs/` root into `docs/aa-protocol/` for grouping.

**Files:**
- Create: `docs/aa-protocol/` (directory)
- Move: 10 files from `docs/` to `docs/aa-protocol/`

**Step 1: Create directory and move files**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
mkdir -p docs/aa-protocol

mv docs/aa-protocol-reference.md docs/aa-protocol/protocol-reference.md
mv docs/aa-phone-side-debug.md docs/aa-protocol/phone-side-debug.md
mv docs/android-auto-protocol-cross-reference.md docs/aa-protocol/cross-reference.md
mv docs/aa-apk-deep-dive.md docs/aa-protocol/apk-deep-dive.md
mv docs/aa-display-rendering.md docs/aa-protocol/display-rendering.md
mv docs/aa-video-resolution.md docs/aa-protocol/video-resolution.md
mv docs/aa-troubleshooting-runbook.md docs/aa-protocol/troubleshooting-runbook.md
mv docs/apk-proto-reference.md docs/aa-protocol/apk-proto-reference.md
mv docs/apk-indexing.md docs/aa-protocol/apk-indexing.md
mv docs/proto-validation-report.md docs/aa-protocol/proto-validation-report.md
```

**Step 2: Verify moves**

```bash
ls docs/aa-protocol/
```

Expected: 10 files listed.

**Step 3: Commit**

```bash
git add -A docs/aa-protocol/ docs/aa-protocol-reference.md docs/aa-phone-side-debug.md \
  docs/android-auto-protocol-cross-reference.md docs/aa-apk-deep-dive.md \
  docs/aa-display-rendering.md docs/aa-video-resolution.md docs/aa-troubleshooting-runbook.md \
  docs/apk-proto-reference.md docs/apk-indexing.md docs/proto-validation-report.md
git commit -m "docs: move AA protocol docs into docs/aa-protocol/ subdirectory"
```

---

### Task 2: Import Community Repo as Archive

Copy all docs from `openauto-pro-community/` into `docs/OpenAutoPro_archive_information/`.

**Files:**
- Create: `docs/OpenAutoPro_archive_information/` (directory tree)
- Source: `/home/matt/claude/personal/openautopro/openauto-pro-community/`

**Step 1: Create directory structure and copy files**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
mkdir -p docs/OpenAutoPro_archive_information/firmware
mkdir -p docs/OpenAutoPro_archive_information/plans

# Root docs
cp ../openauto-pro-community/README.md docs/OpenAutoPro_archive_information/README.md
cp ../openauto-pro-community/CLAUDE.md docs/OpenAutoPro_archive_information/CLAUDE-original.md

# Research docs
cp ../openauto-pro-community/docs/reverse-engineering-notes.md docs/OpenAutoPro_archive_information/
cp ../openauto-pro-community/docs/abi-compatibility-analysis.md docs/OpenAutoPro_archive_information/
cp ../openauto-pro-community/docs/android-auto-fix-research.md docs/OpenAutoPro_archive_information/
cp ../openauto-pro-community/docs/bluetooth-wireless-aa-setup.md docs/OpenAutoPro_archive_information/
cp ../openauto-pro-community/docs/android-auto-apk-decompilation.md docs/OpenAutoPro_archive_information/
cp ../openauto-pro-community/docs/android-auto-protocol-cross-reference.md docs/OpenAutoPro_archive_information/

# Firmware analyses
cp ../openauto-pro-community/docs/alpine-ilx-w650bt-firmware-analysis.md docs/OpenAutoPro_archive_information/firmware/alpine-ilx-w650bt.md
cp ../openauto-pro-community/docs/alpine-halo9-firmware-analysis.md docs/OpenAutoPro_archive_information/firmware/alpine-halo9.md
cp ../openauto-pro-community/docs/kenwood-dnx-firmware-analysis.md docs/OpenAutoPro_archive_information/firmware/kenwood-dnx.md
cp ../openauto-pro-community/docs/sony-xav-firmware-analysis.md docs/OpenAutoPro_archive_information/firmware/sony-xav.md
cp ../openauto-pro-community/docs/pioneer-dmh-firmware-analysis.md docs/OpenAutoPro_archive_information/firmware/pioneer-dmh.md

# Historical plans
cp ../openauto-pro-community/docs/plans/*.md docs/OpenAutoPro_archive_information/plans/
```

**Step 2: Add archival header to README**

Prepend to `docs/OpenAutoPro_archive_information/README.md`:

```markdown
> **Archive Notice:** This content was imported from the
> [openauto-pro-community](https://github.com/mrmees/openauto-pro-community)
> repository. It documents reverse engineering of the original OpenAuto Pro
> software and is preserved here as reference material. Active development
> happens in the parent openauto-prodigy repository.

---

```

**Step 3: Verify import**

```bash
find docs/OpenAutoPro_archive_information/ -name "*.md" | wc -l
```

Expected: ~18 files.

**Step 4: Commit**

```bash
git add docs/OpenAutoPro_archive_information/
git commit -m "docs: import openauto-pro-community repo as archived reference"
```

---

### Task 3: Consolidate Completed Plan Docs — Milestone 1 (Foundation)

Read all Feb 16-18 plan docs, distill into a single milestone summary.

**Files:**
- Create: `docs/plans/milestone-01-foundation.md`
- Read (then delete): 6 source files

**Source plans to read and summarize:**
- `2026-02-16-protocol-audit-fixes.md`
- `2026-02-17-aa-performance-design.md`
- `2026-02-17-aa-performance-plan.md`
- `2026-02-18-architecture-and-ux-design.md`
- `2026-02-18-architecture-implementation-plan.md`
- `plans/autorun/protocol-exploration-01.md` through `06.md`

**Step 1: Read all source plans**

Read each file listed above. Extract: what was built, key design decisions, lessons learned.

**Step 2: Write milestone summary**

Create `docs/plans/milestone-01-foundation.md` with this structure:

```markdown
# Milestone 1: Foundation (Feb 16-18, 2026)

## What Was Built
- [Bullet list of features/changes delivered]

## Key Design Decisions
- [Decisions and their rationale]

## Lessons Learned
- [Hard-won insights worth preserving]

## Source Documents
- [List of original plan filenames that were consolidated]
```

Target: 200-400 lines. Preserve substance, cut process artifacts.

**Step 3: Delete source plans**

```bash
rm docs/plans/2026-02-16-protocol-audit-fixes.md
rm docs/plans/2026-02-17-aa-performance-design.md
rm docs/plans/2026-02-17-aa-performance-plan.md
rm docs/plans/2026-02-18-architecture-and-ux-design.md
rm docs/plans/2026-02-18-architecture-implementation-plan.md
rm -rf docs/plans/autorun/
```

**Step 4: Commit**

```bash
git add docs/plans/milestone-01-foundation.md
git add -u docs/plans/
git commit -m "docs: consolidate Feb 16-18 plans into milestone-01-foundation summary"
```

---

### Task 4: Consolidate Completed Plan Docs — Milestone 2 (AA Integration)

**Files:**
- Create: `docs/plans/milestone-02-aa-integration.md`
- Read (then delete): source files

**Source plans:**
- `2026-02-19-firmware-features-implementation.md`
- `2026-02-19-infrastructure-improvements-design.md`
- `2026-02-19-infrastructure-improvements-implementation.md`
- `2026-02-21-aa-sidebar-design.md`
- `2026-02-21-aa-sidebar-implementation.md`
- `2026-02-21-audio-pipeline-design.md`
- `2026-02-21-audio-pipeline-plan.md`
- `2026-02-21-settings-redesign-design.md`
- `2026-02-21-settings-redesign-plan.md`

**Steps:** Same as Task 3 — read, summarize, delete sources, commit.

```bash
git commit -m "docs: consolidate Feb 19-21 plans into milestone-02-aa-integration summary"
```

---

### Task 5: Consolidate Completed Plan Docs — Milestone 3 (Companion & System)

**Files:**
- Create: `docs/plans/milestone-03-companion-system.md`
- Read (then delete): source files

**Source plans:**
- `2026-02-22-companion-app-design.md`
- `2026-02-22-companion-app-implementation.md`
- `2026-02-22-multi-vehicle-companion-design.md`
- `2026-02-22-multi-vehicle-companion-plan.md`
- `2026-02-22-settings-ui-scaling-design.md`
- `2026-02-22-settings-ui-scaling.md`
- `2026-02-22-touch-friendly-settings-redesign.md`
- `2026-02-22-touch-friendly-settings-implementation.md`

**Steps:** Same as Task 3 — read, summarize, delete sources, commit.

```bash
git commit -m "docs: consolidate Feb 22-23 plans into milestone-03-companion-system summary"
```

---

### Task 6: Consolidate Completed Plan Docs — Milestone 4 (Protocol Correctness)

**Files:**
- Create: `docs/plans/milestone-04-protocol-correctness.md`
- Read (then delete): source files

**Source plans:**
- `2026-02-23-oaa-integration-design.md`
- `2026-02-23-oaa-integration-implementation.md`
- `2026-02-23-open-androidauto-design.md`
- `2026-02-23-open-androidauto-implementation.md`
- `2026-02-23-open-androidauto-integration.md`
- `2026-02-24-move-handlers-to-library.md`
- `2026-02-24-open-androidauto-organization-design.md`
- `2026-02-24-open-androidauto-organization-implementation.md`
- `2026-02-24-protocol-completeness-design.md`
- `2026-02-24-protocol-p0-implementation.md`
- `2026-02-25-aa-proto-deep-dive-design.md`
- `2026-02-25-apk-preprocessing-design.md`
- `2026-02-25-apk-preprocessing-implementation.md`
- `2026-02-25-protocol-plumbing.md`
- `2026-02-25-proto-partial-match-investigation.md`
- `2026-02-25-proto-phase-a-migration-manifest.md`
- `2026-02-25-proto-phase-a-patch-checklist.md`
- `2026-02-25-proto-validation-implementation.md`
- `2026-02-25-proto-validation-pipeline-design.md`

**Steps:** Same as Task 3 — read, summarize, delete sources, commit.

```bash
git commit -m "docs: consolidate Feb 23-25 plans into milestone-04-protocol-correctness summary"
```

---

### Task 7: Consolidate Completed Plan Docs — Milestone 5 (AV Optimization)

**Files:**
- Create: `docs/plans/milestone-05-av-optimization.md`
- Read (then delete): source files

**Source plans:**
- `2026-02-25-av-pipeline-optimization-design.md`
- `2026-02-25-av-pipeline-optimization-plan.md`
- `2026-02-25-hwaccel-decoder-selection-design.md`
- `2026-02-26-video-pipeline-optimization-design.md`
- `2026-02-26-video-pipeline-optimization-plan.md`
- `2026-02-26-proxy-reapply-resilience-plan.md`
- `2026-02-26-proxy-routing-plan.md`
- `2026-02-26-qr-pairing-design.md`
- `2026-02-26-qr-pairing-implementation.md`
- `2026-02-26-system-service-design.md`
- `2026-02-26-system-service-implementation.md`

**Steps:** Same as Task 3 — read, summarize, delete sources, commit.

```bash
git commit -m "docs: consolidate Feb 25-26 plans into milestone-05-av-optimization summary"
```

---

### Task 8: Move Active Plans to plans/active/

**Files:**
- Create: `docs/plans/active/` (directory)
- Move: 2 files

**Step 1: Move active plans**

```bash
cd /home/matt/claude/personal/openautopro/openauto-prodigy
mkdir -p docs/plans/active
mv docs/plans/2026-02-21-config-contract-overhaul-design.md docs/plans/active/
mv docs/plans/2026-02-21-config-contract-overhaul-plan.md docs/plans/active/
mv docs/plans/2026-02-21-architecture-extensibility-plan.md docs/plans/active/
```

**Step 2: Verify only milestones + active + design doc remain**

```bash
ls docs/plans/
```

Expected: `milestone-01-foundation.md` through `milestone-05-av-optimization.md`, `active/`, `2026-02-26-doc-cleanup-workflow-design.md`, `2026-02-26-doc-cleanup-workflow-plan.md`

**Step 3: Commit**

```bash
git add -A docs/plans/
git commit -m "docs: move active plans to docs/plans/active/"
```

---

### Task 9: Triage Workspace Loose Files

Review each loose markdown file at the workspace root and decide disposition with the user.

**Files to triage:**
- `/home/matt/claude/personal/openautopro/aa-proxy-rs-connection-troubleshooting.md`
- `/home/matt/claude/personal/openautopro/codex_architecture_corrections.md`
- `/home/matt/claude/personal/openautopro/wireless-aa-initial-handshake-findings.md`
- `/home/matt/claude/personal/openautopro/miata-hardware-reference.md`

**Step 1: Read each file**

Read all 4 files to understand their content and relevance.

**Step 2: Present disposition options to user**

For each file, ask: move to prodigy archive, move to prodigy reference, delete, or leave in place.

**Step 3: Execute user decisions**

Move/delete as directed.

**Step 4: Commit any moves**

```bash
git commit -m "docs: triage workspace loose files"
```

---

### Task 10: Rewrite INDEX.md

Replace the current INDEX.md with one matching the new structure.

**Files:**
- Modify: `docs/INDEX.md`

**Step 1: Write new INDEX.md**

Structure:

```markdown
# Documentation Index

## Project Management
- project-vision.md — product intent, design principles, constraints
- roadmap-current.md — current priorities (Now/Next/Later)
- session-handoffs.md — session continuity log
- wishlist.md — idea parking lot

## Getting Started
- development.md — build, dependencies, cross-compile, dual-platform
- wireless-setup.md — WiFi AP + Bluetooth for wireless AA

## Reference
- config-schema.md — YAML config keys and defaults
- design-decisions.md — architectural choices and rationale
- design-philosophy.md — core design principles (detailed)
- plugin-api.md — IPlugin interface, lifecycle, IHostContext
- hfp-stack-spike.md — HFP audio routing research
- debugging-notes.md — common issues and fixes
- testing-reconnect.md — reconnect test procedures

## AA Protocol
- aa-protocol/protocol-reference.md — comprehensive protocol reference
- aa-protocol/phone-side-debug.md — phone-side behavior and debugging
- aa-protocol/cross-reference.md — protocol cross-reference (aasdk/wire formats)
- aa-protocol/apk-deep-dive.md — APK v16.1 analysis
- aa-protocol/display-rendering.md — video pipeline and rendering
- aa-protocol/video-resolution.md — resolution negotiation
- aa-protocol/troubleshooting-runbook.md — troubleshooting guide
- aa-protocol/apk-proto-reference.md — protobuf message reference
- aa-protocol/apk-indexing.md — APK indexing pipeline
- aa-protocol/proto-validation-report.md — field migration validation

## Milestone History
- plans/milestone-01-foundation.md — Feb 16-18: initial build, architecture, protocol
- plans/milestone-02-aa-integration.md — Feb 19-21: oaa library, settings, audio, sidebar
- plans/milestone-03-companion-system.md — Feb 22-23: companion app, multi-vehicle, UI
- plans/milestone-04-protocol-correctness.md — Feb 23-25: proto migration, APK analysis
- plans/milestone-05-av-optimization.md — Feb 25-26: video, HW accel, proxy, system service

## Active Plans
- plans/active/ — in-progress or not-yet-started design/implementation plans

## Pi Configuration
- pi-config/ — system config snapshot (hostapd, systemd, BlueZ, labwc, udev, boot)

## Archive
- OpenAutoPro_archive_information/ — original OpenAuto Pro reverse engineering (from community repo)
```

**Step 2: Commit**

```bash
git add docs/INDEX.md
git commit -m "docs: rewrite INDEX.md to match new documentation structure"
```

---

### Task 11: Rewrite CLAUDE.md

Trim the 20K CLAUDE.md to remove stale content and reference the new workflow docs.

**Files:**
- Modify: `CLAUDE.md` (repo root)

**Step 1: Read current CLAUDE.md**

Identify sections to keep, trim, or remove:
- **KEEP:** What This Is, Repository Layout, Build & Test, Cross-Compilation, Pi Deployment, Architecture, Key Files, Gotchas (pruned), Hardware
- **REMOVE:** Current Status section (replaced by roadmap-current.md), Design Philosophy section (now in project-vision.md and design-philosophy.md)
- **ADD:** Reference to workflow docs (AGENTS.md, project-vision, roadmap, session-handoffs)
- **TRIM:** Gotchas — remove entries for issues that are fully resolved and unlikely to recur. Keep entries that represent ongoing constraints or non-obvious behavior.

**Step 2: Rewrite CLAUDE.md**

Key changes:
- Add "Workflow" section near top pointing to AGENTS.md and docs/project-vision.md, roadmap-current.md, session-handoffs.md
- Remove "Design Philosophy" section (say "See docs/project-vision.md and docs/design-philosophy.md")
- Remove "Current Status" section (say "See docs/roadmap-current.md")
- Prune Gotchas: remove fixed issues, keep active constraints
- Update any file paths that changed (aa-protocol/ subdirectory)
- Update Pi IP if needed (192.168.1.152 per MEMORY.md, but CLAUDE.md says .149)

**Step 3: Verify no broken references**

```bash
grep -r "aa-protocol-reference.md\|aa-phone-side-debug.md\|aa-apk-deep-dive.md" docs/ CLAUDE.md
```

Fix any stale paths.

**Step 4: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: rewrite CLAUDE.md — trim stale content, reference workflow docs"
```

---

### Task 12: Update Workspace CLAUDE.md

Update the top-level workspace CLAUDE.md to reflect the consolidation.

**Files:**
- Modify: `/home/matt/claude/personal/openautopro/CLAUDE.md`

**Step 1: Read current workspace CLAUDE.md**

**Step 2: Update directory layout table**

- Remove `openauto-pro-community/` entry (or mark as archived)
- Note that community content now lives in `openauto-prodigy/docs/OpenAutoPro_archive_information/`
- Update any stale paths or descriptions

**Step 3: Commit** (if inside a git repo — workspace root may not be)

---

### Task 13: Update MEMORY.md

Update the session memory to reflect the new doc structure and workflow.

**Files:**
- Modify: `/home/matt/.claude/projects/-home-matt-claude-personal-openautopro/memory/MEMORY.md`

**Step 1: Add entry about new workflow structure**

Note the AGENTS.md loop, doc locations, and that community repo is archived into prodigy.

**Step 2: Remove any stale entries that conflict with new structure**

---

### Task 14: Final Verification

**Step 1: Verify no orphaned files in docs/**

```bash
ls docs/*.md | sort
```

Should only show: config-schema.md, debugging-notes.md, design-decisions.md, design-philosophy.md, development.md, hfp-stack-spike.md, INDEX.md, plugin-api.md, project-vision.md, roadmap-current.md, session-handoffs.md, testing-reconnect.md, wireless-setup.md, wishlist.md

**Step 2: Verify plan directory is clean**

```bash
ls docs/plans/
```

Should show: 5 milestone files, `active/`, design doc, this plan file.

**Step 3: Verify archive is complete**

```bash
find docs/OpenAutoPro_archive_information/ -name "*.md" | wc -l
```

Should be ~18 files.

**Step 4: Run build to make sure nothing is broken**

```bash
cd build && cmake --build . -j$(nproc)
```

(Docs-only change — build should be unaffected, but verify.)

**Step 5: Commit session handoff**

Append first entry to `docs/session-handoffs.md` documenting this cleanup work.
