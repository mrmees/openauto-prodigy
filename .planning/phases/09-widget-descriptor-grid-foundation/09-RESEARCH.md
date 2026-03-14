# Phase 09: Widget Descriptor & Grid Foundation - Research

**Researched:** 2026-03-14
**Domain:** Qt 6 QML grid layout, DPI-aware physical sizing, YAML schema migration
**Confidence:** HIGH

## Summary

This phase enriches the widget descriptor metadata (category, description, icon) and replaces fixed-pixel grid math with DPI-based physical sizing. The existing codebase already has the right structure -- `DisplayInfo` computes grid dimensions, `WidgetGridModel` manages placements, `WidgetPickerModel` exposes descriptors -- so this is a targeted refactor of grid math plus additive metadata fields rather than an architectural overhaul.

The biggest technical risk is the grid migration algorithm: when saved grid dimensions differ from computed dimensions, placements must be proportionally remapped without data loss. The existing `setGridDimensions()` uses a simple clamp-and-hide approach that will be replaced with a proportional remap + page-spill strategy. This is the most algorithmically complex piece of the phase.

**Primary recommendation:** Use diagonal divisor 9 as the standard cell sizing constant, producing 7x4 grids on the 7" Pi target and consistently reasonable layouts across 800x480 through 1920x1080 displays. The pixel heuristic fallback (same divisor applied to window diagonal) produces identical results in windowed mode.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Canonical category IDs (lowercase, singular) + display labels, not free-form strings
- Built-in IDs: `status`, `media`, `navigation`, `launcher`; plugins use existing or register new
- Short tagline description format, 2-5 words max
- Grouped cards picker layout: category headers above horizontal card rows
- Square cells always -- single cellSide value
- DPI from QScreen physical dimensions, not window size
- DPI sanity range 80-300; outside = pixel heuristic fallback
- `display.screen_size` config is hard override for QScreen physical dims
- DPI priority cascade: config override -> QScreen -> pixel heuristic
- Windowed mode uses pixel heuristic fallback
- Both cols and rows derive freely from floor(usable / cellSide)
- Floor minimums: 3 cols / 2 rows
- Centered grid with even gutters (symmetric margins)
- Runtime recompute on resize
- `home.gridDensityBias` config: -1/0/+1 (roomy/standard/dense)
- DisplayInfo.cpp computes cellSide only (not cols/rows/offsets)
- HomeMenu.qml (or QML helper) computes grid frame (cols/rows/offsets from available rect + cellSide)
- WidgetGridModel.cpp owns logical placements in integer grid units only, no pixel awareness
- Boot sequence: model loads base placements from YAML, waits for QML to provide cols/rows, first remap produces live placements, grid invisible until first real layout
- Proportional position remap on dimension change; spans stay original size
- Shrink: clamp position, nudge overlaps, spill to next page if needed, mark hidden with warning if min span exceeds grid
- Store source grid dims (grid_cols/grid_rows) in YAML alongside placements
- Base snapshot is source of truth; remap re-derives from base, not mutated state
- Remap is runtime-only; persists only when user explicitly edits
- No migration code per version; saved dims ARE the version
- Remove 56px dock deduction now; dock overlaps grid via z-order
- No grid-side awareness of dock; Phase 10 will remove dock entirely

### Claude's Discretion
- Exact diagonal divisor constant for cell sizing
- DPI fallback heuristic formula for windowed/non-fullscreen mode
- Remap nudge algorithm for resolving overlaps
- Page spill strategy for shrink-remap overflow
- Picker card sizing, spacing, and scroll behavior
- Category sort order in picker
- gridDensityBias effect magnitude
- Whether QML grid frame logic lives in HomeMenu.qml or dedicated GridFrame helper

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| WF-01 | WidgetDescriptor includes category, description, and icon metadata fields | Add `category` and `description` fields to WidgetDescriptor struct; expose as WidgetPickerModel roles; update all registerWidget calls in main.cpp |
| GL-01 | Grid cell dimensions derived from physical mm targets via DPI instead of fixed pixel values | Rewrite DisplayInfo to compute cellSide via diagonal-proportional formula (divisor 9); QML grid frame computes cols/rows from cellSide + available rect |
| GL-02 | Grid math no longer deducts hardcoded dock height from available space | Remove `dockPx = 56` deduction from DisplayInfo::updateGridDimensions(); dock will overlap grid at z=10 |
| GL-03 | YAML widget layout includes grid_version field with migration when grid dimensions change | Add grid_cols/grid_rows to widget_grid YAML section; WidgetGridModel gets proportional remap algorithm; base snapshot preserved in YAML |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt 6.8 | 6.8.2 | QScreen API for physical DPI, QML grid layout | Already in use; QScreen exposes physicalDotsPerInch() |
| yaml-cpp | existing | YAML persistence for grid_cols/grid_rows | Already used for all config |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| QScreen | Qt 6.8 | Physical DPI via physicalDotsPerInch() or physicalSize() | DisplayInfo cell sizing |

