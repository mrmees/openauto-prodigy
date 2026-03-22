# Phase 11: Widget Framework Polish - Research

**Researched:** 2026-03-14
**Domain:** Qt 6 QML widget framework formalization (sizing, lifecycle, data injection)
**Confidence:** HIGH

## Summary

Phase 11 formalizes the widget framework contract across four areas: size/layout, data+actions, lifecycle/performance, and authoring/docs. The core implementation work is (1) resize clamping enforcement in the QML drag handle, (2) exposing live `colSpan`/`rowSpan` properties to widgets via the context, and (3) rewriting all 6 widgets to use span-based breakpoints instead of pixel breakpoints.

The existing codebase is well-structured for this work. `WidgetGridModel` already enforces min/max in `resizeWidget()` on the C++ side. The QML resize handle in `HomeMenu.qml` already clamps against `model.minCols`/`model.maxCols`/etc. during drag (lines 500-503). `WidgetInstanceContext` already exists with provider injection but lacks `colSpan`/`rowSpan` properties. The gap is narrow: add span properties to context, plumb them as live-updating, and rewrite widget QML to use spans instead of pixel thresholds.

**Primary recommendation:** Structure as 3 waves -- (1) C++ infrastructure (WidgetInstanceContext span properties + resize clamping verification), (2) widget rewrites (all 6 widgets to span-based), (3) contract documentation.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Single QML component per widget with span-driven internal adaptation
- Host exposes `colSpan` and `rowSpan` as live properties (with NOTIFY) -- these are the stable layout contract
- Spans only -- no derived convenience booleans (isCompact, isWide, etc.)
- No custom created/destroying signals -- QML `Component.onCompleted`/`Component.onDestruction` are sufficient
- Resize clamping: WidgetGridModel enforces descriptor min/max as single source of truth during drag-resize
- Widget data providers injected via WidgetInstanceContext / IHostContext, NOT fished from root QML context
- Ambient platform singletons (ThemeService, UiMetrics) may stay ambient
- No aspect ratio enforcement
- Performance rules are strict, not guidelines
- Normal widgets must reserve press-and-hold for host edit/context handling
- Muted placeholder state for error/empty/disconnected (dimmed ~40% opacity, widget icon, short status text)
- Rewrite all 4 content widgets as primary reference implementations + update 2 launcher widgets as compliance targets
- Missing capability handling deferred -- document as future contract rule, do NOT add capability metadata or picker filtering

### Claude's Discretion
- Exact resize clamping UX (visual feedback during drag-resize when hitting min/max limits)
- How colSpan/rowSpan are injected into widget context (model role bindings vs explicit QObject properties)
- Whether to create a WidgetContract.md or fold the contract into the existing plugin-api.md
- Instance config API surface (read/write methods, change notification mechanism)
- Which existing widget serves as the primary documented walkthrough vs supporting examples
- Stale comment cleanup in HomeMenu.qml (line 118 "above page dots" reference)

### Deferred Ideas (OUT OF SCOPE)
- Capability registry for cross-plugin data sharing
- LiveSurface contribution type (document boundary only)
- Widget settings UI pattern
- Widget-specific controller QObject
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| WF-02 | WidgetGridModel enforces min/max size constraints from WidgetDescriptor as single source of truth | Resize clamping already exists in C++ (`resizeWidget()` lines 177-186) and QML drag handle (lines 500-503). Need verification that the QML clamp is visually responsive and prevents overshoot. |
| WF-03 | WidgetHost emits opt-in lifecycle signals (created, resized, destroying) observable by widget QML | CONTEXT.md overrides this: NO custom lifecycle signals. Use QML `Component.onCompleted`/`onDestruction` + live `colSpan`/`rowSpan` properties with NOTIFY. WidgetInstanceContext needs new Q_PROPERTYs. |
| WF-04 | Widget QML uses grid-span or UiMetrics-based thresholds instead of hardcoded pixel breakpoints | All 4 content widgets have pixel breakpoints (ClockWidget: 250/180, AAStatus: 250, NowPlaying: 400/180, Navigation: 400/180/500). Replace with colSpan/rowSpan checks. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt 6.8 | 6.8.2 | QML UI framework | Already in use, provides property bindings, NOTIFY signals, QAbstractListModel |
| C++17 | gcc 12+ | WidgetInstanceContext, WidgetGridModel | Already in use |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| QtTest | 6.8.2 | Unit tests | Existing test framework (`oap_add_test()` helper) |

