# Phase 12: Documentation - Research

**Researched:** 2026-03-15
**Domain:** Technical documentation (developer guide + architecture decision records)
**Confidence:** HIGH

## Summary

Phase 12 is a pure documentation phase with no code changes. The deliverables are two documents: (1) a plugin-widget developer guide that enables a third-party developer to create a widget plugin from scratch, and (2) architecture decision records capturing the key design choices made during v0.6/v0.6.1.

The existing `docs/plugin-api.md` covers plugin lifecycle and IHostContext services comprehensively but needs to be extended with a practical "how to build a widget" tutorial. The widget contract is well-defined in code: `WidgetDescriptor` fields, `WidgetInstanceContext` properties, QML conventions (`widgetContext` property, span-based breakpoints, `requestContextMenu` forwarding), and the critical `QT_QML_SKIP_CACHEGEN` CMake requirement. The existing `docs/design-decisions.md` covers early v0.4 decisions (video pipeline, touch, transport) but has no entries for the v0.6-v0.6.1 widget/grid/architecture decisions captured in STATE.md.

**Primary recommendation:** Write two markdown files: `docs/widget-developer-guide.md` (tutorial-style, self-contained) and update `docs/design-decisions.md` with v0.6-v0.6.1 ADRs. No code changes needed.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DOC-01 | Plugin-widget developer guide covers manifest spec, registration API, lifecycle, and sizing conventions | Existing `plugin-api.md` provides foundation; widget contract fully defined in `WidgetTypes.hpp`, `WidgetInstanceContext.hpp`, `WidgetContextFactory.hpp`; 6 example widgets in `qml/widgets/`; CMake patterns in `src/CMakeLists.txt` |
| DOC-02 | Architecture decision records document key design choices for future contributors | 13+ decisions captured in `STATE.md` under `### Decisions`; existing `design-decisions.md` covers v0.4 only; v0.6-v0.6.1 decisions need ADR treatment |
</phase_requirements>

## Standard Stack

This phase produces documentation only. No new libraries or dependencies.

### Tools
| Tool | Purpose | Why |
|------|---------|-----|
| Markdown | Document format | Already used throughout `docs/` |
| Existing `docs/` structure | Organization | `docs/INDEX.md` serves as central index |

## Architecture Patterns

### Document Structure

```
docs/
├── plugin-api.md                # Existing — reference API docs (keep as-is)
├── widget-developer-guide.md    # NEW — tutorial-style guide (DOC-01)
├── design-decisions.md          # EXTEND — add v0.6-v0.6.1 ADRs (DOC-02)
└── INDEX.md                     # UPDATE — add new doc links
```

### Pattern 1: Tutorial-Style Developer Guide (DOC-01)

**What:** A self-contained document that walks a developer through creating a widget plugin from zero to working widget.

**Structure:**
1. **Prerequisites** — Qt 6.8, CMake, C++17, project build working
2. **Widget Concepts** — WidgetDescriptor, WidgetInstanceContext, grid model, categories
3. **Step-by-Step: Your First Widget** — concrete example building a simple custom widget
4. **Manifest Spec** — all WidgetDescriptor fields with types, defaults, valid values
5. **Registration** — `widgetDescriptors()` override vs main.cpp `WidgetRegistry::registerWidget()`
6. **QML Contract** — `widgetContext` property, span-based breakpoints, provider access, context menu forwarding, UiMetrics usage, ThemeService colors
7. **Sizing Conventions** — min/max/default cols/rows, responsive breakpoints using `colSpan`/`rowSpan` (not pixels)
8. **Category Taxonomy** — status, media, navigation, launcher (with hardcoded order)
9. **CMake Setup** — `QT_QML_SKIP_CACHEGEN TRUE` requirement, `set_source_files_properties`, resource alias
10. **Gotchas** — MouseArea z-order for pressAndHold, opacity signal naming, Loader URL caching