### Alternatives Considered
None -- this phase modifies existing components only; no new libraries needed.

## Architecture Patterns

### Modified Project Structure
```
src/
  ui/
    DisplayInfo.cpp        # MODIFY: cellSide computation (remove gridColumns/gridRows)
    DisplayInfo.hpp        # MODIFY: cellSide Q_PROPERTY, remove grid dim properties
    WidgetGridModel.cpp    # MODIFY: proportional remap algorithm, base snapshot
    WidgetGridModel.hpp    # MODIFY: remap API, base placements storage
    WidgetPickerModel.cpp  # MODIFY: CategoryRole, DescriptionRole
    WidgetPickerModel.hpp  # MODIFY: new roles enum
  core/
    widget/WidgetTypes.hpp # MODIFY: add category, description to WidgetDescriptor
    YamlConfig.cpp         # MODIFY: grid_cols/grid_rows persistence
  main.cpp               # MODIFY: add category/description to registerWidget calls
qml/
  applications/home/
    HomeMenu.qml           # MODIFY: compute cols/rows/offsets from cellSide + available rect
  components/
    WidgetPicker.qml       # REWRITE: grouped-by-category card layout
```

### Pattern 1: Diagonal-Proportional Cell Sizing
**What:** cellSide = diag_px / divisor, where divisor = 9 (standard), adjusted by density bias
**When to use:** Any DPI-based grid sizing where cells should feel physically consistent

Analysis across target displays with divisor = 9:
| Display | DPI | cellSide | Cell mm | Grid |
|---------|-----|----------|---------|------|
| 5" 800x480 | 187 | 104px | 14.1mm | 7x4 |
| 7" 1024x600 (Pi) | 170 | 132px | 19.8mm | 7x4 |
| 10.1" 1920x1080 | 218 | 245px | 28.5mm | 7x4 |

Cell sizes range from 14-29mm across common displays -- all comfortably touch-tappable. The 7" Pi target gets ~20mm cells, which is larger than the current ~150px fixed cells and feels appropriate for a car touchscreen.

**DPI priority cascade implementation:**
```cpp
// In DisplayInfo.cpp
qreal DisplayInfo::computeCellSide() const
{
    qreal diagPx = std::hypot(windowWidth_, windowHeight_);
    qreal effectiveDivisor = kBaseDivisor + densityBias_ * kBiasStep;

    // Priority 1: config screen_size override
    if (configScreenSizeOverride_ > 0) {
        qreal dpi = diagPx / configScreenSizeOverride_;
        if (dpi >= 80.0 && dpi <= 300.0) {
            return diagPx / effectiveDivisor;
        }
    }

    // Priority 2: QScreen physical DPI (fullscreen only)
    if (isFullscreen_ && qscreenDpi_ >= 80.0 && qscreenDpi_ <= 300.0) {
        return diagPx / effectiveDivisor;
    }

    // Priority 3: Pixel heuristic fallback (windowed or bad DPI)
    return diagPx / effectiveDivisor;
}
```

Note: all three paths produce the same formula (`diagPx / divisor`) because the divisor is a universal proportion. DPI validation only matters for determining IF physical sizing is trustworthy -- but the diagonal-proportional approach makes the output identical regardless. The cascade matters for *future* mm-based targeting if we ever decouple from the diagonal proportion.

### Pattern 2: Proportional Grid Remap Algorithm
**What:** When saved grid dims differ from current, scale positions proportionally
**When to use:** Grid dimension changes (display change, density bias change, window resize + persist)

