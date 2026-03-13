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
        case DefaultColsRole: return desc.defaultCols;
        case DefaultRowsRole: return desc.defaultRows;
        default: return {};
    }
}

QHash<int, QByteArray> WidgetPickerModel::roleNames() const {
    return {
        {WidgetIdRole, "widgetId"},
        {DisplayNameRole, "displayName"},
        {IconNameRole, "iconName"},
        {DefaultColsRole, "defaultCols"},
        {DefaultRowsRole, "defaultRows"}
    };
}

void WidgetPickerModel::filterByAvailableSpace(int availCols, int availRows) {
    beginResetModel();
    filtered_.clear();

    // Add a "No Widget" option at the top to allow clearing the slot
    WidgetDescriptor noneDesc;
    noneDesc.id = QString();
    noneDesc.displayName = QStringLiteral("No Widget");
    noneDesc.iconName = QStringLiteral("\ue5cd"); // close/clear icon
    filtered_.append(noneDesc);

    filtered_.append(registry_->widgetsFittingSpace(availCols, availRows));
    endResetModel();
}

} // namespace oap
