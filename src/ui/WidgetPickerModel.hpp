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
        IconNameRole
    };

    explicit WidgetPickerModel(WidgetRegistry* registry, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void filterForSize(int sizeFlag);

private:
    WidgetRegistry* registry_;
    QList<WidgetDescriptor> filtered_;
};

} // namespace oap