```cpp
// Pseudocode for remap
struct RemapResult {
    QList<GridPlacement> live;
    QStringList warnings;  // widgets that couldn't fit
};

RemapResult remapPlacements(
    const QList<GridPlacement>& base,  // from YAML (source of truth)
    int savedCols, int savedRows,       // dims when base was saved
    int newCols, int newRows,           // current computed dims
    WidgetRegistry* registry)
{
    RemapResult result;
    // Per-page occupancy tracking
    QHash<int, QVector<bool>> pageOccupancy;

    for (const auto& bp : base) {
        GridPlacement p = bp;
        p.visible = true;

        // 1. Proportional position scaling
        if (savedCols > 0 && savedRows > 0) {
            p.col = qRound(static_cast<qreal>(bp.col) * newCols / savedCols);
            p.row = qRound(static_cast<qreal>(bp.row) * newRows / savedRows);
        }

        // 2. Spans stay original size (do NOT scale)
        // 3. Clamp to fit within bounds
        p.col = qMin(p.col, qMax(0, newCols - p.colSpan));
        p.row = qMin(p.row, qMax(0, newRows - p.rowSpan));

        // 4. Check min span vs grid size
        auto desc = registry->descriptor(p.widgetId);
        if (desc && (desc->minCols > newCols || desc->minRows > newRows)) {
            p.visible = false;
            result.warnings.append(p.instanceId + ": min span exceeds grid");
            result.live.append(p);
            continue;
        }

        // 5. Check overlap, nudge if possible
        if (overlaps(p, pageOccupancy[p.page], newCols, newRows)) {
            if (!nudgeToFit(p, pageOccupancy[p.page], newCols, newRows)) {
                // 6. Spill to next page
                spillToNextPage(p, pageOccupancy, newCols, newRows);
            }
        }

        markOccupied(p, pageOccupancy[p.page], newCols, newRows);
        result.live.append(p);
    }
    return result;
}
```

### Pattern 3: Base Snapshot Architecture
**What:** YAML placements are the canonical layout; every remap derives from base, not live state
**When to use:** Any grid dimension change (resize, density bias, display change)

```
Boot:
  1. Load base placements + saved grid dims from YAML
  2. Wait for QML to provide current cols/rows (after layout)
  3. If dims differ: remap from base -> live placements
  4. If dims match: base = live (no remap)

Edit mode save:
  1. User moves/adds/removes widget -> live state changes
  2. On save: live state becomes new base snapshot
  3. Save base + current grid dims to YAML

Display change:
  1. New dims computed
  2. Re-remap from ORIGINAL base snapshot (not current live state)
  3. Prevents drift from repeated resize cycles
```

### Pattern 4: Category-Grouped Picker
**What:** WidgetPickerModel exposes CategoryRole/DescriptionRole; QML groups by category
**When to use:** Widget selection UI

Two approaches for grouping in QML:
1. **DelegateModel with groups** -- Qt's built-in grouping mechanism via `DelegateModel { groups: [...] }`
2. **Flat list with section headers** -- `ListView { section.property: "category" }` with section delegates

Recommendation: **ListView sections** -- simpler, well-documented, no DelegateModel complexity. Section headers are category labels, delegates are horizontal card rows within each section.

### Anti-Patterns to Avoid
- **Scaling spans during remap:** Spans represent widget logical size, not proportional position. A 2x2 widget should stay 2x2 on a bigger grid -- the cells themselves are bigger.
- **Auto-persisting remapped layouts:** Remap is a view concern. Only persist when user explicitly edits. Otherwise resize cycles accumulate rounding errors.
- **Using window rect for DPI in fullscreen:** Window may not cover the full panel. QScreen physical dimensions are panel-level truth.
- **Computing cols/rows in C++:** DisplayInfo doesn't know about navbar, page indicators, margins. Only QML knows the actual available rect.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| QML list grouping | Custom group model | ListView section.property | Built-in, handles dynamic data |
| DPI detection | Parse EDID manually | QScreen::physicalDotsPerInchX/Y() | Qt already wraps platform DPI |
| Proportional rounding | Simple integer division | qRound(float * newDim / oldDim) | Avoids systematic rounding bias |

## Common Pitfalls

### Pitfall 1: QScreen Returns 0 or Garbage Physical Size
**What goes wrong:** Many cheap car displays report 0mm physical size via EDID, making physicalDotsPerInch() return infinity or NaN.
**Why it happens:** Display manufacturers skip EDID physical size fields on budget panels.
**How to avoid:** The 80-300 DPI sanity range catches this. Values outside range trigger pixel heuristic fallback. Also check for NaN/Inf explicitly.
**Warning signs:** `QScreen::physicalSize()` returning `QSizeF(0, 0)`.

