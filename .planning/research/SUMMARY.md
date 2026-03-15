# Project Research Summary

**Project:** OpenAuto Prodigy v0.6.1 — Widget Framework & Layout Refinement
**Domain:** Automotive head unit UI — widget manifest formalization, navigation consolidation, DPI-aware grid layout
**Researched:** 2026-03-14
**Confidence:** HIGH

## Executive Summary

v0.6.1 is a refinement milestone, not a greenfield build. The widget system, plugin architecture, and grid infrastructure all shipped in v0.5.2–v0.6. This milestone formalizes what already exists: a documented widget manifest contract, removal of the now-redundant `LauncherDock` in favor of a widget-based launcher, and DPI-aware grid math that produces physically consistent touch targets across display sizes. Zero new dependencies are required — every capability needed is already in the locked Qt 6.8 / C++17 / PipeWire / BlueZ stack.

The recommended approach is methodical: extend `WidgetDescriptor` first (safe struct additions with defaults), fix the grid math next, create the launcher widget replacement third, then remove the dock. This ordering ensures no user is ever left without navigation. The hardest single decision — whether to convert the launcher to a full-fledged widget or leave a simpler app shortcut — is settled in favor of full widget conversion. It eliminates a parallel data model (`LauncherModel` vs `PluginModel`), recovers 90px of vertical screen space on every display, and makes the home screen a conceptually uniform surface where everything is a widget.

The primary risks are not technical — the codebase is solid. They are UX and migration risks: removing the dock before replacement navigation is in place bricks the UI, and changing grid dimension constants silently invalidates saved widget layouts. Both are avoidable with clear sequencing and a config migration pass. The research is highly confident because all findings come from direct inspection of the running codebase on `dev/0.6`.

---

## Key Findings

### Recommended Stack

No new dependencies. The full stack is locked and sufficient for all v0.6.1 work. See `STACK.md` for the full version table.

**Core technologies (all locked, no changes):**
- **Qt 6.8.2 / QML:** UI framework, widget rendering, `QAbstractListModel` grid model — all APIs needed are stable Qt 5+ patterns
- **C++17:** `WidgetDescriptor` struct extensions, `DisplayInfo` DPI math, `WidgetGridModel` lifecycle — no new language features needed
- **CMake 3.22+:** Build system unchanged; only source file additions and removals
- **yaml-cpp:** YAML config persistence for grid layout — already used for `GridPlacement` storage

All additions are compatible with both Qt 6.4 (dev VM) and Qt 6.8 (Pi). No new `find_package()`, no new apt packages, no new `target_link_libraries()`.

### Expected Features

See `FEATURES.md` for the full feature table with complexity and dependency analysis.

**Must have (table stakes):**
- Formal widget manifest spec — documented `WidgetDescriptor` contract; without this, third-party widget development is guesswork
- Widget size constraint enforcement (resize + placement) — Android, HA, and every mature widget framework hard-stops at declared min/max; currently enforcement in `WidgetGridModel` is unverified against descriptor bounds
- Dock removal with equivalent navigation access — removal is safe only after the launcher widget replacement is in place and verified on Pi
- DPI-consistent grid density — cell sizes should be physically uniform across the target display range (800x480 through 1920x1080)
- Plugin-to-widget registration path audit — verify `IPlugin::widgetDescriptors()` is the single registration path consumed by `PluginManager`

**Should have (differentiators):**
- `category` field on `WidgetDescriptor` — enables picker grouping as the widget catalog grows
- `description` field — picker subtitle; Android 12+ has this for good reason on small screens
- `userRemovable` capability flag — lets system-critical widgets be pinned
- Page visibility optimization — bind `Timer.running` to page visibility; saves CPU when widgets are off-screen
- Widget lifecycle signals in `WidgetHost.qml` — `widgetActivated`, `widgetDeactivated`, `widgetResized`; opt-in, no breakage to existing widgets

**Defer to v0.7+:**
- Per-widget configuration dialog and storage
- Widget store or marketplace (violates offline constraint; do not build it)
- Animated resize transitions (janky on Pi GPU; ghost rectangle preview is the right call)
- AA LiveSurface widget (complex video pipeline work; already declared deferred in `WidgetTypes.hpp`)
- Drag-to-reorder pages (long-press ambiguity with widget drag; not worth it at 2-3 pages)

### Architecture Approach

