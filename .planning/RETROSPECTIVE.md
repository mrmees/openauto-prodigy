# Retrospective

## Milestone: v0.4 — Logging & Theming

**Shipped:** 2026-03-01
**Phases:** 2 | **Plans:** 9 | **Duration:** ~96 min execution

### What Was Built
- Categorized logging infrastructure (6 subsystems, quiet defaults, verbose via CLI/config/web panel)
- Full log call migration — zero raw qDebug/qInfo/qWarning calls remain
- Multi-theme color palette system with user override paths
- 3-tier brightness control (sysfs > ddcutil > software overlay)
- Wallpaper system with per-theme defaults, user override, settings picker
- Gesture overlay brightness control and wallpaper shell integration

### What Worked
- Gap-closure plans were fast and targeted (~1-3 min each)
- UAT-driven gap discovery caught real issues (config defaults gate, wallpaper refresh)
- Static library build optimization eliminated redundant test compilations
- Phase 2 averaged 7 min/plan vs Phase 1's 19 min — velocity improved with familiarity

### What Was Inefficient
- Original milestone scope (v1.0 with 4+ phases) was too ambitious — rescoped to v0.4 after completing only 2 phases
- Context gathering for Phase 3 (equalizer) was done without user input — had to be discarded and redone
- Phase 1 Plan 3 (UAT gap closure) took disproportionate time due to log severity triage complexity

### Patterns Established
- Config defaults gate: any new config key must be registered in `initDefaults()` before `setValueByPath()` will accept writes
- Q_INVOKABLE required for QML → C++ method calls via context properties
- Wallpaper override independent of theme (setTheme sets default, setWallpaper overrides)
- Software dimming as fallback when hardware backlight unavailable

### Key Lessons
- Scope milestones to what's actually planned in the roadmap, not aspirational feature lists
- Context gathering must involve the user — AI assumptions about feature design are not substitute for real requirements
- Gap-closure plans are inherently faster than initial implementation plans — budget accordingly

---