### Pitfall 2: Grid Invisible Until QML Layout Completes
**What goes wrong:** If QML sends cols/rows before its own layout is finalized (e.g., in Component.onCompleted), the available rect may be 0x0 or partial.
**Why it happens:** QML layout happens in stages; parent sizes aren't finalized during child onCompleted.
**How to avoid:** Use `onWidthChanged`/`onHeightChanged` handlers in HomeMenu.qml, guarded by `width > 0 && height > 0`. Don't compute grid in Component.onCompleted.
**Warning signs:** Grid showing 3x2 (minimum) then jumping to correct size.

### Pitfall 3: Remap Nudge Creates Infinite Loop
**What goes wrong:** Nudging widget A displaces widget B, which nudges C, which displaces A again.
**Why it happens:** Greedy nudge without ordering guarantees.
**How to avoid:** Process widgets in insertion order (first-placed wins). Later widgets get nudged/spilled, never earlier ones. Once a widget is placed, it doesn't move.
**Warning signs:** Remap taking >1ms or producing different results on repeated calls.

### Pitfall 4: Existing Tests Hardcode Current Grid Constants
**What goes wrong:** Tests like `test_grid_dimensions.cpp` and `test_display_info.cpp` assert specific column/row counts (e.g., 6x4 for 1024x600) based on the current 150px/125px cell targets.
**Why it happens:** Tests were written for the old grid math.
**How to avoid:** Update ALL test assertions to match the new diagonal-proportional formula. With divisor 9, 1024x600 produces 7x4, not 6x4.
**Warning signs:** Test failures on grid dimension values.

### Pitfall 5: Base Snapshot Persistence Race
**What goes wrong:** User edits widget in edit mode, resize event fires mid-edit, remap overwrites edit.
**Why it happens:** Remap uses base snapshot, which doesn't include in-progress edits.
**How to avoid:** Defer remap while edit mode is active. Queue the dimension change and apply remap when edit mode exits.
**Warning signs:** Widget jumps back to old position during drag/resize.

## Code Examples

### Adding Category/Description to WidgetDescriptor
```cpp
// In WidgetTypes.hpp
struct WidgetDescriptor {
    QString id;
    QString displayName;
    QString iconName;
    QString category;       // NEW: canonical category ID ("status", "media", etc.)
    QString description;    // NEW: short tagline ("Current time", "Connection status")
    QUrl qmlComponent;
    QString pluginId;
    DashboardContributionKind contributionKind = DashboardContributionKind::Widget;
    QVariantMap defaultConfig;
    int minCols = 1;
    int minRows = 1;
    int maxCols = 6;
    int maxRows = 4;
    int defaultCols = 1;
    int defaultRows = 1;
};
```

### Widget Registration with New Fields
```cpp
// In main.cpp
oap::WidgetDescriptor clockDesc;
clockDesc.id = "org.openauto.clock";
clockDesc.displayName = "Clock";
clockDesc.iconName = "\ue8b5";
clockDesc.category = "status";         // NEW
clockDesc.description = "Current time"; // NEW
clockDesc.minCols = 1; clockDesc.minRows = 1;
clockDesc.maxCols = 6; clockDesc.maxRows = 4;
clockDesc.defaultCols = 2; clockDesc.defaultRows = 2;
clockDesc.qmlComponent = QUrl("qrc:/OpenAutoProdigy/ClockWidget.qml");
widgetRegistry->registerWidget(clockDesc);
```

### DisplayInfo cellSide Property
```cpp
// In DisplayInfo.hpp
class DisplayInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(int windowWidth READ windowWidth NOTIFY windowSizeChanged)
    Q_PROPERTY(int windowHeight READ windowHeight NOTIFY windowSizeChanged)
    Q_PROPERTY(qreal screenSizeInches READ screenSizeInches NOTIFY screenSizeChanged)
    Q_PROPERTY(int computedDpi READ computedDpi NOTIFY windowSizeChanged)
    Q_PROPERTY(qreal cellSide READ cellSide NOTIFY cellSideChanged) // NEW: replaces grid dims

    // REMOVED: gridColumns, gridRows -- these move to QML

private:
    static constexpr qreal kBaseDivisor = 9.0;
    static constexpr qreal kBiasStep = 0.8; // divisor adjustment per bias level
    int densityBias_ = 0; // -1, 0, +1
};
```

