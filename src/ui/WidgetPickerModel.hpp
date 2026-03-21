#pragma once

#include <QAbstractListModel>
#include "core/widget/WidgetTypes.hpp"

namespace oap {

class WidgetRegistry;

class WidgetPickerModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        WidgetIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        IconNameRole,
        DefaultColsRole,
        DefaultRowsRole,
        CategoryRole,
        DescriptionRole,
        CategoryLabelRole
    };

    explicit WidgetPickerModel(WidgetRegistry* registry, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void filterByAvailableSpace(int availCols, int availRows, bool includeNoWidget = true);

private:
    WidgetRegistry* registry_;
    QList<WidgetDescriptor> filtered_;
};

} // namespace oap
