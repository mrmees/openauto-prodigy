#include "ui/WidgetGridModel.hpp"
#include "core/widget/WidgetRegistry.hpp"
#include <algorithm>
#include <cmath>
#include <QDebug>

namespace oap {

WidgetGridModel::WidgetGridModel(WidgetRegistry* registry, QObject* parent)
    : QAbstractListModel(parent), registry_(registry)
{
}

int WidgetGridModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return livePlacements_.size();
}

QVariant WidgetGridModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= livePlacements_.size())
        return {};

    const auto& p = livePlacements_[index.row()];
    switch (role) {
    case WidgetIdRole:      return p.widgetId;
    case InstanceIdRole:    return p.instanceId;
    case ColumnRole:        return p.col;
    case RowRole:           return p.row;
    case ColSpanRole:       return p.colSpan;
    case RowSpanRole:       return p.rowSpan;
    case OpacityRole:       return p.opacity;
    case VisibleRole:       return p.visible;
    case PageRole:          return p.page;
    case SingletonRole: {
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc) return desc->singleton;
        }
        return false;
    }
    case ConfigRole:
        return p.config;
    case HasConfigSchemaRole: {
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc) return !desc->configSchema.isEmpty();
        }
        return false;
    }
    case DisplayNameRole: {
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc) return desc->displayName;
        }
        return {};
    }
    case IconNameRole: {
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc) return desc->iconName;
        }
        return {};
    }
    case QmlComponentRole: {
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc) return desc->qmlComponent;
        }
        return {};
    }
    case MinColsRole: {
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc) return desc->minCols;
        }
        return 1;
    }
    case MinRowsRole: {
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc) return desc->minRows;
        }
        return 1;
    }
    case MaxColsRole: {
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc) return desc->maxCols;
        }
        return 6;
    }
    case MaxRowsRole: {
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc) return desc->maxRows;
        }
        return 4;
    }
    case DefaultColsRole: {
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc) return desc->defaultCols;
        }
        return 1;
    }
    case DefaultRowsRole: {
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc) return desc->defaultRows;
        }
        return 1;
    }
    default: return {};
    }
}

QHash<int, QByteArray> WidgetGridModel::roleNames() const
{
    return {
        {WidgetIdRole, "widgetId"},
        {InstanceIdRole, "instanceId"},
        {ColumnRole, "column"},
        {RowRole, "row"},
        {ColSpanRole, "colSpan"},
        {RowSpanRole, "rowSpan"},
        {OpacityRole, "opacity"},
        {QmlComponentRole, "qmlComponent"},
        {VisibleRole, "visible"},
        {MinColsRole, "minCols"},
        {MinRowsRole, "minRows"},
        {MaxColsRole, "maxCols"},
        {MaxRowsRole, "maxRows"},
        {DefaultColsRole, "defaultCols"},
        {DefaultRowsRole, "defaultRows"},
        {PageRole, "page"},
        {SingletonRole, "isSingleton"},
        {ConfigRole, "instanceConfig"},
        {HasConfigSchemaRole, "hasConfigSchema"},
        {DisplayNameRole, "displayName"},
        {IconNameRole, "iconName"}
    };
}

bool WidgetGridModel::placeWidget(const QString& widgetId, int col, int row,
                                   int colSpan, int rowSpan)
{
    if (!canPlace(col, row, colSpan, rowSpan))
        return false;

    QString instanceId = widgetId + "-" + QString::number(nextInstanceId_++);

    GridPlacement p;
    p.instanceId = instanceId;
    p.widgetId = widgetId;
    p.col = col;
    p.row = row;
    p.colSpan = colSpan;
    p.rowSpan = rowSpan;
    p.opacity = 0.25;
    p.page = activePage_;
    p.visible = true;

    beginInsertRows(QModelIndex(), livePlacements_.size(), livePlacements_.size());
    livePlacements_.append(p);
    markOccupied(p);
    endInsertRows();

    promoteToBase();
    emit placementsChanged();
    return true;
}

bool WidgetGridModel::moveWidget(const QString& instanceId, int newCol, int newRow)
{
    int idx = findPlacement(instanceId);
    if (idx < 0) return false;

    const auto& p = livePlacements_[idx];
    // Use placement's own page for collision check
    if (!canPlaceOnPage(newCol, newRow, p.colSpan, p.rowSpan, p.page, instanceId))
        return false;

    livePlacements_[idx].col = newCol;
    livePlacements_[idx].row = newRow;
    rebuildOccupancy();

    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, {ColumnRole, RowRole});

    promoteToBase();
    emit placementsChanged();
    return true;
}

