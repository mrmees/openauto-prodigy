#pragma once

#include <QAbstractListModel>
#include <QVector>
#include "core/widget/WidgetTypes.hpp"

namespace oap {

class WidgetRegistry;

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
        QmlComponentRole,
        VisibleRole
    };

    explicit WidgetGridModel(WidgetRegistry* registry, QObject* parent = nullptr);

    // QAbstractListModel
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Core grid operations
    Q_INVOKABLE bool placeWidget(const QString& widgetId, int col, int row,
                                  int colSpan, int rowSpan);
    Q_INVOKABLE bool moveWidget(const QString& instanceId, int newCol, int newRow);
    Q_INVOKABLE bool resizeWidget(const QString& instanceId, int newColSpan, int newRowSpan);
    Q_INVOKABLE void removeWidget(const QString& instanceId);
    Q_INVOKABLE void setWidgetOpacity(const QString& instanceId, double opacity);
    Q_INVOKABLE bool canPlace(int col, int row, int colSpan, int rowSpan,
                               const QString& excludeInstanceId = {}) const;

    // Grid dimensions
    void setGridDimensions(int cols, int rows);
    int gridColumns() const;
    int gridRows() const;

    // Serialization
    QList<GridPlacement> placements() const;
    void setPlacements(const QList<GridPlacement>& placements, WidgetRegistry* registry = nullptr);

    // Instance ID management
    int nextInstanceId() const { return nextInstanceId_; }
    void setNextInstanceId(int id) { nextInstanceId_ = id; }

signals:
    void gridDimensionsChanged();
    void placementsChanged();

private:
    int findPlacement(const QString& instanceId) const;
    void rebuildOccupancy();
    void markOccupied(const GridPlacement& p);
    void clearOccupancy();
    QString cellOwner(int col, int row) const;

    WidgetRegistry* registry_;
    QList<GridPlacement> placements_;
    QVector<QString> occupancy_; // flat grid: cols_ * rows_
    int cols_ = 0;
    int rows_ = 0;
    int nextInstanceId_ = 0;
};

} // namespace oap
