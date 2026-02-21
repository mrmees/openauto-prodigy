#pragma once

#include "INotificationService.hpp"
#include <QObject>
#include <QList>
#include <QTimer>
#include <QUuid>

namespace oap {

struct Notification {
    QString id;
    QString kind;       // "toast", "incoming_call", "status_icon"
    QString message;
    QString sourcePluginId;
    int priority = 50;
    int ttlMs = 0;      // 0 = persistent until dismissed
    QVariantMap extra;   // additional payload
};

class NotificationService : public QObject, public INotificationService {
    Q_OBJECT
public:
    explicit NotificationService(QObject* parent = nullptr);

    QString post(const QVariantMap& notification) override;
    void dismiss(const QString& notificationId) override;

    QList<Notification> active() const { return notifications_; }

signals:
    void notificationAdded(const oap::Notification& n);
    void notificationRemoved(const QString& id);

private:
    QList<Notification> notifications_;
};

} // namespace oap
