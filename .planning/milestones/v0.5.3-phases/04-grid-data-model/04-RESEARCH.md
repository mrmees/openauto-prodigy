# Phase 04: Grid Data Model & Persistence - Research

**Researched:** 2026-03-12
**Domain:** Qt 6 C++ data models, grid occupancy logic, YAML persistence
**Confidence:** HIGH

## Summary

This phase replaces the pane-based `WidgetPlacementModel` (3 fixed slots: main/sub1/sub2) with a cell-based `WidgetGridModel` that tracks placements as (col, row, colSpan, rowSpan) with occupancy tracking and collision detection. The existing codebase has clean separation between the placement model, widget registry, and YAML config, making this a surgical replacement rather than a rewrite.

The key technical challenges are: (1) deriving grid dimensions from display size in a way that works across resolutions, (2) implementing occupancy tracking with efficient collision detection, (3) extending `WidgetDescriptor` from a binary Main/Sub enum to flexible min/max/default column/row ranges, and (4) YAML schema migration with clean defaults for the grid format.

**Primary recommendation:** Build `WidgetGridModel` as a `QAbstractListModel` (like the existing widget models) with an internal 2D occupancy bitmap. Derive grid dimensions in C++ `DisplayInfo` (not QML-only) so both C++ model logic and QML rendering can access them. Keep the auto-save-on-change pattern from the existing `WidgetPlacementModel`.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **6x4 grid** for 1024x600 display; grid density scales with display size via UiMetrics
- Launcher dock **floats below** grid area (not consuming grid rows)
- **Visible gaps** between grid cells (using UiMetrics.gridGap)
- Config migration **dropped entirely** -- no current users, ignore old pane config and write fresh grid defaults
- GRID-07 descoped from this milestone
- Resolution portability: **clamp to edges**, overlap conflicts resolved by **first-placed wins** (late-comers hidden, kept in config)
- Widget size constraints: **flexible min/max ranges** (minCols, minRows, maxCols, maxRows) + defaultCols/defaultRows
- Full-width spanning allowed (widget can span all 6 columns)
- **Both grid-cell and widget-level backgrounds** -- grid cell provides glass card (WidgetHost), widgets can render internal backgrounds; per-cell opacity adjustable (can be 0%)
- **Pre-register** nav turn-by-turn and unified now playing widget descriptor stubs (IDs, names, icons, size constraints; QML component URL empty/placeholder)

### Claude's Discretion
- Exact grid density formula (how UiMetrics derives columns/rows from display dimensions)
- Occupancy tracking algorithm implementation
- YAML config schema structure for grid placements
- Default widget layout arrangement for fresh installs
- WidgetPickerModel changes (filter by grid space available vs old size enum)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| GRID-01 | Home screen uses a cell-based grid for widget placement (replaces fixed 3-pane layout) | WidgetGridModel replaces WidgetPlacementModel; QAbstractListModel with grid coordinate roles |
| GRID-02 | Grid density (columns/rows) is auto-derived from display size via UiMetrics | DisplayInfo gains gridColumns/gridRows Q_PROPERTYs; formula in Architecture Patterns |
| GRID-04 | Widgets declare min/max grid spans (replaces Main/Sub size enum) | WidgetDescriptor struct gains 6 int fields, WidgetSize enum removed |
| GRID-05 | Grid layout (positions, sizes, opacity) persists in YAML config across restarts | New `widget_grid` YAML schema replaces `widget_config`; auto-save on placementsChanged |
| GRID-07 | Existing 3-pane config auto-migrates to grid coordinates on first launch | DESCOPED per user decision -- detect old config, discard, write fresh defaults |
| GRID-08 | Widget layouts survive resolution changes -- widgets clamped/reflowed to valid positions | Clamping logic in WidgetGridModel::setGridDimensions(); first-placed-wins for overlaps |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Qt 6 (Core, Gui, Quick) | 6.4 (dev) / 6.8 (Pi) | QAbstractListModel, Q_PROPERTY, signals/slots | Already the project stack |
| yaml-cpp | system | YAML config read/write | Already used for all config persistence |
| QtTest | 6.4+ | Unit tests | Already used for all 71 existing tests |