bool WidgetGridModel::resizeWidget(const QString& instanceId, int newColSpan, int newRowSpan)
{
    int idx = findPlacement(instanceId);
    if (idx < 0) return false;

    const auto& p = livePlacements_[idx];

    // Check against widget's min/max constraints
    if (registry_) {
        auto desc = registry_->descriptor(p.widgetId);
        if (desc) {
            if (newColSpan < desc->minCols || newColSpan > desc->maxCols)
                return false;
            if (newRowSpan < desc->minRows || newRowSpan > desc->maxRows)
                return false;
        }
    }

    // Use placement's own page for collision check
    if (!canPlaceOnPage(p.col, p.row, newColSpan, newRowSpan, p.page, instanceId))
        return false;

    livePlacements_[idx].colSpan = newColSpan;
    livePlacements_[idx].rowSpan = newRowSpan;
    rebuildOccupancy();

    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, {ColSpanRole, RowSpanRole});

    promoteToBase();
    emit placementsChanged();
    return true;
}

void WidgetGridModel::removeWidget(const QString& instanceId)
{
    int idx = findPlacement(instanceId);
    if (idx < 0) return;

    // Singleton widgets cannot be removed
    if (registry_) {
        auto desc = registry_->descriptor(livePlacements_[idx].widgetId);
        if (desc && desc->singleton) return;
    }

    beginRemoveRows(QModelIndex(), idx, idx);
    livePlacements_.removeAt(idx);
    endRemoveRows();

    rebuildOccupancy();
    promoteToBase();
    emit placementsChanged();
}

void WidgetGridModel::setWidgetOpacity(const QString& instanceId, double opacity)
{
    int idx = findPlacement(instanceId);
    if (idx < 0) return;

    livePlacements_[idx].opacity = qBound(0.0, opacity, 1.0);
    promoteToBase();
    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, {OpacityRole});
    emit placementsChanged();
}

// --- Config methods ---

QVariantMap WidgetGridModel::validateConfig(const QString& widgetId, const QVariantMap& raw) const
{
    if (!registry_) return {};
    auto desc = registry_->descriptor(widgetId);
    if (!desc || desc->configSchema.isEmpty()) return {};

    // Build schema key lookup
    QHash<QString, const ConfigSchemaField*> schemaMap;
    for (const auto& field : desc->configSchema)
        schemaMap[field.key] = &field;

    QVariantMap validated;
    for (auto it = raw.constBegin(); it != raw.constEnd(); ++it) {
        auto* field = schemaMap.value(it.key(), nullptr);
        if (!field) continue;  // reject unknown keys

        switch (field->type) {
        case ConfigFieldType::Bool: {
            QVariant v = it.value();
            if (v.typeId() == QMetaType::Bool) {
                validated[it.key()] = v.toBool();
            } else if (v.typeId() == QMetaType::QString) {
                const QString s = v.toString().toLower();
                if (s == "true")
                    validated[it.key()] = true;
                else if (s == "false")
                    validated[it.key()] = false;
                // else: malformed string — drop key, default will apply
            }
            // else: non-bool, non-string type — drop key, default will apply
            break;
        }
        case ConfigFieldType::IntRange: {
            QVariant v = it.value();
            bool ok = false;
            int val = v.toInt(&ok);
            if (!ok) break;  // malformed — drop key, default will apply
            val = qBound(field->rangeMin, val, field->rangeMax);
            validated[it.key()] = val;
            break;
        }
        case ConfigFieldType::Enum: {
            if (field->values.contains(it.value())) {
                validated[it.key()] = it.value();
            }
            // else: silently drop invalid enum value (default will apply)
            break;
        }
        }
    }
    return validated;
}

void WidgetGridModel::setWidgetConfig(const QString& instanceId, const QVariantMap& config)
{
    int idx = findPlacement(instanceId);
    if (idx < 0) return;

    auto& p = livePlacements_[idx];
    QVariantMap validated = validateConfig(p.widgetId, config);
    p.config = validated;

    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, {ConfigRole});
    promoteToBase();
    emit placementsChanged();

    // Compute effective config (defaults + validated overrides) for live notification
    QVariantMap effective;
    if (registry_) {
        auto desc = registry_->descriptor(p.widgetId);
        if (desc) effective = desc->defaultConfig;
    }
    for (auto it = validated.constBegin(); it != validated.constEnd(); ++it)
        effective[it.key()] = it.value();

    emit widgetConfigChanged(instanceId, effective);
}