### QML Grid Frame Computation
```qml
// In HomeMenu.qml (or a GridFrame helper)
Item {
    id: gridFrame

    // Available rect = parent minus margins (navbar/page indicators handled by Shell layout)
    readonly property real availW: width - 2 * UiMetrics.marginPage
    readonly property real availH: height - 2 * UiMetrics.marginPage

    // Cell side from DisplayInfo
    readonly property real cellSide: DisplayInfo ? DisplayInfo.cellSide : 120

    // Derive cols/rows from available space
    readonly property int gridCols: Math.max(3, Math.floor(availW / cellSide))
    readonly property int gridRows: Math.max(2, Math.floor(availH / cellSide))

    // Centered grid with symmetric gutters
    readonly property real gridW: gridCols * cellSide
    readonly property real gridH: gridRows * cellSide
    readonly property real offsetX: (availW - gridW) / 2 + UiMetrics.marginPage
    readonly property real offsetY: (availH - gridH) / 2 + UiMetrics.marginPage

    // Push dims to C++ model when they change
    onGridColsChanged: if (gridCols > 0) WidgetGridModel.setGridDimensions(gridCols, gridRows)
    onGridRowsChanged: if (gridRows > 0) WidgetGridModel.setGridDimensions(gridCols, gridRows)
}
```

### YAML Grid Dims Persistence
```cpp
// In YamlConfig.cpp
int YamlConfig::gridSavedCols() const {
    return root_["widget_grid"]["grid_cols"].as<int>(0); // 0 = never saved
}
int YamlConfig::gridSavedRows() const {
    return root_["widget_grid"]["grid_rows"].as<int>(0);
}
void YamlConfig::setGridSavedDims(int cols, int rows) {
    root_["widget_grid"]["grid_cols"] = cols;
    root_["widget_grid"]["grid_rows"] = rows;
}
```

