# Project Research Summary

**Project:** OpenAuto Prodigy v0.6 — Plugin-Widget Architecture Formalization
**Domain:** Embedded automotive Qt/QML head unit — plugin and widget contract hardening
**Researched:** 2026-03-13
**Confidence:** HIGH

## Executive Summary

OpenAuto Prodigy v0.6 is not a greenfield build — it is a contract formalization pass on a working system. The stack (Qt 6.4/6.8, C++17, CMake, yaml-cpp, PipeWire, BlueZ) is locked and validated. No new libraries are required. The milestone's scope is precise: plug six specific architectural gaps in the plugin-widget contract that currently make the system work for four built-in plugins but would be guesswork for a third-party developer. Every research file converges on the same message: do the structural work, resist speculative additions.

The recommended approach is a strict 8-step build order where each step keeps the codebase compiling and produces no observable behavior change until the final integration step. The two independent axes — plugin lifecycle (initialize/activate/deactivate/shutdown) and widget instance lifecycle (create/activate/resize/destroy) — must be kept formally separated throughout. The existing `WidgetDescriptor`, `IPlugin`, `IHostContext`, `WidgetGridModel`, and `WidgetInstanceContext` structures each need targeted additions, not rewrites. The formalized architecture enables the v0.7 widget-based launcher, which depends on everything v0.6 defines.

The primary risk in this milestone is scope creep, not technical difficulty. Three of the seven critical pitfalls are variants of the same failure mode: over-engineering something that only needs to be correct for four plugins and one developer right now. The secondary risk is the AndroidAutoPlugin constructor: it carries legacy pre-IHostContext parameters that must not be touched during this milestone. Keeping those two constraints in mind — minimal manifest, leave AA constructor alone — removes most of the ways this milestone can go wrong.

## Key Findings

### Recommended Stack

The stack is fully locked. All v0.6 capabilities are achievable with existing dependencies. The only change is structural: `IHostContext` gets a `widgetRegistry()` accessor, `WidgetDescriptor` gets seven new fields, and one new header-only interface (`IWidgetLifecycle`) is added. No new `find_package()`, no new apt packages, no new link targets.

**Core technologies (all locked, no changes):**
- **Qt 6.4/6.8 + QML**: Framework and UI engine — dual-version constraint is already managed; all v0.6 additions use APIs available in both versions
- **C++17 + CMake**: Implementation and build — `std::function` in `WidgetDescriptor` and nullable `IWidgetLifecycle*` pointer are plain C++17 std with no Qt version concern
- **yaml-cpp 0.7+**: Config and manifest parsing — extend for optional `widgets:` YAML block in plugin manifests; no schema validation library needed, manual `isValid()` is sufficient and already the project standard

**New source files (no new libs):**
- `src/core/widget/IWidgetLifecycle.hpp` — header-only optional per-instance lifecycle interface
- `src/core/widget/SystemWidgetRegistrar.hpp/.cpp` — pure refactor extracting main.cpp inline widget blocks

### Expected Features

**Must have (table stakes for a formalized plugin-widget contract):**
- `WidgetDescriptor` extended with capability flags (`isSystem`, `isRemovable`, `isRepositionable`), `category`, `description`, `apiVersion`, and nullable `IWidgetLifecycle*` — these fields are the manifest spec
- `IHostContext::widgetRegistry()` accessor — enables push-model plugin self-registration during `initialize()`
- `SystemWidgetRegistrar` — removes inline widget struct literals from `main.cpp`, registers Clock/AAStatus/NavTurn/NowPlaying with typed system vs user flags
- `IWidgetLifecycle` interface — optional per-instance C++ lifecycle hooks (onCreate, onActivate, onDeactivate, onResize, onDestroy)
- `WidgetInstanceContext` resize notification — change `cellWidth`/`cellHeight` from `CONSTANT` to `NOTIFY`-able Q_PROPERTYs; add `colSpan`/`rowSpan`
- `WidgetHost.qml` `pageVisible` property propagation — widgets pause expensive work when their page is not active
- `WidgetGridModel` placement validation — cross-check widgetIds against registry on load, skip orphaned entries with warning instead of crashing
- All four existing widgets refactored to use new descriptor fields and context properties
- `docs/plugin-development.md` — the architecture has no value without documentation

**Should have (meaningful DX improvement, add if time permits):**
- `plugin.yaml` optional `widgets:` section parsed into `WidgetManifestEntry` list — documents the contract in one file, enables future QML-only plugin widget registration
- Widget placeholder on QML load failure — visible error tile instead of silent blank glass card
- `IPlugin::onWidgetPageVisibilityChanged` — C++ companion to `pageVisible` QML property, for plugins with expensive background work