QVariantMap WidgetGridModel::widgetConfig(const QString& instanceId) const
{
    int idx = findPlacement(instanceId);
    if (idx < 0) return {};
    return livePlacements_[idx].config;
}

QVariantMap WidgetGridModel::effectiveWidgetConfig(const QString& instanceId) const
{
    int idx = findPlacement(instanceId);
    if (idx < 0) return {};

    const auto& p = livePlacements_[idx];
    QVariantMap result;
    if (registry_) {
        auto desc = registry_->descriptor(p.widgetId);
        if (desc) result = desc->defaultConfig;
    }
    for (auto it = p.config.constBegin(); it != p.config.constEnd(); ++it)
        result[it.key()] = it.value();
    return result;
}

QVariantList WidgetGridModel::configSchemaForWidget(const QString& widgetId) const
{
    if (!registry_) return {};
    auto desc = registry_->descriptor(widgetId);
    if (!desc || desc->configSchema.isEmpty()) return {};

    QVariantList result;
    for (const auto& field : desc->configSchema) {
        QVariantMap m;
        m["key"] = field.key;
        m["label"] = field.label;
        switch (field.type) {
        case ConfigFieldType::Enum:    m["type"] = QStringLiteral("enum"); break;
        case ConfigFieldType::Bool:    m["type"] = QStringLiteral("bool"); break;
        case ConfigFieldType::IntRange: m["type"] = QStringLiteral("intrange"); break;
        }
        m["options"] = QVariant::fromValue(field.options);
        m["values"] = field.values;
        m["rangeMin"] = field.rangeMin;
        m["rangeMax"] = field.rangeMax;
        m["rangeStep"] = field.rangeStep;
        result.append(m);
    }
    return result;
}

QVariantMap WidgetGridModel::defaultConfigForWidget(const QString& widgetId) const
{
    if (!registry_) return {};
    auto desc = registry_->descriptor(widgetId);
    if (!desc) return {};
    return desc->defaultConfig;
}

QVariantMap WidgetGridModel::widgetMeta(const QString& instanceId) const
{
    int idx = findPlacement(instanceId);
    if (idx < 0) return {};

    const auto& p = livePlacements_[idx];
    QVariantMap meta;
    meta["widgetId"] = p.widgetId;
    meta["instanceId"] = p.instanceId;

    if (registry_) {
        auto desc = registry_->descriptor(p.widgetId);
        if (desc) {
            meta["displayName"] = desc->displayName;
            meta["iconName"] = desc->iconName;
            meta["hasConfigSchema"] = !desc->configSchema.isEmpty();
            meta["isSingleton"] = desc->singleton;
        }
    }

    return meta;
}

bool WidgetGridModel::canPlace(int col, int row, int colSpan, int rowSpan,
                                const QString& excludeInstanceId) const
{
    return canPlaceOnPage(col, row, colSpan, rowSpan, activePage_, excludeInstanceId);
}

bool WidgetGridModel::canPlaceOnPage(int col, int row, int colSpan, int rowSpan,
                                      int page, const QString& excludeInstanceId) const
{
    // Bounds check
    if (col < 0 || row < 0) return false;
    if (col + colSpan > cols_ || row + rowSpan > rows_) return false;

    // Direct iteration over placements for page-scoped collision
    for (const auto& p : livePlacements_) {
        if (p.page != page || !p.visible) continue;
        if (!excludeInstanceId.isEmpty() && p.instanceId == excludeInstanceId) continue;

        // Check rectangle overlap
        if (col < p.col + p.colSpan && col + colSpan > p.col &&
            row < p.row + p.rowSpan && row + rowSpan > p.row)
            return false;
    }
    return true;
}

QVariantMap WidgetGridModel::findFirstAvailableCell(int colSpan, int rowSpan) const
{
    for (int r = 0; r <= rows_ - rowSpan; ++r) {
        for (int c = 0; c <= cols_ - colSpan; ++c) {
            if (canPlace(c, r, colSpan, rowSpan))
                return {{"col", c}, {"row", r}};
        }
    }
    return {{"col", -1}, {"row", -1}};
}

// --- Page management ---

int WidgetGridModel::activePage() const { return activePage_; }