The architecture requires surgical changes rather than structural surgery. `WidgetDescriptor` gets three new fields (`category`, `description`, `userRemovable`). `DisplayInfo::updateGridDimensions()` replaces fixed pixel targets with physical mm targets converted via `computedDpi()`. `LauncherDock.qml` and `LauncherModel` are deleted; a new `LauncherWidget.qml` reads `PluginModel` directly (single source of truth). `WidgetHost.qml` gains three opt-in lifecycle signals. See `ARCHITECTURE.md` for the full component boundary table and data flow diagrams.

**Major components and changes:**

1. **`DisplayInfo`** — replace `targetCellW=150, targetCellH=125` (fixed pixels) with physical mm targets (`~25mm wide, ~22mm tall`) converted to pixels via `computedDpi()`; remove `dockPx=56` deduction
2. **`WidgetDescriptor` / `WidgetGridModel`** — add `category`, `description`, `userRemovable` fields; add `CategoryRole`, `DescriptionRole`, `UserRemovableRole` to model; add `Q_INVOKABLE clampedSize()` as single source of truth for resize constraint enforcement
3. **`LauncherWidget.qml` (NEW)** — `org.openauto.launcher` widget, `minCols=2/maxCols=6`, reads `PluginModel` for available plugins, adaptive icon row layout within cell bounds
4. **`LauncherDock.qml` + `LauncherModel` (DELETED)** — backed by `YamlConfig::launcherTiles()`, parallel to `PluginModel`; both eliminated after launcher widget is in place
5. **`WidgetHost.qml`** — add `widgetActivated`, `widgetDeactivated`, `widgetResized` signals; page visibility tracking
6. **`HomeMenu.qml`** — remove `LauncherDock` from ColumnLayout; emit lifecycle signals on page visibility changes

**Key patterns established:**
- Launcher widget reads `PluginModel` (not a separate config model) — eliminates dual source of truth
- `WidgetHost` is the composition root — all widgets wrapped by it, never placed bare in the Repeater
- Cell size in physical mm, not pixels — DPI-independent touch targets
- `Q_INVOKABLE clampedSize()` — QML drag preview and C++ model use identical constraint logic, no divergence

### Critical Pitfalls

Full detail in `PITFALLS.md`. Top 5 that require explicit mitigation in the roadmap:

1. **Dock removal before replacement navigation exists** — Every `LauncherDock` action (AA, BT Audio, Phone, Settings) must have a non-dock entry point before the dock is deleted. QA gate: reach every view without SSH. `LauncherWidget` must ship in a separate commit before the dock removal commit.

2. **Grid dimension changes silently invalidate saved layouts** — Changing `targetCellW`/`targetCellH` constants moves grid boundaries; placements stored as absolute cell indices become out-of-bounds, and the UX is "my widgets vanished after update." Mitigation: add `grid_dimensions` to the YAML layout header; run a migration pass on load that clamps placements to the new bounds.

3. **C++ and QML resize constraint logic diverge** — `WidgetGridModel::resizeWidget()` enforces constraints in C++; the drag ghost overlay in `HomeMenu.qml` duplicates this logic in QML. Fix: add `Q_INVOKABLE clampedSize()` to the model; QML calls it instead of reimplementing constraints independently.

4. **Sub-pixel cell dimensions cause visible gaps** — `pageView.width / gridColumns` produces fractional pixels on 1024x600. Use `Math.floor()` for cell dimensions with the remainder distributed as outer margin padding. Test all 4 navbar edge positions.

5. **Interactive widget MouseAreas silently suppress long-press** — Widgets with internal `MouseArea` controls prevent `WidgetHost`'s `z:-1` long-press detector from firing. Framework-level fix (`childMouseEventFilter` in WidgetHost) is safer than convention-only documentation. This affects all third-party widget authors, not just the built-ins.

---

## Implications for Roadmap

The architecture research produced an explicit 6-step build order where each step leaves the app functional and is independently testable. The roadmap phases should follow this dependency chain.

### Phase 1: WidgetDescriptor Extensions
**Rationale:** Pure struct additions with defaults; zero risk; every downstream step needs the enriched descriptor
**Delivers:** `category`, `description`, `userRemovable` fields on `WidgetDescriptor`; new `QAbstractListModel` roles; all built-in widget registrations updated with category values
**Addresses:** Widget manifest spec (table stakes), category/description/flags (differentiators)
**Avoids:** N/A — no behavioral change, purely additive