**Defer (v0.7+):**
- Widget picker category grouping and description display — v0.7 widget-based launcher consumes these fields
- Widget preview images in picker
- Widget-level per-instance settings YAML schema with UI generation
- Community plugin examples (OBD, camera, GPIO)

### Architecture Approach

The formalization restructures widget registration from a pull model (main.cpp harvests `widgetDescriptors()` after plugin init) to a push model (plugins call `hostContext->widgetRegistry()->registerWidget()` during `initialize()`). A new `SystemWidgetRegistrar` extracts the four inline widget registration blocks from main.cpp into a dedicated class. `IWidgetLifecycle` provides optional per-instance C++ callbacks wired through `WidgetInstanceContext`. Plugin lifecycle (full-screen view activation) and widget instance lifecycle (home screen grid cells) are formally documented as orthogonal — a plugin being deactivated must never affect widget placements.

**Major components and their v0.6 changes:**
1. `IHostContext` / `HostContext` — gains `widgetRegistry()` accessor; the dependency anchor, build first
2. `WidgetDescriptor` — gains seven new fields with safe defaults; pure struct change, all existing code compiles unchanged
3. `SystemWidgetRegistrar` (new) — pure refactor extracting main.cpp widget blocks; identical runtime behavior
4. `IWidgetLifecycle` (new) — header-only interface; nullable pointer on descriptor means zero overhead for stateless widgets
5. `WidgetInstanceContext` — gains `colSpan`/`rowSpan` NOTIFY properties and lifecycle hook dispatch
6. `WidgetGridModel` — gains placement validation on `setPlacements()`
7. Static plugins (AA, BtAudio, Phone, Equalizer) — migrate to self-registration in `initialize()`

### Critical Pitfalls

1. **Over-engineering the manifest format** — every field added to `WidgetDescriptor` or `plugin.yaml` must have a named consumer in the same phase diff. If no code reads it, it does not ship. No speculative fields, no "we'll need this for future third-party plugins" reasoning.

2. **Touching the AndroidAutoPlugin constructor** — it carries legacy `Configuration` + `YamlConfig` parameters predating IHostContext. The code comment says "fix in later phases." This milestone does not touch the AA constructor for any reason. At most, add a `TODO(v0.7)` comment and move on.

3. **Widget lifecycle hooks firing before plugin is initialized** — `initializeAll()` must complete before any widget instance is created. `WidgetGridModel` populates instances lazily after `initializeAll()` returns. Add `Q_ASSERT(plugin->isInitialized())` before any lifecycle hook call.

4. **QML widget context not scoped per instance** — widget instances must each get their own `QQmlContext` as a child of the plugin context, not the root context. Two `NowPlayingWidget` instances on different pages must share service data (via shared plugin context ancestry) but have independent `instanceId` and per-instance config (per-instance child context).

5. **YAML config migration breaking existing layouts** — the split is absolute: descriptor metadata lives in code, placement data lives in YAML. `isSystem`, `isRemovable`, `category` are never stored in `widgets.yaml`. On `setPlacements()`, these values come from the registry, not from migration logic. Keep a v3 YAML fixture for regression testing before any migration code runs.

6. **IPlugin IID vs apiVersion confusion** — do not bump `OAP_PLUGIN_IID` unless the vtable actually changed. Adding new virtual methods with default implementations does not require an IID bump. Document the distinction in `IPlugin.hpp` before any refactor.

7. **Resize hook blocking the main thread** — `onResize` must return in under 1ms. Hooks store new colSpan/rowSpan values only; recalculation is scheduled via queued signal, never done synchronously in the hook body.

## Implications for Roadmap

Research converges on a natural 8-step build order where each step keeps the codebase compiling. The steps form three logical phases: contract foundation, lifecycle wiring, and plugin migration + proof.

### Phase 1: Contract Foundation

**Rationale:** Three of the seven pitfalls are prevented entirely by getting contracts and boundaries right before writing any lifecycle code. This phase defines the spec; later phases implement it. No observable behavior change.

**Delivers:**
- Enriched `WidgetDescriptor` struct (all new fields with safe defaults)
- `IWidgetLifecycle` interface header
- `IHostContext::widgetRegistry()` accessor wired through `HostContext`
- `SystemWidgetRegistrar` class (pure refactor of main.cpp inline blocks — identical runtime behavior)
- Documentation: IID vs apiVersion distinction, AndroidAutoPlugin constructor freeze, descriptor-vs-YAML split, plugin-lifecycle vs widget-lifecycle separation as orthogonal