void WidgetGridModel::setActivePage(int page)
{
    if (activePage_ == page) return;
    activePage_ = page;
    rebuildOccupancy();
    emit activePageChanged();
}

int WidgetGridModel::pageCount() const { return pageCount_; }

void WidgetGridModel::setPageCount(int count)
{
    if (pageCount_ == count || count < 1) return;
    pageCount_ = count;
    emit pageCountChanged();
}

void WidgetGridModel::addPage()
{
    int insertAt = pageCount_ - 1; // Insert before the reserved (last) page

    beginResetModel();
    // Shift all placements on pages >= insertAt up by 1
    for (auto& p : livePlacements_) {
        if (p.page >= insertAt)
            p.page++;
    }
    for (auto& p : basePlacements_) {
        if (p.page >= insertAt)
            p.page++;
    }
    pageCount_++;
    rebuildOccupancy();
    endResetModel();

    emit pageCountChanged();
    emit placementsChanged();
}

bool WidgetGridModel::removePage(int page)
{
    // Cannot remove page 0 (first page) or pages with singleton widgets
    if (page <= 0 || page >= pageCount_) return false;
    if (pageHasSingleton(page)) return false;

    // Remove all placements on this page
    beginResetModel();
    for (int i = livePlacements_.size() - 1; i >= 0; --i) {
        if (livePlacements_[i].page == page)
            livePlacements_.removeAt(i);
    }

    // Shift higher pages down
    for (auto& p : livePlacements_) {
        if (p.page > page)
            p.page--;
    }
    endResetModel();

    pageCount_--;

    // Adjust activePage if needed
    if (activePage_ >= pageCount_) {
        activePage_ = pageCount_ - 1;
        emit activePageChanged();
    }

    rebuildOccupancy();
    promoteToBase();
    emit pageCountChanged();
    emit placementsChanged();
    return true;
}

void WidgetGridModel::removeAllWidgetsOnPage(int page)
{
    beginResetModel();
    for (int i = livePlacements_.size() - 1; i >= 0; --i) {
        if (livePlacements_[i].page == page)
            livePlacements_.removeAt(i);
    }
    endResetModel();

    rebuildOccupancy();
    promoteToBase();
    emit placementsChanged();
}

int WidgetGridModel::widgetCountOnPage(int page) const
{
    int count = 0;
    for (const auto& p : livePlacements_) {
        if (p.page == page && p.visible)
            count++;
    }
    return count;
}

// --- Grid dimensions with proportional remap ---

void WidgetGridModel::setGridDimensions(int cols, int rows)
{
    if (cols == cols_ && rows == rows_) return;

    // Widget-selected deferral: queue dims but don't remap
    if (widgetSelected_) {
        pendingCols_ = cols;
        pendingRows_ = rows;
        remapPending_ = true;
        // Still update cols_/rows_ so grid dimensions property reflects the change
        cols_ = cols;
        rows_ = rows;
        emit gridDimensionsChanged();
        return;
    }

    cols_ = cols;
    rows_ = rows;

    // Boot readiness guard: if model not yet initialized, just store dims
    // Defer remap until setPlacements + setSavedDimensions have been called
    if (basePlacements_.isEmpty() && savedCols_ == 0) {
        clearOccupancy();
        emit gridDimensionsChanged();
        return;
    }

    // First-time setup guard: savedCols=0 means config never saved grid dims.
    // Adopt current dims as baseline -- no remap needed
    if (savedCols_ == 0) {
        savedCols_ = cols;
        savedRows_ = rows;
        beginResetModel();
        livePlacements_ = basePlacements_;
        endResetModel();
        rebuildOccupancy();
        emit gridDimensionsChanged();
        emit placementsChanged();
        return;
    }

    // Same as saved dims: identity copy from base
    if (cols == savedCols_ && rows == savedRows_) {
        beginResetModel();
        livePlacements_ = basePlacements_;
        endResetModel();
        rebuildOccupancy();
        emit gridDimensionsChanged();
        emit placementsChanged();
        return;
    }

    // Dims differ from saved: proportional remap
    remapPlacements(cols, rows);

    rebuildOccupancy();
    emit gridDimensionsChanged();
    emit placementsChanged();
    beginResetModel();
    endResetModel();
}

