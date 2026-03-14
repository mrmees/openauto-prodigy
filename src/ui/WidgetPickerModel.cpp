#include "ui/WidgetPickerModel.hpp"
#include "core/widget/WidgetRegistry.hpp"
#include <algorithm>

namespace oap {

static int categoryOrder(const QString& id, bool isNoWidget) {
    if (isNoWidget) return -1;
    if (id == "status") return 0;
    if (id == "media") return 1;
    if (id == "navigation") return 2;
    if (id == "launcher") return 3;
    if (id.isEmpty()) return 999; // uncategorized
    return 100; // known but not built-in
}

static QString categoryLabel(const QString& id) {
    static const QHash<QString, QString> labels = {
        {"status", "Status"},
        {"media", "Media"},
        {"navigation", "Navigation"},
        {"launcher", "Launcher"}
    };
    auto it = labels.find(id);
    if (it != labels.end()) return it.value();
    if (id.isEmpty()) return QStringLiteral("Other");
    // Capitalize first letter of unknown category
    QString result = id;
    result[0] = result[0].toUpper();
    return result;
}

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
        case CategoryRole: return desc.category;
        case DescriptionRole: return desc.description;
        case CategoryLabelRole: {
            // "No Widget" entry has empty id — its categoryLabel is empty
            if (desc.id.isEmpty() && desc.displayName == QStringLiteral("No Widget"))
                return QString();
            return categoryLabel(desc.category);
        }
        default: return {};
    }
}

QHash<int, QByteArray> WidgetPickerModel::roleNames() const {
    return {
        {WidgetIdRole, "widgetId"},
        {DisplayNameRole, "displayName"},
        {IconNameRole, "iconName"},
        {DefaultColsRole, "defaultCols"},
        {DefaultRowsRole, "defaultRows"},
        {CategoryRole, "category"},
        {DescriptionRole, "description"},
        {CategoryLabelRole, "categoryLabel"}
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

    // Sort: "No Widget" first (empty id), then by category order, then alpha by name
    std::stable_sort(filtered_.begin(), filtered_.end(),
        [](const WidgetDescriptor& a, const WidgetDescriptor& b) {
            bool aIsNone = a.id.isEmpty() && a.displayName == QStringLiteral("No Widget");
            bool bIsNone = b.id.isEmpty() && b.displayName == QStringLiteral("No Widget");
            int orderA = categoryOrder(a.category, aIsNone);
            int orderB = categoryOrder(b.category, bIsNone);
            if (orderA != orderB) return orderA < orderB;
            return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
        });

    endResetModel();
}

} // namespace oap