### Supporting
No new libraries needed. This phase is pure Qt model/data logic with existing yaml-cpp persistence.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| 2D occupancy vector | QSet<QPair<int,int>> for occupied cells | Vector is O(1) lookup, QSet has overhead; vector wins for small grids |
| C++ grid dimensions | QML-only computation | C++ model needs dimensions for clamping; must be in C++ |

## Architecture Patterns

### Grid Density Formula

For the user's discretion on deriving grid dimensions from display size:

```
Target cell size: ~170px wide, ~150px tall (based on 1024x600 -> 6x4)
Subtract margins: usable = windowSize - 2*marginPage - dockHeight(bottom)
columns = floor(usableWidth / targetCellWidth)
rows = floor(usableHeight / targetCellHeight)
Clamp: columns in [3, 8], rows in [2, 6]
```

Verification against known displays:
- 800x480: ~4.5 -> 4 cols, ~2.8 -> 2 rows (tight but usable, or 5x3 with smaller cells)
- 1024x600: ~5.7 -> 6 cols, ~3.6 -> 4 rows (matches decision)
- 1280x720: ~7.1 -> 7 cols, ~4.4 -> 4 rows
- 1920x1080: ~10.8 -> 8 cols (clamped), ~6.8 -> 6 cols (clamped)

The user mentioned 800x480 -> 5x3 specifically. Adjusting target cell width to ~155px and target cell height to ~145px:
- 800x480: usable ~760x~400 -> 4.9 -> 5 cols, 2.8 -> 3 rows. Matches.
- 1024x600: usable ~984x~520 -> 6.3 -> 6 cols, 3.6 -> 4 rows. Matches.

**Recommendation:** Use target cell dimensions of ~155x145px as the divisor, with clamps [3,8] for columns and [2,6] for rows. Compute in C++ `DisplayInfo` so both the model and QML can use it. The dock height should be subtracted before computing rows.

### Recommended Changes by File

```
src/core/widget/WidgetTypes.hpp     -- Replace WidgetSize enum with grid span fields
src/core/widget/WidgetRegistry.hpp  -- Replace widgetsForSize() with widgetsFittingSpace()
src/ui/WidgetGridModel.hpp/cpp      -- NEW: replaces WidgetPlacementModel
src/ui/WidgetPickerModel.hpp/cpp    -- Update filter from size enum to available space
src/ui/WidgetInstanceContext.hpp    -- Replace paneSize with cellWidth/cellHeight
src/ui/DisplayInfo.hpp/cpp          -- Add gridColumns/gridRows Q_PROPERTYs
src/core/YamlConfig.hpp/cpp         -- New widget_grid schema, bump config version
qml/controls/UiMetrics.qml          -- Expose gridColumns/gridRows from DisplayInfo
src/main.cpp                        -- Update widget registrations, add Phase 06 stubs
```

### Pattern 1: WidgetGridModel as QAbstractListModel

**What:** A list model where each row represents one placed widget with grid coordinates. Exposes roles for QML Repeater consumption (Phase 05 will consume it).
**When to use:** This phase builds the model; Phase 05 wires it to QML rendering.

```cpp
// WidgetGridModel.hpp
class WidgetGridModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int gridColumns READ gridColumns NOTIFY gridDimensionsChanged)
    Q_PROPERTY(int gridRows READ gridRows NOTIFY gridDimensionsChanged)

public:
    enum Roles {
        WidgetIdRole = Qt::UserRole + 1,
        InstanceIdRole,
        ColumnRole,
        RowRole,
        ColSpanRole,
        RowSpanRole,
        OpacityRole,
        QmlComponentRole
    };

    // Core grid operations
    Q_INVOKABLE bool placeWidget(const QString& widgetId, int col, int row,
                                  int colSpan, int rowSpan);
    Q_INVOKABLE bool moveWidget(const QString& instanceId, int newCol, int newRow);
    Q_INVOKABLE bool resizeWidget(const QString& instanceId, int newColSpan, int newRowSpan);
    Q_INVOKABLE void removeWidget(const QString& instanceId);
    Q_INVOKABLE void setWidgetOpacity(const QString& instanceId, double opacity);
    Q_INVOKABLE bool canPlace(int col, int row, int colSpan, int rowSpan,
                               const QString& excludeInstanceId = {}) const;

    // Grid dimensions (from DisplayInfo)
    void setGridDimensions(int cols, int rows); // triggers clamping/reflow
    int gridColumns() const;
    int gridRows() const;

signals:
    void gridDimensionsChanged();
    void placementsChanged(); // for auto-save wiring
};
```