void WidgetGridModel::remapPlacements(int newCols, int newRows)
{
    QHash<int, QVector<QString>> pageOccupancy;
    QList<GridPlacement> result;

    for (const auto& base : basePlacements_) {
        GridPlacement p = base;

        // 1. Check min span vs grid dims
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc) {
                if (desc->minCols > newCols || desc->minRows > newRows) {
                    p.visible = false;
                    result.append(p);
                    qWarning() << "Widget" << p.instanceId << "hidden: min span"
                               << desc->minCols << "x" << desc->minRows
                               << "exceeds grid" << newCols << "x" << newRows;
                    continue;
                }
            }
        }

        // 2. Clamp spans to fit new grid (preserve as much as possible, respect minCols/minRows)
        if (p.colSpan > newCols) {
            p.colSpan = newCols;
            if (registry_) {
                auto desc = registry_->descriptor(p.widgetId);
                if (desc) p.colSpan = std::max(p.colSpan, desc->minCols);
            }
        }
        if (p.rowSpan > newRows) {
            p.rowSpan = newRows;
            if (registry_) {
                auto desc = registry_->descriptor(p.widgetId);
                if (desc) p.rowSpan = std::max(p.rowSpan, desc->minRows);
            }
        }

        // 3. Proportional position (spans stay original size when possible)
        p.col = qRound(static_cast<double>(base.col) * newCols / savedCols_);
        p.row = qRound(static_cast<double>(base.row) * newRows / savedRows_);

        // 4. Clamp to bounds
        p.col = std::min(p.col, std::max(0, newCols - p.colSpan));
        p.row = std::min(p.row, std::max(0, newRows - p.rowSpan));

        // 5. Ensure page occupancy exists
        if (!pageOccupancy.contains(p.page))
            pageOccupancy[p.page] = QVector<QString>(newCols * newRows);

        auto& occ = pageOccupancy[p.page];

        // 6. Check overlap with already-placed widgets on same page
        bool overlaps = false;
        for (int c = p.col; c < p.col + p.colSpan && !overlaps; ++c) {
            for (int r = p.row; r < p.row + p.rowSpan && !overlaps; ++r) {
                if (c < newCols && r < newRows && !occ[r * newCols + c].isEmpty())
                    overlaps = true;
            }
        }

        // 7. If overlap, try spiral nudge
        if (overlaps) {
            overlaps = !spiralNudge(p, occ, newCols, newRows);
        }

        // 8. If still overlapping, spill to next page
        if (overlaps) {
            spillToNextPage(p, pageOccupancy, newCols, newRows);
        }

        // 9. Mark placed position in occupancy
        p.visible = true;
        auto& finalOcc = pageOccupancy[p.page];
        for (int c = p.col; c < p.col + p.colSpan; ++c) {
            for (int r = p.row; r < p.row + p.rowSpan; ++r) {
                if (c < newCols && r < newRows)
                    finalOcc[r * newCols + c] = p.instanceId;
            }
        }

        result.append(p);
    }

    beginResetModel();
    livePlacements_ = result;
    endResetModel();
}

bool WidgetGridModel::spiralNudge(GridPlacement& p, const QVector<QString>& occupancy,
                                    int cols, int rows) const
{
    // Spiral search: expanding rings from the target position
    int maxRadius = std::max(cols, rows);

    for (int radius = 1; radius <= maxRadius; ++radius) {
        // Try all positions at this Manhattan distance in a spiral pattern
        for (int dc = -radius; dc <= radius; ++dc) {
            for (int dr = -radius; dr <= radius; ++dr) {
                // Only check positions at the current ring (not inner positions)
                if (std::abs(dc) != radius && std::abs(dr) != radius)
                    continue;

                int nc = p.col + dc;
                int nr = p.row + dr;

                // Bounds check
                if (nc < 0 || nr < 0) continue;
                if (nc + p.colSpan > cols || nr + p.rowSpan > rows) continue;

                // Check if all cells are free
                bool fits = true;
                for (int c = nc; c < nc + p.colSpan && fits; ++c) {
                    for (int r = nr; r < nr + p.rowSpan && fits; ++r) {
                        if (!occupancy[r * cols + c].isEmpty())
                            fits = false;
                    }
                }

                if (fits) {
                    p.col = nc;
                    p.row = nr;
                    return true;
                }
            }
        }
    }
    return false;
}

