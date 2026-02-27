# Documentation Cleanup & Structured Workflow — Design

**Date:** 2026-02-26
**Status:** Approved

## Problem

10 days of intense development produced ~90 documentation files across 3 repos with no consistent structure. Docs are stale, hard to navigate, and there's no workflow loop for maintaining continuity across sessions. The companion-app repo has a proven workflow (AGENTS.md, project-vision, roadmap, session-handoffs) that should be replicated.

## Scope

Full sweep across all three documentation surfaces:
- `openauto-prodigy/` — primary target (90 docs, needs consolidation + workflow)
- `openauto-pro-community/` — archive into prodigy (research complete, no longer active)
- `openautopro/` workspace root — triage loose files case-by-case

## Approach: Companion-App Mirror

Replicate the companion-app's proven workflow structure in openauto-prodigy, plus consolidate scattered docs.

## New Document Structure

```
openauto-prodigy/
├── AGENTS.md                          # NEW — workflow loop (CREATED)
├── CLAUDE.md                          # REWRITE — trim, update paths, reference workflow docs
│
├── docs/
│   ├── project-vision.md              # NEW — product intent, principles, constraints (CREATED)
│   ├── roadmap-current.md             # NEW — Now/Next/Later (CREATED)
│   ├── session-handoffs.md            # NEW — session log (CREATED)
│   ├── wishlist.md                    # NEW — idea parking lot (CREATED)
│   ├── INDEX.md                       # REWRITE — match new layout
│   │
│   ├── development.md                 # KEEP
│   ├── config-schema.md               # KEEP
│   ├── plugin-api.md                  # KEEP
│   ├── wireless-setup.md              # KEEP
│   ├── debugging-notes.md             # KEEP
│   ├── design-philosophy.md           # KEEP (detailed elaboration of vision principles)
│   ├── design-decisions.md            # KEEP
│   ├── hfp-stack-spike.md             # KEEP
│   ├── testing-reconnect.md           # KEEP
│   │
│   ├── aa-protocol/                   # REORGANIZE — group AA protocol docs
│   │   ├── protocol-reference.md
│   │   ├── phone-side-debug.md
│   │   ├── cross-reference.md
│   │   ├── apk-deep-dive.md
│   │   ├── display-rendering.md
│   │   ├── video-resolution.md
│   │   ├── troubleshooting-runbook.md
│   │   ├── apk-proto-reference.md
│   │   ├── apk-indexing.md
│   │   └── proto-validation-report.md
│   │
│   ├── plans/
│   │   ├── milestone-01-foundation.md          # CONSOLIDATE — Feb 16-18
│   │   ├── milestone-02-aa-integration.md      # CONSOLIDATE — Feb 19-21
│   │   ├── milestone-03-companion-system.md    # CONSOLIDATE — Feb 22-23
│   │   ├── milestone-04-protocol-correctness.md # CONSOLIDATE — Feb 24-25
│   │   ├── milestone-05-av-optimization.md     # CONSOLIDATE — Feb 25-26
│   │   └── active/
│   │       ├── config-contract-overhaul-design.md
│   │       └── architecture-extensibility-plan.md
│   │
│   ├── pi-config/                     # KEEP
│   │
│   ├── OpenAutoPro_archive_information/   # NEW — community repo import
│   │   ├── README.md
│   │   ├── reverse-engineering-notes.md
│   │   ├── abi-compatibility-analysis.md
│   │   ├── android-auto-fix-research.md
│   │   ├── bluetooth-wireless-aa-setup.md
│   │   ├── firmware/
│   │   │   ├── alpine-ilx-w650bt.md
│   │   │   ├── alpine-halo9.md
│   │   │   ├── kenwood-dnx.md
│   │   │   ├── sony-xav.md
│   │   │   └── pioneer-dmh.md
│   │   └── plans/                     # Original community plans (historical)
│   │
│   └── testing/                       # KEEP — test captures and probes
```

## AGENTS.md Workflow Loop

Modeled on companion-app, adapted for C++/CMake:

1. Check alignment with `docs/project-vision.md` before implementation.
2. Update `docs/roadmap-current.md` when priorities or sequencing change.
3. Before claiming completion: local build + test suite + cross-compile if Pi-affecting.
4. Append handoff entry to `docs/session-handoffs.md`.

## Plan Consolidation Strategy

61 individual plan/design docs consolidated into 5 milestone summaries:

| Summary | Period | Themes |
|---------|--------|--------|
| milestone-01-foundation | Feb 16-18 | Initial build, architecture, protocol audit, firmware features |
| milestone-02-aa-integration | Feb 19-21 | oaa library, settings, audio pipeline, sidebar |
| milestone-03-companion-system | Feb 22-23 | Companion app, multi-vehicle, settings scaling, touch UI |
| milestone-04-protocol-correctness | Feb 24-25 | Proto field migration, APK analysis, protocol completeness |
| milestone-05-av-optimization | Feb 25-26 | Video pipeline, HW accel, proxy routing, system service |

Each summary captures: what was built, key design decisions, and lessons learned. Individual plan files are deleted after consolidation.

Active/incomplete plans move to `docs/plans/active/`.

## Community Repo Consolidation

All docs from `openauto-pro-community/` move to `docs/OpenAutoPro_archive_information/`. Firmware analyses grouped under a `firmware/` subdirectory. Community README gets an archival header. The community GitHub repo can be archived after import.

## CLAUDE.md Rewrite

The current 20K CLAUDE.md needs trimming:
- Remove "Current Status" section (replaced by roadmap-current.md)
- Remove stale gotchas for issues that are fixed
- Update file paths to match new doc structure
- Reference workflow docs instead of duplicating information
- Keep: architecture overview, build commands, key files table, active gotchas, hardware specs

## Workspace Loose Files

Triage case-by-case during implementation:
- `aa-proxy-rs-connection-troubleshooting.md`
- `codex_architecture_corrections.md`
- `wireless-aa-initial-handshake-findings.md`
- `miata-hardware-reference.md`
