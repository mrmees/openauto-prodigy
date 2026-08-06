#pragma once

#include <QAbstractListModel>
#include "core/widget/WidgetTypes.hpp"

namespace oap {

class WidgetRegistry;

class WidgetPickerModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QVariantList categories READ categories NOTIFY categoriesChanged)
    Q_PROPERTY(int categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY categoryFilterChanged)
public:
    enum Roles {
        WidgetIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        IconNameRole,
        DefaultColsRole,
        DefaultRowsRole,
        CategoryRole,
        DescriptionRole,
        CategoryLabelRole,
        MinColsRole,
        MinRowsRole,
        MaxColsRole,
        MaxRowsRole,
        DefaultSizeLabelRole,
        SizeRangeLabelRole
    };

    explicit WidgetPickerModel(WidgetRegistry* registry, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void filterByAvailableSpace(int availCols, int availRows, bool includeNoWidget = true);
    QVariantList categories() const;
    int categoryFilter() const { return categoryFilter_; }
    Q_INVOKABLE void setCategoryFilter(int categoryIndex);

signals:
    void categoriesChanged();
    void categoryFilterChanged();

private:
    QStringList availableCategoryIds() const;
    void rebuildFiltered();

    WidgetRegistry* registry_;
    QList<WidgetDescriptor> available_;
    QList<WidgetDescriptor> filtered_;
    int categoryFilter_ = 0;
};

} // namespace oap