**Key content sources (all in-repo, HIGH confidence):**
- `src/core/widget/WidgetTypes.hpp` — WidgetDescriptor struct definition
- `src/ui/WidgetInstanceContext.hpp` — context properties available to QML
- `src/core/plugin/IPlugin.hpp` — `widgetDescriptors()` virtual method
- `src/main.cpp:470-556` — concrete registration examples (6 widgets)
- `qml/widgets/ClockWidget.qml` — simplest widget, good tutorial model
- `qml/widgets/NowPlayingWidget.qml` — complex widget with providers and controls
- `src/CMakeLists.txt:254-283` — QML resource setup with SKIP_CACHEGEN
- `docs/plugin-api.md` — existing API reference to cross-reference

### Pattern 2: Architecture Decision Records (DOC-02)

**What:** Extend `docs/design-decisions.md` with v0.6-v0.6.1 decisions in the existing format (section header, decision, rationale).

**Decisions to document (from STATE.md `### Decisions`):**

| Decision | Source |
|----------|--------|
| DPI-based grid sizing: `cellSide = diagPx / (9.0 + bias * 0.8)` | Phase 09 |
| Grid cols/rows computed in QML from DisplayInfo.cellSide (reactive) | Phase 09 |
| Dock replaced by z=10 overlay outside ColumnLayout | Phase 10 |
| JS-based model filtering in QML (not C++ proxy model) for categories | Phase 09 |
| Category order hardcoded in static map | Phase 09 |
| Remap clamps oversized spans rather than hiding widgets | Phase 09 |
| promoteToBase() on every user edit mutation | Phase 09 |
| Base snapshot pattern: basePlacements_ vs livePlacements_ | Phase 09 |
| Reserved page derived from singleton presence (pageHasSingleton) | Phase 10 |
| Fixed instanceIds for seeded singletons | Phase 10 |
| Auto-snap threshold (60% waste, single-pass) | Phase 10.1 |
| Clock as active page indicator (dot encoding) | Phase 10.1 |
| WidgetContextFactory as dedicated class (not on WidgetGridModel) | Phase 11 |
| Context injection via Loader.onLoaded + Binding elements | Phase 11 |
| NowPlayingWidget controls stay as provider methods, not ActionRegistry | Phase 11 |

**Format:** Follow the existing ADR style in `design-decisions.md` — markdown section with "Decision:" and "Rationale:" subsections.

### Anti-Patterns to Avoid

- **Don't duplicate plugin-api.md content** — the developer guide should reference it, not copy it. Widget guide focuses on the widget-specific contract; plugin-api.md covers IPlugin lifecycle and IHostContext services.
- **Don't write speculative docs** — only document what exists today. Dynamic plugin loading is "implemented but untested" per existing docs; note this but don't write a full tutorial for it.
- **Don't invent new conventions** — document the patterns visible in the 6 existing widgets, not hypothetical best practices.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| API reference | Manual field-by-field docs | Reference existing `plugin-api.md` | Already comprehensive and maintained |
| Code examples | Synthetic examples | Excerpts from actual widgets | Real code is always more accurate than made-up examples |
| Diagrams | ASCII art architecture diagrams | Text descriptions with file paths | Diagrams go stale; file paths are grep-verifiable |

## Common Pitfalls

### Pitfall 1: QT_QML_SKIP_CACHEGEN Omission
**What goes wrong:** Widget QML loaded via `Loader { source: url }` fails silently at runtime — the file is replaced by compiled C++ during cachegen, and the URL lookup returns nothing.
**Why it happens:** Qt 6.8 cachegen converts raw .qml to compiled C++ by default. Widgets loaded dynamically by URL need the raw .qml to exist in the resource tree.
**How to avoid:** Every widget QML file MUST have `QT_QML_SKIP_CACHEGEN TRUE` in its `set_source_files_properties()` block.
**Warning signs:** Widget area shows blank/empty on the home screen despite correct registration.