**Addresses features from FEATURES.md:** Widget capability flags, category, description, apiVersion — all struct additions that enable everything downstream.

**Avoids pitfalls:** Over-engineering (no speculative fields), YAML migration breakage (establishes the split before any migration code), IID confusion, AA constructor breakage.

**Research flag:** Standard patterns — no research phase needed. Pure struct changes and interface definition. Well-understood Qt patterns, directly analogous to Qt Creator plugin lifecycle.

### Phase 2: Widget Lifecycle Wiring

**Rationale:** With contracts defined, this phase wires them up. `WidgetInstanceContext` gains resize notification and lifecycle dispatch. `WidgetGridModel` gains placement validation. `WidgetHost.qml` gains `pageVisible` propagation. Pitfalls 3, 4, and 7 are live here and must be actively avoided.

**Delivers:**
- `WidgetInstanceContext` with NOTIFY Q_PROPERTYs for `colSpan`/`rowSpan` and optional `IWidgetLifecycle*` dispatch
- `WidgetGridModel` placement validation (skip orphaned widgetIds with qCWarning)
- `WidgetHost.qml` `pageVisible` bool propagated from SwipeView current page index
- Unit tests: mock lifecycle ordering guarantee, placement validation with orphaned id, two-instance context scope verification
- Performance check: `QSG_RENDER_TIMING=1` on Pi confirms <16ms frame times during resize drag

**Uses from STACK.md:** Qt `Q_PROPERTY` with NOTIFY (both Qt 6.4 and 6.8), QTest for ordering tests.

**Implements from ARCHITECTURE.md:** Widget instance lifecycle flow (create → activate → resize → deactivate → destroy), QML context hierarchy (engine root → plugin child → widget instance child).

**Avoids pitfalls:** Lifecycle hooks before plugin init (initializeAll ordering enforced), context scope collision (per-instance context hierarchy established here), resize thread blocking (documented <1ms constraint in hook contract before hooks are wired).

**Research flag:** Needs careful attention during implementation — context scoping and lifecycle ordering are the most likely failure points. Standard Qt patterns but ordering-sensitive. Write the unit tests before writing the wiring.

### Phase 3: Plugin Migration and Reference Implementation

**Rationale:** The architecture is proven only when the four existing plugins and widgets use it. This phase migrates all static plugins to push-based registration, refactors widget descriptors with new fields, adds `pageVisible` guards to widget QML, and writes developer documentation.

**Delivers:**
- All four static plugins migrated to `initialize()`-time self-registration via `hostContext->widgetRegistry()`
- Main.cpp polling loop removed
- `IPlugin::widgetDescriptors()` marked deprecated (base implementation returns empty list for backward compat with dynamic plugins)
- Clock, AAStatus, NavTurn, NowPlaying widget descriptors updated with all new fields (isSystem, isRemovable, category, description)
- Widget QML updated: Timer pause on invisible pages (Clock, NowPlaying), responsive layout via colSpan/rowSpan instead of pixel breakpoints
- `docs/plugin-development.md` with full contract spec and a worked "hello world" widget example
- If time permits: `plugin.yaml` `widgets:` section parsed into `WidgetManifestEntry`

**Addresses features from FEATURES.md:** Structured plugin-to-widget registration, refactor scope for existing widgets, documented plugin-widget contract.

**Avoids pitfalls:** Double-registration (remove polling loop in the same commit as plugin migration — atomic refactor, not a gradual transition), YAML migration correctness (v3 fixture test before merge).

**Research flag:** Standard patterns — no research phase needed. Mechanical migration following the contract defined in Phase 1. All decisions made before this phase begins.

### Phase Ordering Rationale

- **Phase 1 before Phase 2:** Lifecycle hooks cannot be wired correctly without the interface and context hierarchy defined first. Writing hooks before the contract is a live pitfall vector.
- **Phase 2 before Phase 3:** Plugins cannot self-register widgets that use new descriptor fields until `WidgetInstanceContext` correctly handles them. Migrating plugins before lifecycle wiring is tested creates a double failure surface.
- **Phase 3 validates Phases 1+2:** Working reference implementations are the proof that the architecture is correct. A failure in Phase 3 points back to a gap in the foundation, not to the migration itself.
- **Each phase preserves a compiling, working codebase:** The 8-step internal build order (WidgetDescriptor enrichment → IWidgetLifecycle → widgetRegistry accessor → SystemWidgetRegistrar → lifecycle wiring → grid validation → plugin migration → manifest YAML) allows git bisect to isolate any regression.