### Pattern 2: Occupancy Bitmap

**What:** A flat `QVector<bool>` of size `cols * rows` for O(1) cell occupancy checks.
**When to use:** Every placement, move, resize, and collision check.

```cpp
// Internal to WidgetGridModel
class OccupancyGrid {
public:
    void resize(int cols, int rows);
    bool isOccupied(int col, int row) const;
    bool regionFree(int col, int row, int colSpan, int rowSpan,
                     const QString& excludeInstance = {}) const;
    void markOccupied(int col, int row, int colSpan, int rowSpan,
                       const QString& instanceId);
    void clearRegion(int col, int row, int colSpan, int rowSpan);
    void rebuild(const QList<GridPlacement>& placements); // full rebuild after resize

private:
    int cols_ = 0, rows_ = 0;
    QVector<QString> cells_; // empty string = free, else instanceId
    // Using QString instead of bool allows "who occupies this cell?" queries
};
```

**Key insight:** Storing the instanceId (not just bool) in each cell allows the occupancy grid to answer "what widget is at (col, row)?" which is needed for collision resolution during clamping.

### Pattern 3: GridPlacement Struct (replaces WidgetPlacement)

```cpp
struct GridPlacement {
    QString instanceId;     // unique per placement
    QString widgetId;       // "org.openauto.clock"
    int col = 0;
    int row = 0;
    int colSpan = 1;
    int rowSpan = 1;
    double opacity = 0.25;  // glass card opacity
    bool visible = true;    // false when clamped out (kept in config)

    // For YAML round-trip only (not used in model logic)
    QVariantMap extraConfig;
};
```

### Pattern 4: Resolution Change Clamping

**What:** When grid dimensions change, widgets that no longer fit must be adjusted.
**Algorithm:**
1. Sort placements by original insertion order (preserved in list order)
2. Clear occupancy grid
3. For each placement:
   a. Clamp colSpan/rowSpan to fit within new grid dimensions and widget's max constraints
   b. Clamp col/row so widget stays within bounds: `col = min(col, gridCols - colSpan)`
   c. If clamped position overlaps an already-placed widget, mark `visible = false`
   d. Otherwise, place it and mark cells as occupied
4. Emit `placementsChanged()` to trigger save

### Pattern 5: YAML Config Schema

```yaml
widget_grid:
  version: 2  # v1 was pane-based, v2 is grid-based
  placements:
    - instance_id: "clock-0"
      widget_id: "org.openauto.clock"
      col: 0
      row: 0
      col_span: 2
      row_span: 2
      opacity: 0.25
    - instance_id: "aa-status-1"
      widget_id: "org.openauto.aa-status"
      col: 2
      row: 0
      col_span: 2
      row_span: 1
      opacity: 0.25
    - instance_id: "now-playing-2"
      widget_id: "org.openauto.bt-now-playing"
      col: 2
      row: 1
      col_span: 3
      row_span: 2
      opacity: 0.25
```

### Pattern 6: WidgetDescriptor with Grid Spans

```cpp
struct WidgetDescriptor {
    QString id;
    QString displayName;
    QString iconName;          // Material icon codepoint
    QUrl qmlComponent;         // qrc URL to widget QML (empty for stubs)
    QString pluginId;          // empty for standalone widgets

    // Grid size constraints (replaces WidgetSizeFlags)
    int minCols = 1;
    int minRows = 1;
    int maxCols = 6;           // full width on 6-col grid
    int maxRows = 4;           // full height on 4-row grid
    int defaultCols = 1;
    int defaultRows = 1;

    QVariantMap defaultConfig;
};
```

