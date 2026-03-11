#include "ui/WidgetPickerModel.hpp"
#include "core/widget/WidgetRegistry.hpp"

namespace oap {

WidgetPickerModel::WidgetPickerModel(WidgetRegistry* registry, QObject* parent)
    : QAbstractListModel(parent), registry_(registry) {}

int WidgetPickerModel::rowCount(const QModelIndex&) const {
    return filtered_.size();
}

QVariant WidgetPickerModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= filtered_.size())
        return {};
    const auto& desc = filtered_[index.row()];
    switch (role) {
        case WidgetIdRole: return desc.id;
        case DisplayNameRole: return desc.displayName;
        case IconNameRole: return desc.iconName;
        default: return {};
    }
}

QHash<int, QByteArray> WidgetPickerModel::roleNames() const {
    return {
        {WidgetIdRole, "widgetId"},
        {DisplayNameRole, "displayName"},
        {IconNameRole, "iconName"}
    };
}

void WidgetPickerModel::filterForSize(int sizeFlag) {
    beginResetModel();
    filtered_.clear();

    // Add a "No Widget" option at the top to allow clearing the pane
    WidgetDescriptor noneDesc;
    noneDesc.id = QString();
    noneDesc.displayName = QStringLiteral("No Widget");
    noneDesc.iconName = QStringLiteral("\ue5cd"); // close/clear icon
    filtered_.append(noneDesc);

    filtered_.append(registry_->widgetsForSize(static_cast<WidgetSize>(sizeFlag)));
    endResetModel();
}

} // namespace oap
