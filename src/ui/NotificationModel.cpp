#include "NotificationModel.hpp"
#include "core/services/NotificationService.hpp"

namespace oap {

NotificationModel::NotificationModel(NotificationService* service, QObject* parent)
    : QAbstractListModel(parent), service_(service)
{
    connect(service_, &NotificationService::notificationAdded, this, [this]() {
        beginResetModel(); endResetModel(); emit countChanged();
    });
    connect(service_, &NotificationService::notificationRemoved, this, [this]() {
        beginResetModel(); endResetModel(); emit countChanged();
    });
}

int NotificationModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return service_->active().size();
}

QVariant NotificationModel::data(const QModelIndex& index, int role) const
{
    auto notifications = service_->active();
    if (index.row() < 0 || index.row() >= notifications.size()) return {};
    const auto& n = notifications[index.row()];
    switch (role) {
    case NotificationIdRole:   return n.id;
    case KindRole:             return n.kind;
    case MessageRole:          return n.message;
    case SourcePluginRole:     return n.sourcePluginId;
    case PriorityRole:         return n.priority;
    default:                   return {};
    }
}

QHash<int, QByteArray> NotificationModel::roleNames() const
{
    return {
        {NotificationIdRole, "notificationId"},
        {KindRole,           "kind"},
        {MessageRole,        "message"},
        {SourcePluginRole,   "sourcePlugin"},
        {PriorityRole,       "priority"}
    };
}

} // namespace oap