### Pattern 7: Default Layout for Fresh Install

For a 6x4 grid:
```
+------+------+------+------+------+------+
|             |                            |
|    Clock    |       Now Playing          |
|     2x2     |          3x2              |
|             |                            |
+------+------+------+------+------+------+
|  AA Status  |                            |
|     2x1     |       (empty)              |
+------+------+------+------+------+------+
|                                          |
|                (empty)                   |
+------+------+------+------+------+------+
```

Placements: Clock at (0,0, 2x2), Now Playing at (2,0, 3x2), AA Status at (0,2, 2x1). Leaves space for future widgets in Phase 06.

### Anti-Patterns to Avoid
- **Don't derive grid dimensions in QML only** -- the C++ model needs dimensions for clamping logic and validation. Compute in DisplayInfo, expose to QML via UiMetrics.
- **Don't use QAbstractTableModel** -- despite being a grid, the model is consumed by QML Repeater (Phase 05) which works with list models. A table model adds complexity without benefit since QML doesn't use row/column indices from the model.
- **Don't store page assignments yet** -- multi-page (Phase 08) is separate. Keep placements in a flat list with no pageId for now. Add it later.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| YAML serialization | Custom JSON/binary format | yaml-cpp (already in use) | Consistency with existing config, human-readable |
| Observable model for QML | Manual signal wiring | QAbstractListModel | Qt's data binding expects this interface |
| Config defaults | Manual "if empty" checks everywhere | YamlConfig::initDefaults() pattern | Established pattern, catches all missing keys |

## Common Pitfalls

### Pitfall 1: Config defaults gate
**What goes wrong:** New YAML keys return garbage if not in initDefaults()
**Why it happens:** `YAML::Node::as<T>()` throws on missing keys without defaults
**How to avoid:** Add every new config key to `YamlConfig::initDefaults()` and to `buildDefaultsNode()`
**Warning signs:** Crashes on fresh install (no existing config file)

### Pitfall 2: QAbstractListModel role changes need full reset
**What goes wrong:** Adding/removing items works, but changing what data a role returns requires `dataChanged` signal with correct roles vector
**Why it happens:** QML caches role data aggressively
**How to avoid:** Emit `dataChanged(index, index, {role})` for single-property changes. Use `beginResetModel/endResetModel` for grid dimension changes that affect all items.

### Pitfall 3: Occupancy grid desync
**What goes wrong:** Occupancy bitmap says cell is free but a widget is actually there (or vice versa)
**Why it happens:** Forgetting to update occupancy on every mutation (place, move, resize, remove)
**How to avoid:** Make occupancy updates atomic -- every mutation calls `rebuildOccupancy()` or carefully paired `clearRegion/markOccupied`. Tests should verify occupancy state after every operation.

### Pitfall 4: Grid dimension change during AA fullscreen
**What goes wrong:** If display resolution were to change while AA is active, clamping runs on placements
**Why it happens:** Unlikely but theoretically possible (HDMI hotplug)
**How to avoid:** Grid dimensions should only recompute on app startup or explicit settings change, not on every window resize event. Debounce or ignore during AA session.

### Pitfall 5: Instance ID uniqueness
**What goes wrong:** Duplicate instanceIds cause occupancy corruption
**Why it happens:** User places same widget type twice, naive ID generation uses widgetId as instanceId
**How to avoid:** Generate instanceIds with a monotonic counter: `widgetId + "-" + nextInstanceId_++`. Store counter in config or derive from max existing ID.

### Pitfall 6: WidgetSize enum removal breaks existing tests
**What goes wrong:** 6 existing test files reference WidgetSize::Main/Sub
**Why it happens:** WidgetDescriptor struct change is not backward compatible
**How to avoid:** Update all 6 test files in the same commit as the struct change. The compiler will catch all references.

## Code Examples

### Widget Registration with Grid Spans (main.cpp)

