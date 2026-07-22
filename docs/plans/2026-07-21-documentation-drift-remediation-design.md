# Current Documentation Drift Remediation — Design

Status: ACTIVE

**Date:** 2026-07-21  
**Grounded against:** `origin/main` at `1157de0`  
**Publication:** one docs-only series, independent of runtime follow-up work

## 1. Outcome

Make every current, user-facing repository guide agree with the implementation
that ships today. Correct complete guidance by subsystem rather than applying
isolated substitutions, while leaving historical records intact.

The series is documentation-only: it may update Markdown, contributor
instructions, comments whose only purpose is guidance, and obsolete snapshots
under `docs/pi-config/`; it must not change executable behavior.

## 2. Decisions Locked

- **Implementation is the authority for present-tense claims.** Each correction
  is checked directly against current C++, QML, configuration defaults,
  installer scripts, packaging scripts, tests, or Git state as appropriate.
- **Current guidance only.** Files under `docs/archive/` and old handoff entries
  remain immutable history. Completion adds one new handoff at the top.
- **Wishlist-then-promote remains intact.** Shipped work may be closed or
  reworded accurately; unapproved ideas remain in the wishlist and do not
  become roadmap or plan scope.
- **Comprehensive subsystem passes.** Each task reconciles the whole affected
  section with code, including nearby contradictions that would make the
  corrected sentence misleading.
- **Public wording stays operational and sanitized.** Tracked files describe
  the repository and its behavior without review provenance or internal
  evidence trails.
- **No build ceremony by default.** Link, search, and source-comparison checks
  are the gate. A build is required only if a tracked executable source changes
  unexpectedly; that would be a plan deviation and must stop the docs batch.

## 3. Scope

### Current status and roadmap

- `docs/roadmap-current.md`
- `docs/wishlist.md`
- `docs/INDEX.md`
- Current plan status annotations whose shipped/pending state is demonstrably
  stale

### Architecture and design guidance

- `docs/architecture.md`
- `docs/design-decisions.md`
- `docs/design-philosophy.md`

### Reference documentation

- `docs/reference/settings-tree.md`
- `docs/reference/config-schema.md`
- `docs/reference/plugin-api.md`
- `docs/reference/widget-developer-guide.md`
- `docs/reference/release-packaging.md`

### Contributor guidance

- `AGENTS.md`
- `src/core/aa/AGENTS.md`
- `qml/AGENTS.md`
- `src/AGENTS.md` only if current `origin/main` still contains a false rule

### How-to and setup guidance

- `docs/development.md`
- `docs/wireless-setup.md`
- `docs/how-to/testing-reconnect.md`
- `docs/how-to/debugging-notes.md`
- `docs/pi-config/README.md`
- `docs/pi-config/bluetooth-service-override.conf`
- Obsolete dnsmasq snapshots under `docs/pi-config/`

### Closure-only files

- `docs/session-handoffs.md` — append one newest entry only
- This design and its implementation plan — mark complete and archive together

## 4. Explicitly Out of Scope

- Any runtime, installer, service-unit, schema, QML, or build-system behavior
  change
- Rewriting archived plans, old handoffs, release history, tags, or commit
  messages to read as current
- Adding new product features, priorities, or commitments
- Repairing behavior merely because a documentation comparison reveals a code
  defect; such discoveries go through the wishlist/project loop separately
- Publishing internal review records or security-sensitive operational detail
- Folding this work into the runtime follow-up branch or PR

## 5. Source-of-Truth Map

| Claim family | Required implementation evidence |
|---|---|
| Delivery and shipped status | current plan headers, Git history/tags, registered plugins, tests |
| AA transport/video/touch | AA session/orchestrator code, service discovery, decoder paths, evdev bridge |
| Services and composition | `src/main.cpp`, service interfaces, plugin host context |
| Settings UI | current QML settings pages, menu tests, registered models/actions |
| Configuration | `YamlConfig` defaults/accessors, shipped YAML, installer writes, config coverage tests |
| Widgets/plugins | registries, manifests, public interfaces, QML loaders, packaging code |
| Pi setup | `install.sh`, service/BlueZ templates, networkd/hostapd snapshots |
| Release archive | packaging script inputs and package-verification tests |

## 6. Acceptance

1. Every changed present-tense claim has a cited implementation comparison in
   the execution notes or commit review.
2. Current guidance contains no known obsolete component names, configuration
   keys, ports, scripts, menu entries, dependency claims, or delivery-status
   statements covered by this scope.
3. Archived files and prior handoff entries have no diff.
4. `python3 scripts/check-doc-links.py` passes.
5. Targeted `rg` checks listed in the implementation plan pass, with any
   intentional historical/current occurrences adjudicated explicitly.
6. `git diff --check` passes.
7. The repository review gate returns no unadjudicated findings.
8. The final handoff records changes, rationale, status, next steps,
   verification, and review adjudication without exact suite counts.
