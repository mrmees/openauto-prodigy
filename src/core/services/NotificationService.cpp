#include "NotificationService.hpp"

namespace oap {

NotificationService::NotificationService(QObject* parent) : QObject(parent) {}

QString NotificationService::post(const QVariantMap& data)
{
    Notification n;
    n.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    n.kind = data.value("kind").toString();
    n.message = data.value("message").toString();
    n.sourcePluginId = data.value("sourcePluginId").toString();
    n.priority = data.value("priority", 50).toInt();
    n.ttlMs = data.value("ttlMs", 0).toInt();

    notifications_.append(n);
    emit notificationAdded(n);

    if (n.ttlMs > 0) {
        QString id = n.id;
        QTimer::singleShot(n.ttlMs, this, [this, id]() { dismiss(id); });
    }

    return n.id;
}

void NotificationService::dismiss(const QString& notificationId)
{
    for (int i = 0; i < notifications_.size(); ++i) {
        if (notifications_[i].id == notificationId) {
            notifications_.removeAt(i);
            emit notificationRemoved(notificationId);
            return;
        }
    }
}

} // namespace oap
