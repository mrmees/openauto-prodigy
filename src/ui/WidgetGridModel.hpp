#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVariantMap>
#include <QVector>
#include "core/widget/WidgetTypes.hpp"

namespace oap {

class WidgetRegistry;

class WidgetGridModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int gridColumns READ gridColumns NOTIFY gridDimensionsChanged)
    Q_PROPERTY(int gridRows READ gridRows NOTIFY gridDimensionsChanged)
    Q_PROPERTY(int activePage READ activePage WRITE setActivePage NOTIFY activePageChanged)
    Q_PROPERTY(int pageCount READ pageCount WRITE setPageCount NOTIFY pageCountChanged)

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
        VisibleRole,
        MinColsRole,
        MinRowsRole,
        MaxColsRole,
        MaxRowsRole,
        DefaultColsRole,
        DefaultRowsRole,
        PageRole,
        SingletonRole
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
    Q_INVOKABLE QVariantMap findFirstAvailableCell(int colSpan, int rowSpan) const;

    // Page management
    Q_INVOKABLE void addPage();
    Q_INVOKABLE bool removePage(int page);
    Q_INVOKABLE void removeAllWidgetsOnPage(int page);
    Q_INVOKABLE int widgetCountOnPage(int page) const;
    Q_INVOKABLE bool isReservedPage(int page) const;

    int activePage() const;
    void setActivePage(int page);
    int pageCount() const;
    void setPageCount(int count);

    // Grid dimensions
    void setGridDimensions(int cols, int rows);
    int gridColumns() const;
    int gridRows() const;

    // Base snapshot / remap support
    void setSavedDimensions(int cols, int rows);
    Q_INVOKABLE void setEditMode(bool editing);

    // Serialization
    QList<GridPlacement> placements() const;
    void setPlacements(const QList<GridPlacement>& placements, WidgetRegistry* registry = nullptr);

    // Instance ID management
    int nextInstanceId() const { return nextInstanceId_; }
    void setNextInstanceId(int id) { nextInstanceId_ = id; }

signals:
    void gridDimensionsChanged();
    void placementsChanged();
    void activePageChanged();
    void pageCountChanged();

private:
    int findPlacement(const QString& instanceId) const;
    bool canPlaceOnPage(int col, int row, int colSpan, int rowSpan,
                         int page, const QString& excludeInstanceId = {}) const;
    void rebuildOccupancy();
    void markOccupied(const GridPlacement& p);
    void clearOccupancy();
    QString cellOwner(int col, int row) const;

    // Remap algorithm
    void remapPlacements(int newCols, int newRows);
    bool spiralNudge(GridPlacement& p, const QVector<QString>& occupancy, int cols, int rows) const;
    void spillToNextPage(GridPlacement& p, QHash<int, QVector<QString>>& pageOcc, int cols, int rows);

    // Singleton page detection
    bool pageHasSingleton(int page) const;

    // After user edit: promote live state to new base
    void promoteToBase();

    WidgetRegistry* registry_;
    QList<GridPlacement> livePlacements_;
    QList<GridPlacement> basePlacements_;
    QVector<QString> occupancy_; // flat grid: cols_ * rows_
    int cols_ = 0;
    int rows_ = 0;
    int savedCols_ = 0;
    int savedRows_ = 0;
    int nextInstanceId_ = 0;
    int activePage_ = 0;
    int pageCount_ = 2;
    bool editMode_ = false;
    bool remapPending_ = false;
    int pendingCols_ = 0;
    int pendingRows_ = 0;
};

} // namespace oap