No new dependencies needed. This phase is purely internal refactoring of existing code.

## Architecture Patterns

### Current Widget Data Flow
```
main.cpp
  ├── engine.rootContext()->setContextProperty("ProjectionStatus", ...)  // GLOBAL
  ├── engine.rootContext()->setContextProperty("NavigationProvider", ...)  // GLOBAL
  └── engine.rootContext()->setContextProperty("MediaStatus", ...)        // GLOBAL

Widget QML:
  property bool connected: typeof ProjectionStatus !== "undefined" ...   // FISHING FROM GLOBALS
```

### Target Widget Data Flow
```
WidgetInstanceContext (C++ QObject, per-instance)
  ├── Q_PROPERTY(int colSpan READ colSpan NOTIFY colSpanChanged)     // NEW
  ├── Q_PROPERTY(int rowSpan READ rowSpan NOTIFY rowSpanChanged)     // NEW
  ├── Q_PROPERTY(QObject* projectionStatus ...)                       // EXISTS
  ├── Q_PROPERTY(QObject* navigationProvider ...)                     // EXISTS
  └── Q_PROPERTY(QObject* mediaStatus ...)                            // EXISTS

HomeMenu.qml delegate:
  Loader {
      // Set context property so widget QML can access widgetContext.colSpan etc.
  }

Widget QML:
  readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
  readonly property bool showText: colSpan >= 2
```

### Pattern 1: colSpan/rowSpan Injection via QML Context Property

**What:** WidgetInstanceContext gets `colSpan`/`rowSpan` as live Q_PROPERTYs with NOTIFY. HomeMenu.qml creates a WidgetInstanceContext per delegate, updates spans when `model.colSpan`/`model.rowSpan` change, and sets it as a context property on the Loader.

**Why this over model role bindings:** Model roles (`model.colSpan`) are already available in the delegate scope but NOT inside the Loader's loaded component. The loaded widget QML cannot access `model.*` from its parent Loader. A context property or explicit property is needed.