```cpp
// Built-in clock widget
oap::WidgetDescriptor clockDesc;
clockDesc.id = "org.openauto.clock";
clockDesc.displayName = "Clock";
clockDesc.iconName = "\ue8b5";  // schedule
clockDesc.minCols = 1; clockDesc.minRows = 1;
clockDesc.maxCols = 6; clockDesc.maxRows = 4;
clockDesc.defaultCols = 2; clockDesc.defaultRows = 2;
clockDesc.qmlComponent = QUrl("qrc:/OpenAutoProdigy/ClockWidget.qml");
widgetRegistry->registerWidget(clockDesc);

// Phase 06 stub: nav turn-by-turn
oap::WidgetDescriptor navDesc;
navDesc.id = "org.openauto.nav-turn";
navDesc.displayName = "Navigation";
navDesc.iconName = "\ue55c";  // navigation
navDesc.minCols = 2; navDesc.minRows = 1;
navDesc.maxCols = 4; navDesc.maxRows = 2;
navDesc.defaultCols = 3; navDesc.defaultRows = 1;
// qmlComponent intentionally empty -- Phase 06 fills it
widgetRegistry->registerWidget(navDesc);
```

### YAML Round-Trip (YamlConfig)

```cpp
QList<GridPlacement> YamlConfig::gridPlacements() const {
    QList<GridPlacement> result;
    auto placements = root_["widget_grid"]["placements"];
    if (placements.IsSequence()) {
        for (const auto& node : placements) {
            GridPlacement p;
            p.instanceId = QString::fromStdString(
                node["instance_id"].as<std::string>(""));
            p.widgetId = QString::fromStdString(
                node["widget_id"].as<std::string>(""));
            p.col = node["col"].as<int>(0);
            p.row = node["row"].as<int>(0);
            p.colSpan = node["col_span"].as<int>(1);
            p.rowSpan = node["row_span"].as<int>(1);
            p.opacity = node["opacity"].as<double>(0.25);
            result.append(p);
        }
    }
    return result;
}
```

### Grid Density Computation (DisplayInfo)

```cpp
void DisplayInfo::updateGridDimensions() {
    // Subtract margins (scaled ~20px each side) and dock (~56px)
    // Using raw reference values; actual scaling happens in QML
    const int marginPx = 40;  // 2 * 20px reference margin
    const int dockPx = 56;    // reference dock height
    int usableW = windowWidth_ - marginPx;
    int usableH = windowHeight_ - marginPx - dockPx;

    const int targetCellW = 155;
    const int targetCellH = 145;

    gridColumns_ = qBound(3, usableW / targetCellW, 8);
    gridRows_ = qBound(2, usableH / targetCellH, 6);
    emit gridDimensionsChanged();
}
```

### WidgetPickerModel Filter by Available Space