void WidgetGridModel::spillToNextPage(GridPlacement& p, QHash<int, QVector<QString>>& pageOcc,
                                       int cols, int rows)
{
    int nextPage = p.page + 1;
    // Try to place at (0,0) on successive pages
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (!pageOcc.contains(nextPage))
            pageOcc[nextPage] = QVector<QString>(cols * rows);

        auto& occ = pageOcc[nextPage];
        p.page = nextPage;
        p.col = 0;
        p.row = 0;

        // Check if (0,0) is free
        bool fits = true;
        for (int c = 0; c < p.colSpan && fits; ++c) {
            for (int r = 0; r < p.rowSpan && fits; ++r) {
                if (c < cols && r < rows && !occ[r * cols + c].isEmpty())
                    fits = false;
            }
        }

        if (fits) return;

        // Try spiral nudge on this page
        if (spiralNudge(p, occ, cols, rows)) return;

        nextPage++;
    }
}

int WidgetGridModel::gridColumns() const { return cols_; }
int WidgetGridModel::gridRows() const { return rows_; }

QList<GridPlacement> WidgetGridModel::placements() const
{
    return livePlacements_;
}

void WidgetGridModel::setPlacements(const QList<GridPlacement>& placements, WidgetRegistry* reg)
{
    beginResetModel();
    livePlacements_ = placements;
    if (reg) registry_ = reg;

    // Validate persisted config against current schema (sanitize stale/malformed values)
    if (registry_) {
        for (auto& p : livePlacements_) {
            if (!p.config.isEmpty()) {
                p.config = validateConfig(p.widgetId, p.config);
            }
        }
    }

    basePlacements_ = livePlacements_;
    rebuildOccupancy();
    endResetModel();
    emit placementsChanged();
}

void WidgetGridModel::setSavedDimensions(int cols, int rows)
{
    savedCols_ = cols;
    savedRows_ = rows;
}

void WidgetGridModel::setWidgetSelected(bool selected)
{
    if (widgetSelected_ == selected) return;
    widgetSelected_ = selected;

    // Notify QML so it can clear selectedInstanceId when C++ navigates away
    if (!selected)
        emit widgetDeselectedFromCpp();

    // When deselecting, apply pending remap if any
    if (!selected && remapPending_) {
        remapPending_ = false;
        // Dims are already set (cols_/rows_ updated in setGridDimensions)
        // Now apply the actual remap
        if (basePlacements_.isEmpty() && savedCols_ == 0) {
            // Still not initialized -- nothing to do
        } else if (savedCols_ == 0) {
            // First-time setup
            savedCols_ = cols_;
            savedRows_ = rows_;
            beginResetModel();
            livePlacements_ = basePlacements_;
            endResetModel();
            rebuildOccupancy();
            emit placementsChanged();
        } else if (cols_ == savedCols_ && rows_ == savedRows_) {
            beginResetModel();
            livePlacements_ = basePlacements_;
            endResetModel();
            rebuildOccupancy();
            emit placementsChanged();
        } else {
            remapPlacements(cols_, rows_);
            rebuildOccupancy();
            emit placementsChanged();
        }
    }
}

bool WidgetGridModel::pageHasSingleton(int page) const
{
    for (const auto& p : livePlacements_) {
        if (p.page != page) continue;
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc && desc->singleton) return true;
        }
    }
    return false;
}

bool WidgetGridModel::isReservedPage(int page) const
{
    return pageHasSingleton(page);
}

void WidgetGridModel::promoteToBase()
{
    basePlacements_ = livePlacements_;
    savedCols_ = cols_;
    savedRows_ = rows_;
}

int WidgetGridModel::findPlacement(const QString& instanceId) const
{
    for (int i = 0; i < livePlacements_.size(); ++i) {
        if (livePlacements_[i].instanceId == instanceId)
            return i;
    }
    return -1;
}

void WidgetGridModel::rebuildOccupancy()
{
    clearOccupancy();
    for (const auto& p : livePlacements_) {
        if (p.visible && p.page == activePage_)
            markOccupied(p);
    }
}

void WidgetGridModel::markOccupied(const GridPlacement& p)
{
    for (int c = p.col; c < p.col + p.colSpan; ++c) {
        for (int r = p.row; r < p.row + p.rowSpan; ++r) {
            if (c < cols_ && r < rows_)
                occupancy_[r * cols_ + c] = p.instanceId;
        }
    }
}

void WidgetGridModel::clearOccupancy()
{
    occupancy_.fill(QString(), cols_ * rows_);
}

QString WidgetGridModel::cellOwner(int col, int row) const
{
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_)
        return {};
    return occupancy_[row * cols_ + col];
}

} // namespace oap