### Phase 2: DPI-Aware Grid Dimensions + Layout Migration
**Rationale:** Must happen before dock removal so the grid gains the row that dock removal frees; must handle saved layout migration before any constants change
**Delivers:** `DisplayInfo` using physical mm targets via `computedDpi()`; `grid_dimensions` field in YAML layout; migration pass that clamps existing placements to new bounds; `dockPx` deduction removed from C++ (dock not yet removed from QML)
**Addresses:** DPI-consistent grid density (table stakes), sub-pixel artifact fix
**Avoids:** Pitfall 2 (layout invalidation on constant change), Pitfall 4 (sub-pixel cell gaps)

### Phase 3: LauncherWidget Creation
**Rationale:** Must exist and be Pi-verified before the dock can be removed; no deletions in this phase
**Delivers:** `qml/widgets/LauncherWidget.qml`, `org.openauto.launcher` descriptor registration in `main.cpp`, default YAML placement on page 0
**Addresses:** Dock removal with equivalent access (table stakes)
**Avoids:** Pitfall 1 (navigation paths lost before replacement exists)

### Phase 4: LauncherDock Removal + Config Migration
**Rationale:** Replacement navigation (Phase 3) must be proven on Pi before this runs; QA gate must pass before merge
**Delivers:** `LauncherDock.qml` deleted, `LauncherModel` deleted, `HomeMenu.qml` cleaned to SwipeView + PageIndicator only, YAML `launcher.tiles` section deprecated or auto-migrated, 90px vertical space reclaimed for widget grid
**Addresses:** Dock removal (table stakes), dual-model elimination
**Avoids:** Pitfall 1 (navigation loss), Pitfall 8 (dead YAML config confusing users)
**Gate:** "Reach every view without SSH" test passes on Pi before merge

### Phase 5: Widget Framework Polish
**Rationale:** Independent of dock work; can be parallelized with Phase 3 if bandwidth allows; completes the framework for third-party authors
**Delivers:** `Q_INVOKABLE clampedSize()` on `WidgetGridModel`; `widgetActivated`, `widgetDeactivated`, `widgetResized` signals on `WidgetHost.qml`; lifecycle signals emitted from `HomeMenu.qml`; pixel breakpoints in existing 4 widgets migrated to span-based or `UiMetrics`-relative breakpoints
**Addresses:** Resize constraint enforcement (table stakes), lifecycle signals (differentiator), DPI-scalable breakpoints (differentiator)
**Avoids:** Pitfall 3 (constraint divergence), Pitfall 5 (long-press suppression), Pitfall 6 (pixel breakpoints on high-DPI), Pitfall 7 (off-screen widget CPU waste)

### Phase 6: Documentation
**Rationale:** Documents the final state after all changes; enables third-party widget development
**Delivers:** `docs/plugin-widget-guide.md` covering the full `WidgetDescriptor` field contract, registration path, QML lifecycle hooks, size conventions, category taxonomy, CMake setup (`QT_QML_SKIP_CACHEGEN TRUE` requirement for `Loader`-loaded widgets)
**Addresses:** Plugin-widget contract documentation (differentiator)
**Avoids:** Pitfall 10 (SKIP_CACHEGEN omission for new widgets), Pitfall 13 (stringly-typed context property guards)

### Phase Ordering Rationale

- Phase 1 before all others: struct additions are zero-risk and every other change depends on the enriched descriptor
- Phase 2 before Phase 4: the grid dimension change must include a migration pass; reclaiming dock space from the C++ side happens here so when the dock QML is removed in Phase 4 the grid naturally fills the space
- Phase 3 before Phase 4: replacement navigation must be shippable and Pi-verified before removing its predecessor — these are separate commits with a manual QA gate between them
- Phase 5 can overlap Phase 3: no shared files; widget framework polish is independent of launcher conversion
- Phase 6 last: documents the finalized state, not a moving target

### Research Flags

Phases with established patterns (skip `/gsd:research-phase` during planning):
- **Phase 1:** Struct field additions — trivial C++17; no research needed
- **Phase 3:** QML widget creation — follows the established `ClockWidget` / `AAStatusWidget` pattern exactly; just a new QML file + descriptor registration
- **Phase 6:** Documentation writing — no technical research needed; documents what was built