**Two sub-options (Claude's discretion):**

Option A -- **QML context property** (recommended):
```qml
// In HomeMenu.qml delegate, before Loader:
property var widgetCtx: WidgetInstanceContext {
    // C++ object created per delegate, updated on model changes
}

Loader {
    // Widget accesses widgetContext.colSpan
}
```
The C++ WidgetInstanceContext already exists. Add `colSpan`/`rowSpan` as writable properties. HomeMenu.qml's delegate binds `widgetCtx.colSpan = model.colSpan` to keep them live.

Option B -- **Loader property forwarding:**
```qml
Loader {
    property int colSpan: model.colSpan
    property int rowSpan: model.rowSpan
    // Widget accesses parent.colSpan
}
```
Simpler but widgets reference `parent.colSpan` which is fragile (parent assumptions).

**Recommendation:** Option A. WidgetInstanceContext already exists, already has provider injection. Adding span properties there gives widgets a single stable interface (`widgetContext.colSpan`, `widgetContext.projectionStatus`, etc.). This is also what the CONTEXT.md prescribes.

### Pattern 2: Span-Based Breakpoints in Widget QML

**What:** Replace pixel breakpoints with span checks.

**Current (pixel-based):**
```qml
readonly property bool showDate: width >= 250   // fragile, DPI-dependent
readonly property bool showDay: height >= 180
```

**Target (span-based):**
```qml
readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1
readonly property bool showDate: colSpan >= 2
readonly property bool showDay: rowSpan >= 2
```

**Mapping of existing breakpoints to span thresholds:**

| Widget | Current Pixel Check | Span Equivalent | Rationale |
|--------|-------------------|-----------------|-----------|
| ClockWidget | `width >= 250` | `colSpan >= 2` | 250px ~ 2 cells at typical cell size (120-150px) |
| ClockWidget | `height >= 180` | `rowSpan >= 2` | 180px ~ 2 cells |
| AAStatusWidget | `width >= 250` | `colSpan >= 2` | Same threshold class |
| NowPlayingWidget | `width >= 400` | `colSpan >= 3` | 400px ~ 3 cells |
| NowPlayingWidget | `height >= 180` | `rowSpan >= 2` | Same as Clock |
| NavigationWidget | `width >= 400` | `colSpan >= 3` | Same as NowPlaying |
| NavigationWidget | `height >= 180` | `rowSpan >= 2` | Same as Clock |
| NavigationWidget | `width >= 500` | `colSpan >= 4` | 500px ~ 4 cells (wide variant) |

### Pattern 3: Placeholder State (Formalized)

**What:** Standard muted state when widget's data source is unavailable.

Already converged in NowPlayingWidget and NavigationWidget:
```qml
Item {
    anchors.centerIn: parent
    visible: !dataAvailable
    opacity: 0.4

    ColumnLayout {
        anchors.centerIn: parent
        spacing: UiMetrics.spacing

        MaterialIcon {
            icon: "\ue405"  // widget-specific icon
            size: rowSpan >= 2 ? UiMetrics.iconSize * 2 : UiMetrics.iconSize * 1.5
            color: ThemeService.onSurfaceVariant
            Layout.alignment: Qt.AlignHCenter
        }

        NormalText {
            text: "No data"  // widget-specific message
            visible: colSpan >= 2 || rowSpan >= 2
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.onSurfaceVariant
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
```

### Pattern 4: pressAndHold Context Menu Forwarding

**What:** All widgets must forward press-and-hold to the host for edit/context handling.

Already established pattern:
```qml
MouseArea {
    anchors.fill: parent
    z: -1  // behind interactive controls
    pressAndHoldInterval: 500
    onPressAndHold: {
        if (root.parent && root.parent.requestContextMenu)
            root.parent.requestContextMenu()
    }
}
```

Interactive widgets (NowPlaying, Navigation, AAStatus) that have their own MouseAreas for tap targets must also detect pressAndHold on those areas and forward it.

### Anti-Patterns to Avoid
- **Pixel breakpoints in widget QML:** Use `colSpan >= N` not `width >= Npx`. Pixels are DPI-dependent and break across displays.
- **Accessing root context globals:** Use `widgetContext.projectionStatus` not `ProjectionStatus` from root QML context.
- **Custom lifecycle signals:** Use `Component.onCompleted`/`onDestruction` and bind to live properties. No widget-specific lifecycle protocol.
- **Polling timers without justification:** ClockWidget's 1-second timer is the ONE justified case (no event-driven clock source in QML). All other widgets should be event-driven via property bindings.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Widget data access | Custom signal/slot per widget | WidgetInstanceContext Q_PROPERTYs | Already exists, provides typed injection |
| Resize clamping | Custom QML constraint logic | WidgetGridModel min/max + model roles | Already implemented, just needs verification |
| Responsive layout | Custom breakpoint framework | colSpan/rowSpan from context | Spans are stable, pixel-independent |
| Theme compliance | Per-widget color management | ThemeService/UiMetrics singletons | Ambient access is correct for these |

## Common Pitfalls

### Pitfall 1: WidgetInstanceContext Lifetime vs Model Updates
**What goes wrong:** WidgetInstanceContext is created once with initial placement data but colSpan/rowSpan change when user resizes. If properties are CONSTANT, widgets won't reflow.
**Why it happens:** Current WidgetInstanceContext properties are all CONSTANT.
**How to avoid:** Add `colSpan`/`rowSpan` as non-CONSTANT Q_PROPERTYs with NOTIFY signals. Provide setter methods. HomeMenu.qml delegate must call setters when model data changes (bind via Connections on WidgetGridModel dataChanged or via QML property bindings).
**Warning signs:** Widget doesn't reflow after resize until page is re-entered.

### Pitfall 2: Loader Component Context Isolation
**What goes wrong:** Widget QML loaded via `Loader { source: url }` cannot access properties defined on the delegate Item that contains the Loader.
**Why it happens:** QML Loader creates a separate component scope. `model.*` roles are available in the delegate but not inside the loaded component.
**How to avoid:** Either (a) set a context property on the Loader's QQmlContext, or (b) define properties on the Loader itself that the widget accesses via `parent.propertyName`. Option (a) via WidgetInstanceContext is cleaner.
**Warning signs:** `ReferenceError: widgetContext is not defined` at runtime.

### Pitfall 3: Removing Root Context Properties Too Early
**What goes wrong:** If you remove `ProjectionStatus`/`NavigationProvider`/`MediaStatus` root context properties before all widgets are updated, existing widgets break.
**Why it happens:** Multiple consumers (widgets, possibly other QML files) may reference these globals.
**How to avoid:** Phase 11 adds WidgetInstanceContext-based access in all widgets. Root context properties should remain as ambient access for non-widget consumers. Don't remove them in this phase.
**Warning signs:** `TypeError: Cannot read property 'projectionState' of null` in non-widget QML.

### Pitfall 4: QML SKIP_CACHEGEN for Widget Files
**What goes wrong:** Widget QML files loaded via Loader URL fail on Pi when Qt 6.8 cachegen replaces raw .qml with compiled C++.
**Why it happens:** Known gotcha documented in CLAUDE.md -- files loaded via `Loader { source: url }` need `QT_QML_SKIP_CACHEGEN TRUE` in CMake.
**How to avoid:** Ensure all widget .qml files in `qml/widgets/` have SKIP_CACHEGEN set. Verify this is already the case before making changes.
**Warning signs:** Works on dev machine, blank/crashed widget on Pi.

### Pitfall 5: Resize Ghost vs Actual Clamping Mismatch
**What goes wrong:** The resize ghost visual shows a size that the model rejects, or the ghost doesn't stop at min/max limits.
**Why it happens:** QML drag handler and C++ model have independent clamping logic.
**How to avoid:** The QML drag handler already clamps (lines 500-503 of HomeMenu.qml). Verify both paths agree. The QML clamp should be the primary visual constraint; the C++ `resizeWidget()` is the authoritative validation.
**Warning signs:** Ghost shows valid color but resize snaps back on release.

## Code Examples

### Adding colSpan/rowSpan to WidgetInstanceContext

```cpp
// WidgetInstanceContext.hpp additions:
Q_PROPERTY(int colSpan READ colSpan WRITE setColSpan NOTIFY colSpanChanged)
Q_PROPERTY(int rowSpan READ rowSpan WRITE setRowSpan NOTIFY rowSpanChanged)

public:
    int colSpan() const { return colSpan_; }
    int rowSpan() const { return rowSpan_; }
    void setColSpan(int v) { if (colSpan_ != v) { colSpan_ = v; emit colSpanChanged(); } }
    void setRowSpan(int v) { if (rowSpan_ != v) { rowSpan_ = v; emit rowSpanChanged(); } }

signals:
    void colSpanChanged();
    void rowSpanChanged();

private:
    int colSpan_ = 1;
    int rowSpan_ = 1;
```

### Widget QML with Span-Based Breakpoints (ClockWidget example)

```qml
Item {
    id: clockWidget
    clip: true

    // Span-based layout contract
    readonly property int colSpan: widgetContext ? widgetContext.colSpan : 1
    readonly property int rowSpan: widgetContext ? widgetContext.rowSpan : 1
    readonly property bool showDate: colSpan >= 2
    readonly property bool showDay: rowSpan >= 2

    // ... rest of widget using showDate/showDay for visibility
}
```

### HomeMenu.qml Context Injection (sketch)

```qml
// In the widget delegate, create or update context:
Loader {
    id: widgetLoader
    source: model.qmlComponent

    // Expose context to loaded widget
    property QtObject widgetContext: QtObject {
        property int colSpan: model.colSpan
        property int rowSpan: model.rowSpan
        // Provider access would need C++ WidgetInstanceContext for typed providers
    }
}
```

Note: The above is a simplified sketch. The actual implementation should use the C++ WidgetInstanceContext (already exists) with the new span properties, set as a QML context property on the Loader.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Pixel breakpoints in widgets | Span-based breakpoints | Phase 11 | DPI-independent widget layouts |
| Root context property globals | WidgetInstanceContext injection | Phase 11 | Clean data contract, testable |
| WidgetHost lifecycle signals | QML Component.onCompleted + live properties | Phase 11 (decision) | Simpler, idiomatic QML |

**Key insight:** WF-03 as written in REQUIREMENTS.md says "WidgetHost emits opt-in lifecycle signals (created, resized, destroying)". The CONTEXT.md discussion OVERRIDES this: no custom lifecycle signals. QML's built-in hooks plus live span properties are sufficient. The requirement is satisfied by providing the observable state change mechanism (live colSpan/rowSpan with NOTIFY), not by literally emitting named signals.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | QtTest 6.8.2 |
| Config file | `tests/CMakeLists.txt` (uses `oap_add_test()` helper) |
| Quick run command | `cd build && ctest --output-on-failure -R widget` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| WF-02 | Resize clamping at min/max | unit | `cd build && ctest --output-on-failure -R test_widget_grid_model` | Partial -- `testResizeWidgetBeyondMaxFails` exists, need below-min test |
| WF-03 | colSpan/rowSpan live properties with NOTIFY | unit | `cd build && ctest --output-on-failure -R test_widget_instance_context` | Partial -- needs span property tests |
| WF-04 | Span-based breakpoints (no pixel thresholds) | manual-only | Grep for `width >=` or `height >=` in `qml/widgets/` -- should find none | N/A |

### Sampling Rate
- **Per task commit:** `cd build && ctest --output-on-failure -R widget`
- **Per wave merge:** `cd build && ctest --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/test_widget_instance_context.cpp` -- add tests for colSpan/rowSpan properties and NOTIFY signals
- [ ] `tests/test_widget_grid_model.cpp` -- add test for resize below minCols/minRows being rejected
- [ ] No new test files needed -- extend existing test files

## Open Questions

1. **WidgetInstanceContext creation timing in HomeMenu.qml**
   - What we know: WidgetInstanceContext exists in C++ but is not currently instantiated per delegate in HomeMenu.qml. The delegate directly uses model roles.
   - What's unclear: Whether to create WidgetInstanceContext in C++ (via a factory on WidgetGridModel) or in QML (via `Qt.createQmlObject` or a registered QML type). The existing `WidgetHost.qml` component is NOT currently used in HomeMenu.qml's delegate -- it was likely the original component before the grid was built.
   - Recommendation: Register WidgetInstanceContext as a QML creatable type. HomeMenu.qml creates one per delegate, sets colSpan/rowSpan from model bindings, and sets provider references from a global host context reference. This avoids modifying WidgetGridModel's C++ interface.

2. **Whether WidgetHost.qml is still used**
   - What we know: `qml/components/WidgetHost.qml` exists as a standalone component with glass card + Loader + long-press. However, HomeMenu.qml's delegate (lines 300-340) duplicates this functionality inline.
   - What's unclear: Is WidgetHost.qml dead code or used elsewhere?
   - Recommendation: Check if WidgetHost.qml is imported anywhere. If dead code, either revive it as the canonical widget host or remove it. The inline delegate in HomeMenu.qml already handles everything.

3. **Contract documentation format**
   - What we know: CONTEXT.md says Claude's discretion on whether to create a WidgetContract.md or fold into plugin-api.md.
   - Recommendation: Create a standalone `docs/widget-contract.md`. The contract is substantial enough (size/layout, data/actions, lifecycle/performance, interaction model) to warrant its own document. Cross-link from plugin-api.md.

## Sources

### Primary (HIGH confidence)
- Source code analysis of WidgetGridModel.cpp (resize clamping at lines 170-202)
- Source code analysis of HomeMenu.qml (resize handle at lines 436-546, widget delegate at lines 224-340)
- Source code analysis of WidgetInstanceContext.hpp/cpp (existing provider injection)
- Source code analysis of all 6 widget QML files (breakpoints, data access patterns, pressAndHold forwarding)
- Phase 11 CONTEXT.md (locked decisions, all 4 contract areas)

### Secondary (MEDIUM confidence)
- Qt 6.8 QML Loader component scoping behavior (based on working knowledge of Qt QML, verified by existing `requestContextMenu()` pattern in codebase)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - no new dependencies, existing Qt 6.8 / C++17
- Architecture: HIGH - extending existing WidgetInstanceContext, well-understood QML patterns
- Pitfalls: HIGH - based on documented gotchas in CLAUDE.md and direct code analysis

**Research date:** 2026-03-14
**Valid until:** 2026-04-14 (stable -- internal refactoring, no external dependency risk)