```cpp
void WidgetPickerModel::filterByAvailableSpace(int availCols, int availRows) {
    beginResetModel();
    filtered_.clear();
    for (const auto& desc : registry_->allDescriptors()) {
        if (desc.qmlComponent.isEmpty()) continue; // skip stubs
        if (desc.minCols <= availCols && desc.minRows <= availRows) {
            filtered_.append(desc);
        }
    }
    endResetModel();
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Fixed 3-pane layout (main/sub1/sub2) | Cell-based grid (col/row/span) | This phase | Arbitrary widget count and sizes |
| WidgetSize enum (Main/Sub binary) | minCols/minRows/maxCols/maxRows/defaultCols/defaultRows | This phase | Continuous size range |
| Pane-string keys ("main", "sub1") | Grid coordinate keys (col, row) | This phase | Position-independent placement |
| WidgetPlacementModel (QObject) | WidgetGridModel (QAbstractListModel) | This phase | QML Repeater-compatible for Phase 05 |

**Deprecated/outdated:**
- `WidgetSize` enum: Removed entirely, replaced by integer span constraints
- `WidgetPlacementModel`: Replaced by `WidgetGridModel`
- `PageDescriptor.layoutTemplate`: No longer relevant (grid is the layout)
- `WidgetPlacement.paneId`: Replaced by col/row/colSpan/rowSpan
- `widgetsForSize()`: Replaced by space-based filtering

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | QtTest (Qt 6.4+) |
| Config file | tests/CMakeLists.txt (oap_add_test helper) |
| Quick run command | `cd build && ctest -R test_widget_grid -j4 --output-on-failure` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| GRID-01 | WidgetGridModel stores placements as (col, row, colSpan, rowSpan) | unit | `ctest -R test_widget_grid_model --output-on-failure` | Wave 0 |
| GRID-01 | Occupancy tracking detects collisions | unit | `ctest -R test_widget_grid_model --output-on-failure` | Wave 0 |
| GRID-02 | Grid dimensions derived from display size | unit | `ctest -R test_grid_dimensions --output-on-failure` | Wave 0 |
| GRID-04 | WidgetDescriptor has min/max/default cols/rows | unit | `ctest -R test_widget_types --output-on-failure` | Needs update |
| GRID-05 | Grid state round-trips through YAML | unit | `ctest -R test_yaml_config --output-on-failure` | Needs update |
| GRID-08 | Clamping/reflow on dimension change | unit | `ctest -R test_widget_grid_model --output-on-failure` | Wave 0 |

### Sampling Rate
- **Per task commit:** `cd build && cmake --build . -j$(nproc) && ctest -R "test_widget_grid|test_yaml_config|test_widget" --output-on-failure`
- **Per wave merge:** `cd build && cmake --build . -j$(nproc) && ctest --output-on-failure`
- **Phase gate:** Full suite green (71+ tests) before verify

### Wave 0 Gaps
- [ ] `tests/test_widget_grid_model.cpp` -- covers GRID-01, GRID-08 (placement, occupancy, collision, clamping)
- [ ] `tests/test_grid_dimensions.cpp` -- covers GRID-02 (formula correctness for known display sizes)
- [ ] Update `tests/test_widget_types.cpp` -- remove WidgetSize enum tests, add span constraint tests
- [ ] Update `tests/test_widget_registry.cpp` -- replace widgetsForSize tests with space-based filtering
- [ ] Update `tests/test_widget_placement_model.cpp` -- either remove or rename to test new grid model
- [ ] Update `tests/test_widget_config.cpp` -- update for new YAML schema
- [ ] Update `tests/test_widget_instance_context.cpp` -- replace paneSize with grid dimensions

## Open Questions

1. **Dock height awareness in grid density formula**
   - What we know: The dock floats below the grid. Grid rows should not count dock space.
   - What's unclear: Exact dock height varies with UiMetrics.scale. Should the formula use reference (unscaled) values or actual pixel values?
   - Recommendation: Use unscaled reference values (56px dock, 20px margins) since the grid density is a count (integer), not pixel layout. The pixel layout of cells is QML's job (Phase 05).

2. **Instance ID generation strategy**
   - What we know: Must be unique per placement, stable across saves
   - What's unclear: Counter-based (monotonic) vs UUID vs widgetId+position-hash
   - Recommendation: Monotonic counter stored in config (`widget_grid.next_instance_id`). Simple, predictable, no collisions.

3. **Widget stubs for Phase 06 -- what icon codepoints?**
   - What we know: Nav turn widget and unified now playing widget need descriptor stubs
   - Recommendation: `\ue55c` (navigation) for nav turn, `\ue030` (music_note) for unified now playing. Both already in Material Symbols.

## Sources

### Primary (HIGH confidence)
- Project source code: WidgetPlacementModel, WidgetTypes, WidgetRegistry, YamlConfig, UiMetrics, DisplayInfo, HomeMenu.qml, WidgetHost.qml
- Project CLAUDE.md, CONTEXT.md, REQUIREMENTS.md

### Secondary (MEDIUM confidence)
- Qt 6 QAbstractListModel documentation (known from training data, stable API since Qt 5)
- yaml-cpp API (stable, already in project use)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new libraries, pure in-tree changes
- Architecture: HIGH -- patterns derived from existing codebase conventions
- Pitfalls: HIGH -- identified from existing gotchas in CLAUDE.md and code review
- Grid density formula: MEDIUM -- needs Pi touch validation (noted in STATE.md blockers)

**Research date:** 2026-03-12
**Valid until:** 2026-04-12 (stable domain, no external dependencies)
