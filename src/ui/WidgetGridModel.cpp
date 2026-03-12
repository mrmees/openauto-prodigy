#include "ui/WidgetGridModel.hpp"
#include "core/widget/WidgetRegistry.hpp"
#include <algorithm>

namespace oap {

WidgetGridModel::WidgetGridModel(WidgetRegistry* registry, QObject* parent)
    : QAbstractListModel(parent), registry_(registry)
{
}

int WidgetGridModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return placements_.size();
}

QVariant WidgetGridModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= placements_.size())
        return {};

    const auto& p = placements_[index.row()];
    switch (role) {
    case WidgetIdRole:      return p.widgetId;
    case InstanceIdRole:    return p.instanceId;
    case ColumnRole:        return p.col;
    case RowRole:           return p.row;
    case ColSpanRole:       return p.colSpan;
    case RowSpanRole:       return p.rowSpan;
    case OpacityRole:       return p.opacity;
    case VisibleRole:       return p.visible;
    case QmlComponentRole: {
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc) return desc->qmlComponent;
        }
        return {};
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
        {VisibleRole, "visible"}
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
    p.visible = true;

    beginInsertRows(QModelIndex(), placements_.size(), placements_.size());
    placements_.append(p);
    markOccupied(p);
    endInsertRows();

    emit placementsChanged();
    return true;
}

bool WidgetGridModel::moveWidget(const QString& instanceId, int newCol, int newRow)
{
    int idx = findPlacement(instanceId);
    if (idx < 0) return false;

    const auto& p = placements_[idx];
    if (!canPlace(newCol, newRow, p.colSpan, p.rowSpan, instanceId))
        return false;

    placements_[idx].col = newCol;
    placements_[idx].row = newRow;
    rebuildOccupancy();

    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, {ColumnRole, RowRole});
    emit placementsChanged();
    return true;
}

bool WidgetGridModel::resizeWidget(const QString& instanceId, int newColSpan, int newRowSpan)
{
    int idx = findPlacement(instanceId);
    if (idx < 0) return false;

    const auto& p = placements_[idx];

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

    if (!canPlace(p.col, p.row, newColSpan, newRowSpan, instanceId))
        return false;

    placements_[idx].colSpan = newColSpan;
    placements_[idx].rowSpan = newRowSpan;
    rebuildOccupancy();

    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, {ColSpanRole, RowSpanRole});
    emit placementsChanged();
    return true;
}

void WidgetGridModel::removeWidget(const QString& instanceId)
{
    int idx = findPlacement(instanceId);
    if (idx < 0) return;

    beginRemoveRows(QModelIndex(), idx, idx);
    placements_.removeAt(idx);
    endRemoveRows();

    rebuildOccupancy();
    emit placementsChanged();
}

void WidgetGridModel::setWidgetOpacity(const QString& instanceId, double opacity)
{
    int idx = findPlacement(instanceId);
    if (idx < 0) return;

    placements_[idx].opacity = qBound(0.0, opacity, 1.0);
    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, {OpacityRole});
    emit placementsChanged();
}

bool WidgetGridModel::canPlace(int col, int row, int colSpan, int rowSpan,
                                const QString& excludeInstanceId) const
{
    // Bounds check
    if (col < 0 || row < 0) return false;
    if (col + colSpan > cols_ || row + rowSpan > rows_) return false;

    // Occupancy check
    for (int c = col; c < col + colSpan; ++c) {
        for (int r = row; r < row + rowSpan; ++r) {
            const QString& owner = occupancy_[r * cols_ + c];
            if (!owner.isEmpty() && owner != excludeInstanceId)
                return false;
        }
    }
    return true;
}

void WidgetGridModel::setGridDimensions(int cols, int rows)
{
    if (cols == cols_ && rows == rows_) return;

    cols_ = cols;
    rows_ = rows;

    // Clamping algorithm: preserve insertion order, first-placed wins
    clearOccupancy();

    for (int i = 0; i < placements_.size(); ++i) {
        auto& p = placements_[i];

        // Clamp spans to fit new dimensions (don't exceed grid)
        p.colSpan = std::min(p.colSpan, cols_);
        p.rowSpan = std::min(p.rowSpan, rows_);

        // Also clamp spans to widget's min constraints (don't go below min)
        if (registry_) {
            auto desc = registry_->descriptor(p.widgetId);
            if (desc) {
                p.colSpan = std::max(p.colSpan, desc->minCols);
                p.rowSpan = std::max(p.rowSpan, desc->minRows);
                // If min constraints exceed grid, widget cannot fit
                if (p.colSpan > cols_ || p.rowSpan > rows_) {
                    p.visible = false;
                    continue;
                }
            }
        }

        // Clamp position so widget stays in bounds
        p.col = std::min(p.col, std::max(0, cols_ - p.colSpan));
        p.row = std::min(p.row, std::max(0, rows_ - p.rowSpan));

        // Check overlap with already-placed widgets
        bool overlaps = false;
        for (int c = p.col; c < p.col + p.colSpan && !overlaps; ++c) {
            for (int r = p.row; r < p.row + p.rowSpan && !overlaps; ++r) {
                if (!occupancy_[r * cols_ + c].isEmpty())
                    overlaps = true;
            }
        }

        if (overlaps) {
            p.visible = false;
        } else {
            p.visible = true;
            markOccupied(p);
        }
    }

    emit gridDimensionsChanged();
    emit placementsChanged();
    // Full model reset since positions may have changed
    beginResetModel();
    endResetModel();
}

int WidgetGridModel::gridColumns() const { return cols_; }
int WidgetGridModel::gridRows() const { return rows_; }

QList<GridPlacement> WidgetGridModel::placements() const
{
    return placements_;
}

void WidgetGridModel::setPlacements(const QList<GridPlacement>& placements, WidgetRegistry* reg)
{
    beginResetModel();
    placements_ = placements;
    if (reg) registry_ = reg;
    rebuildOccupancy();
    endResetModel();
    emit placementsChanged();
}

int WidgetGridModel::findPlacement(const QString& instanceId) const
{
    for (int i = 0; i < placements_.size(); ++i) {
        if (placements_[i].instanceId == instanceId)
            return i;
    }
    return -1;
}

void WidgetGridModel::rebuildOccupancy()
{
    clearOccupancy();
    for (const auto& p : placements_) {
        if (p.visible)
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