### Pitfall 2: MouseArea z-order for pressAndHold
**What goes wrong:** Long-press context menu never fires on widgets that have interactive controls (buttons, sliders).
**Why it happens:** A top-level MouseArea steals press events from child controls. Using `onPressed: mouse.accepted = false` prevents pressAndHold from ever firing.
**How to avoid:** Place the context-menu MouseArea at `z: -1` behind widget content. Each interactive control's own MouseArea should also forward pressAndHold.
**Warning signs:** Widget controls work but long-press to edit does nothing.

### Pitfall 3: Hardcoded pixel breakpoints
**What goes wrong:** Widget looks fine on one display resolution but breaks on another.
**Why it happens:** Using pixel-based conditionals like `width > 300` instead of span-based like `colSpan >= 3`.
**How to avoid:** All responsive behavior should key off `widgetContext.colSpan` and `widgetContext.rowSpan`, not pixel dimensions.

### Pitfall 4: Missing null-guard on widgetContext
**What goes wrong:** QML binding errors at startup before context injection completes.
**Why it happens:** `widgetContext` starts as `null` and is set after the Loader finishes loading.
**How to avoid:** Always use ternary guards: `widgetContext ? widgetContext.colSpan : 1`.

### Pitfall 5: Using opacity signal name
**What goes wrong:** QML error about duplicate signal — `Item.opacity` already generates `opacityChanged`.
**Why it happens:** QML auto-generates change signals for built-in properties.
**How to avoid:** Never name custom signals `opacityChanged` on QML Items.

## Code Examples

These are the actual patterns from the codebase that the developer guide should present.

### Widget QML Contract (from ClockWidget.qml)
```qml
// Source: qml/widgets/ClockWidget.qml
Item {
    id: clockWidget
    clip: true

    // Widget contract: context injection from host
    property QtObject widgetContext: null

    // Span-based breakpoints for responsive layout
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1
    readonly property bool showDate: colSpan >= 2

    // Long-press for context menu (edit mode)
    MouseArea {
        anchors.fill: parent
        z: -1  // behind widget content
        pressAndHoldInterval: 500
        onPressAndHold: {
            if (clockWidget.parent && clockWidget.parent.requestContextMenu)
                clockWidget.parent.requestContextMenu()
        }
    }
}
```

### Widget Registration (from main.cpp)
```cpp
// Source: src/main.cpp:470-480
oap::WidgetDescriptor clockDesc;
clockDesc.id = "org.openauto.clock";
clockDesc.displayName = "Clock";
clockDesc.iconName = "\ue8b5";  // Material icon codepoint
clockDesc.category = "status";
clockDesc.description = "Current time";
clockDesc.minCols = 1; clockDesc.minRows = 1;
clockDesc.maxCols = 6; clockDesc.maxRows = 4;
clockDesc.defaultCols = 2; clockDesc.defaultRows = 2;
clockDesc.qmlComponent = QUrl("qrc:/OpenAutoProdigy/ClockWidget.qml");
widgetRegistry->registerWidget(clockDesc);
```

### Plugin-Provided Widget (from IPlugin.hpp)
```cpp
// Source: src/core/plugin/IPlugin.hpp:52
virtual QList<WidgetDescriptor> widgetDescriptors() const { return {}; }
```

### CMake Widget QML Setup (from src/CMakeLists.txt)
```cmake
# Source: src/CMakeLists.txt:254-257
set_source_files_properties(../qml/widgets/ClockWidget.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "ClockWidget"
    QT_RESOURCE_ALIAS "ClockWidget.qml"
    QT_QML_SKIP_CACHEGEN TRUE  # REQUIRED for Loader-based widget loading
)
```