### Category-Grouped Picker (QML ListView sections)
```qml
ListView {
    model: WidgetPickerModel
    section.property: "category"
    section.delegate: Item {
        width: ListView.view.width
        height: UiMetrics.fontBody + UiMetrics.spacing * 2
        NormalText {
            text: section  // category display label
            font.pixelSize: UiMetrics.fontBody
            color: ThemeService.onSurfaceVariant
            anchors.left: parent.left
            anchors.leftMargin: UiMetrics.spacing
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    delegate: Rectangle {
        // Widget card with icon, name, description
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Fixed 150x125 px cell targets | Diagonal-proportional square cells | This phase | Physically consistent across displays |
| 56px dock deduction | Dock overlaps grid at z=10 | This phase | Full vertical space for grid |
| Clamp-and-hide on resize | Proportional remap with page spill | This phase | No widget loss on display change |
| Flat picker strip | Category-grouped picker | This phase | Discoverable widget selection |

## Open Questions

1. **Density bias config key path**
   - What we know: `home.gridDensityBias` with values -1/0/+1
   - What's unclear: This key needs to be added to YamlConfig defaults and schema validation. New key path in `setValueByPath` validation.
   - Recommendation: Add to YamlConfig::initDefaults(), value 0. Validate range -1 to +1 in setter.

2. **WidgetPickerModel category display labels**
   - What we know: Category IDs are lowercase strings, display labels are human-readable
   - What's unclear: Where does the ID -> label mapping live? WidgetPickerModel could maintain a static map, or CategoryRole could return a struct.
   - Recommendation: Simple static map in WidgetPickerModel: `{"status" -> "Status", "media" -> "Media", "navigation" -> "Navigation", "launcher" -> "Launcher"}`. Unknown categories display with first letter capitalized. CategoryRole returns the display label string, not the ID (section.property needs the display label).

3. **Sort order within category groups**
   - What we know: Categories should appear in a defined order, not random
   - What's unclear: Alphabetical vs predefined priority?
   - Recommendation: Predefined priority order matching the built-in list: Status, Media, Navigation, Launcher, then alphabetical for plugin-registered categories. Widgets within a category sort alphabetically by displayName.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Qt Test (QTest) 6.8.2 |
| Config file | tests/CMakeLists.txt |
| Quick run command | `cd build && ctest -R 'test_display_info\|test_widget_grid_model\|test_grid_dimensions' --output-on-failure` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| WF-01 | WidgetDescriptor has category/description fields | unit | `ctest -R test_widget_types --output-on-failure` | Yes (needs new assertions) |
| WF-01 | WidgetPickerModel exposes CategoryRole/DescriptionRole | unit | `ctest -R test_widget_picker --output-on-failure` | No -- Wave 0 |
| GL-01 | DisplayInfo computes cellSide from diagonal/DPI | unit | `ctest -R test_display_info --output-on-failure` | Yes (needs rewrite) |
| GL-01 | Cell sizing produces correct grids across target displays | unit | `ctest -R test_grid_dimensions --output-on-failure` | Yes (needs rewrite) |
| GL-02 | No dock height deduction in grid math | unit | `ctest -R test_display_info --output-on-failure` | Yes (assertion update) |
| GL-03 | YAML stores/loads grid_cols/grid_rows | unit | `ctest -R test_yaml_config --output-on-failure` | Yes (needs new test cases) |
| GL-03 | Proportional remap preserves widgets on dimension change | unit | `ctest -R test_widget_grid_remap --output-on-failure` | No -- Wave 0 |
| GL-03 | Shrink remap spills to next page, doesn't delete | unit | `ctest -R test_widget_grid_remap --output-on-failure` | No -- Wave 0 |

### Sampling Rate
- **Per task commit:** `cd build && ctest -R 'test_display_info|test_widget_grid|test_grid_dim|test_widget_types|test_yaml_config' --output-on-failure`
- **Per wave merge:** `cd build && ctest --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/test_widget_picker_model.cpp` -- covers WF-01 (CategoryRole, DescriptionRole, category grouping)
- [ ] `tests/test_widget_grid_remap.cpp` -- covers GL-03 (proportional remap, nudge, page spill, base snapshot)
- [ ] Update `tests/test_display_info.cpp` -- rewrite assertions for cellSide instead of gridColumns/gridRows
- [ ] Update `tests/test_grid_dimensions.cpp` -- rewrite assertions for new divisor-based grid dims
- [ ] Update `tests/test_widget_grid_model.cpp` -- update any assertions depending on old grid dim defaults

## Discretion Recommendations

### Diagonal Divisor: 9
Tested across 5 display configurations. Divisor 9 produces:
- 7x4 on the 7" Pi target (132px cells, ~20mm physical)
- 7x4 on 800x480 (104px cells, ~14mm -- smaller but touchable)
- 7x4 on 1920x1080@10.1" (245px cells, ~29mm)
- Consistent 7-column layout across most common automotive displays
- The current 6x4 layout moves to 7x4, which is a moderate increase

### Density Bias Step: 0.8
Adjusts the divisor by +/-0.8:
- Roomy (bias -1): divisor 8.2 -> larger cells, fewer columns
- Standard (bias 0): divisor 9.0 -> default
- Dense (bias +1): divisor 9.8 -> smaller cells, more columns

On the Pi target: roomy=7x4 (145px), standard=7x4 (132px), dense=8x4 (121px). The jump from 7 to 8 columns happens at the dense setting.

### Pixel Heuristic Fallback: Same Formula
Since diagonal-proportional sizing doesn't actually need DPI (the divisor is a proportion of diagonal pixels), the "fallback" formula is identical: `diagPx / (9 + bias * 0.8)`. The DPI cascade is about *validation* (is the physical data trustworthy?), not about producing different output.

### Nudge Algorithm: Spiral Search
When proportional remap creates an overlap:
1. Try positions in a spiral outward from the target position (right, down-right, down, down-left, left, up-left, up, up-right)
2. First non-overlapping position within grid bounds wins
3. If no position found on current page, spill to next page at position (0,0)
4. Auto-create page if needed during spill

### Category Sort: Predefined + Alpha
Built-in categories in fixed order: Status (0), Media (1), Navigation (2), Launcher (3). Plugin-registered categories after built-ins, sorted alphabetically. "Other" (uncategorized) always last.

### Grid Frame: Inline in HomeMenu.qml
The grid frame computation (cols/rows/offsets from cellSide + available rect) is ~15 lines of property bindings. A dedicated GridFrame.qml component is overkill for this volume. Keep it inline in HomeMenu.qml with clear comments. Extract later if it grows.

## Sources

### Primary (HIGH confidence)
- Existing codebase: DisplayInfo.cpp, WidgetGridModel.cpp, WidgetPickerModel.cpp, YamlConfig.cpp
- Existing tests: test_display_info.cpp, test_grid_dimensions.cpp, test_widget_grid_model.cpp
- CONTEXT.md decisions from /gsd:discuss-phase

### Secondary (MEDIUM confidence)
- Qt 6.8 QScreen API (physicalDotsPerInchX/Y, physicalSize) -- verified via Qt documentation
- Display sizing analysis: computed across 5 target display configurations

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new libraries, existing codebase fully understood
- Architecture: HIGH -- decisions locked in CONTEXT.md, code locations identified
- Pitfalls: HIGH -- based on direct codebase analysis and existing test review
- Cell sizing constants: HIGH -- empirically verified across target displays

**Research date:** 2026-03-14
**Valid until:** 2026-04-14 (stable -- no external dependencies changing)