Phases that may benefit from a brief focused design pass during planning:
- **Phase 2:** The specific mm target values (`~22-25mm wide`) need empirical validation on the Pi at multiple simulated display sizes. The algorithm is correct; the constants need a 30-minute tuning session before locking in.
- **Phase 4:** Config migration strategy (auto-inject launcher widget vs schema version bump) needs a focused decision. Recommendation is auto-inject (simpler for solo maintainer), but the YAML migration path deserves an explicit design before coding.
- **Phase 5:** The `childMouseEventFilter` approach for long-press (Pitfall 5) is the right fix but needs verification that it does not interfere with the drag/resize overlay hit testing in `HomeMenu.qml`. May be sufficient to start with a well-documented `pressAndHold` forwarding convention plus a `WidgetBase.qml` reference template, and defer the framework-level filter if no third-party widgets exist yet.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All from direct codebase inspection; no external dependencies in scope; Qt 6.4/6.8 dual-version compat verified |
| Features | HIGH | Android AppWidget, HA dashboard, and Qt documentation corroborate; existing 4 widgets validate the patterns |
| Architecture | HIGH | Every component analyzed from running `dev/0.6` source; build order derived from actual file dependency graph |
| Pitfalls | HIGH | Most from prior project experience (CLAUDE.md / session memory gotchas) plus direct inspection of vulnerable code paths |

**Overall confidence:** HIGH

### Gaps to Address

- **DPI mm constants need empirical validation:** The proposed `targetMm_W=25, targetMm_H=22` values are analytically derived. They need a quick test on the Pi with `--geometry` flags simulating different display sizes before being locked in. Flag for early in Phase 2 execution.

- **`childMouseEventFilter` vs drag overlay interaction:** Pitfall 5 recommends framework-level long-press interception in `WidgetHost`. Before implementing, verify this does not conflict with the drag/resize overlay event handling in `HomeMenu.qml`. If it does conflict, fall back to a well-documented `pressAndHold` forwarding convention with a `WidgetBase.qml` template.

- **YAML layout migration strategy:** Two viable options (auto-inject launcher widget vs schema version bump). The decision is deferred to Phase 4 planning. Strong lean toward auto-inject given solo maintainer context — but the exact migration logic needs to be designed before writing the code.

- **Widget breakpoint migration scope:** All 4 existing widgets use hardcoded pixel breakpoints. Phase 5 migrates all 4 as reference implementations for the new convention. Scope is clear and bounded; no research gap, just execution.

---

## Sources

### Primary (HIGH confidence — direct codebase inspection)
- `src/core/widget/WidgetTypes.hpp` — `WidgetDescriptor`, `GridPlacement`, `DashboardContributionKind`
- `src/core/widget/WidgetRegistry.{hpp,cpp}` — descriptor store, `widgetsFittingSpace()` filtering
- `src/ui/WidgetGridModel.{hpp,cpp}` — grid model, occupancy, page management, resize/place
- `src/ui/DisplayInfo.{hpp,cpp}` — DPI computation, grid dimension derivation (current `targetCellW/H=150/125`)
- `src/ui/LauncherModel.{hpp,cpp}` — launcher tile model (to be removed)
- `qml/controls/UiMetrics.qml` — DPI scaling, layout tokens, reference DPI 160
- `qml/applications/home/HomeMenu.qml` — home screen layout, drag/resize overlay, SwipeView
- `qml/components/LauncherDock.qml` — current dock (tile actions: `plugin:id`, `navigate:settings`)
- `qml/components/WidgetHost.qml` — glass card, Loader, `z:-1` long-press MouseArea
- `qml/widgets/ClockWidget.qml`, `AAStatusWidget.qml`, `NowPlayingWidget.qml`, `NavigationWidget.qml`
- `src/core/plugin/IPlugin.hpp` — `widgetDescriptors()` interface
- `src/main.cpp` — widget registration wiring, model creation, context properties
- `.planning/PROJECT.md` — v0.6.1 milestone definition and constraints

### Secondary (HIGH confidence — authoritative external docs)
- Android `AppWidgetProviderInfo` API reference — manifest spec, size constraints, lifecycle patterns
- Android Responsive Widget Layouts docs — breakpoint-based sizing with `SizeF` mapping
- Qt 6 Scalability documentation — DPI scaling patterns for QML

### Secondary (MEDIUM confidence)
- Home Assistant Dashboard Grid System (2024) — comparable grid/section design for dashboards
- KDAB: Scalable UIs in QML — practical DPI scaling patterns for embedded/kiosk
- Android Automotive Car UI framework docs — car UI touch target conventions

---
*Research completed: 2026-03-14*
*Ready for roadmap: yes*