### Provider Access (from NowPlayingWidget.qml)
```qml
// Source: qml/widgets/NowPlayingWidget.qml
property bool hasMedia: widgetContext && widgetContext.mediaStatus
                        ? widgetContext.mediaStatus.hasMedia : false
property string title: widgetContext && widgetContext.mediaStatus
                       ? (widgetContext.mediaStatus.title || "") : ""
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Pane-based widget placement (3-pane layout) | Grid-based placement with col/row spans | v0.5.3 (Phase 06) | Widgets can be arbitrary sizes |
| Pixel-based responsive breakpoints | Span-based breakpoints (`colSpan >= 3`) | v0.6.1 (Phase 11) | Resolution-independent widget layouts |
| Widget context via QML context properties | Dedicated `WidgetInstanceContext` object via `widgetContext` property | v0.6.1 (Phase 11) | Typed, documented, NOTIFY-capable |
| Fixed LauncherDock bottom bar | Singleton launcher widgets on reserved page | v0.6.1 (Phase 10) | Reclaimed vertical space for grid |

## Open Questions

1. **Dynamic plugin widget loading**
   - What we know: `PluginManager::discoverPlugins()` loads `.so` files from `~/.openauto/plugins/`; `IPlugin::widgetDescriptors()` is called during registration. The infrastructure exists.
   - What's unclear: Whether dynamic loading actually works end-to-end (docs say "implemented but untested").
   - Recommendation: Document as "available but experimental" in the developer guide. Focus tutorial on static plugin widgets.

2. **LiveSurfaceWidget contribution kind**
   - What we know: Declared in `DashboardContributionKind` enum, filtered out by `WidgetPickerModel`. No runtime host exists.
   - What's unclear: When/if this will ship.
   - Recommendation: Mention in the manifest spec table with "deferred" status. Don't write usage docs.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | CTest + Qt Test (via CMake) |
| Config file | `tests/CMakeLists.txt` |
| Quick run command | `cd build && ctest --output-on-failure` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| DOC-01 | Developer guide covers manifest, registration, lifecycle, sizing, categories, CMake | manual-only | N/A (prose review) | N/A |
| DOC-02 | ADRs document v0.6-v0.6.1 design choices | manual-only | N/A (prose review) | N/A |

**Justification for manual-only:** Documentation correctness is verified by reading, not by automated tests. The underlying APIs and contracts being documented ARE tested by existing unit tests (`test_widget_types`, `test_widget_registry`, `test_widget_instance_context`, `test_widget_grid_model`, `test_widget_contract_qml`, `test_widget_plugin_integration`).

### Sampling Rate
- **Per task commit:** Verify doc links resolve, code examples match actual source
- **Per wave merge:** Read-through for accuracy and completeness
- **Phase gate:** Success criteria checklist review

### Wave 0 Gaps
None -- this phase produces documentation only. No test infrastructure changes needed.

## Sources

### Primary (HIGH confidence)
- `src/core/widget/WidgetTypes.hpp` — WidgetDescriptor struct, GridPlacement struct
- `src/ui/WidgetInstanceContext.hpp` — context properties for widget QML
- `src/ui/WidgetContextFactory.hpp` — factory for context creation
- `src/core/plugin/IPlugin.hpp` — widgetDescriptors() virtual
- `src/main.cpp:470-556` — all 6 widget registration examples
- `src/CMakeLists.txt:254-283` — QML SKIP_CACHEGEN setup
- `qml/widgets/*.qml` — 6 reference widget implementations
- `docs/plugin-api.md` — existing API reference
- `docs/design-decisions.md` — existing ADR document
- `.planning/STATE.md` — v0.6-v0.6.1 decisions list

### Secondary (MEDIUM confidence)
- None needed -- all sources are in-repo

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - no libraries, just markdown docs
- Architecture: HIGH - document structure follows existing `docs/` patterns
- Pitfalls: HIGH - all pitfalls are documented gotchas from actual development (in CLAUDE.md and MEMORY.md)

**Research date:** 2026-03-15
**Valid until:** 2026-04-15 (stable -- documents existing finalized APIs)