### Research Flags

Needs careful implementation attention (internal discipline, not external research):
- **Phase 2 — context scoping:** QML context hierarchy for per-widget-instance context creation. Pitfall 4 does not produce a compile error — it produces a subtle runtime bug that only appears with two instances of the same widget on different pages.
- **Phase 2 — lifecycle ordering:** `initializeAll()` must complete before any widget instance is created. Write the unit test before writing the wiring code.

Standard patterns (no `/gsd:research-phase` needed):
- **Phase 1** — pure struct and interface additions; zero Qt version ambiguity; all decisions are engineering judgment calls, not research questions
- **Phase 3** — mechanical migration following the v0.6 contract; all decisions made in Phases 1 and 2

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Direct codebase inspection confirmed all capabilities exist. No new libraries needed. Qt 6.4/6.8 dual-version compatibility verified for all planned additions — no Qt 6.8-only APIs required. |
| Features | HIGH | Based on codebase inspection + Android AppWidget and Qt Creator plugin lifecycle as reference designs. Clear MVP vs defer split with concrete rationale for each item. |
| Architecture | HIGH | All components read from source. Build order validated against actual file dependencies. Component responsibilities match existing patterns in the codebase. |
| Pitfalls | HIGH | Three pitfalls are specific to this codebase (AA constructor, YAML migration, context scoping) and confirmed against source files. Four are general Qt patterns with documented behavior. All seven have specific prevention strategies and verification criteria. |

**Overall confidence:** HIGH

### Gaps to Address

- **NowPlayingWidget context binding:** The widget uses `typeof MediaBridge !== "undefined"` guards and references `BtAudioPlugin` by QML context name. Phase 2 context scoping must verify that this guard still works when the widget instance context is a grandchild of the root context rather than a direct child. The plugin child context must be in the ancestry chain for all placed widgets — not only during plugin activation.
- **Dynamic plugin migration path:** The research specifies the polling loop is removed atomically with static plugin migration. If any dynamic plugins exist in `~/.openauto/plugins/`, they use `widgetDescriptors()`. The deprecated-but-kept base implementation handles this, but the interaction needs to be explicit in `docs/plugin-development.md`: dynamic plugins continue via the old path until they individually migrate.
- **`WidgetPickerModel` category role exposure:** Category grouping in the picker is a v0.7 feature, but the `category` field is added in v0.6. Confirm that `WidgetPickerModel` exposes `category` as a data model role even if the picker QML does not group by it yet — otherwise v0.7 has a silent model gap to fill on first use.

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection: `src/core/plugin/IPlugin.hpp`, `IHostContext.hpp`, `HostContext.hpp`, `PluginManager.cpp`, `PluginManifest.hpp`, `WidgetTypes.hpp`, `WidgetRegistry.hpp`, `WidgetGridModel.hpp`, `WidgetInstanceContext.hpp`, `WidgetPickerModel.hpp`, `AndroidAutoPlugin.hpp`, `BtAudioPlugin.hpp/.cpp`, `PluginRuntimeContext.cpp`, `main.cpp` lines 388-442, `qml/widgets/ClockWidget.qml`, `qml/widgets/NowPlayingWidget.qml`
- `.planning/PROJECT.md` — v0.6 milestone goals and scope constraints
- Qt Creator Plugin Lifecycle docs — initialize/extensionsInitialized/aboutToShutdown ordering; the canonical C++ plugin lifecycle pattern
- Qt QQmlContext Class docs — property injection, context lifecycle, performance notes
- Qt How to Create Qt Plugins docs — confirmed QQmlEngineExtensionPlugin is the wrong tool for this use case

### Secondary (MEDIUM confidence)
- Android AppWidget developer docs — AppWidgetProviderInfo fields (resizeMode, widgetCategory, widgetFeatures), lifecycle callbacks — used as reference design comparison, not direct adoption
- Qt Application Manager Manifest docs — categories, backgroundMode, capabilities patterns for IVI context
- Plugin-based IVI Architectures with Qt (ICSinc) — IVI plugin patterns in Qt automotive context
- yaml-cpp issue #1017 — confirmed no built-in schema validation; manual `isValid()` is standard practice

### Tertiary (LOW confidence)
- Figma Widget Manifest — widget manifest field patterns for capability declaration (reference only; very different runtime model from in-process QML widgets)

---
*Research completed: 2026-03-13*
*Ready for roadmap: yes*
