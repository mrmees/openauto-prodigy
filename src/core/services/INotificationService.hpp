#pragma once

#include <QVariantMap>
#include <QString>

namespace oap {

class INotificationService {
public:
    virtual ~INotificationService() = default;

    /// Post a notification. Required fields: kind, message.
    /// Optional: sourcePluginId, priority (0-100), ttlMs (0 = persistent).
    /// Returns notification ID.
    virtual QString post(const QVariantMap& notification) = 0;

    /// Dismiss a notification by ID.
    virtual void dismiss(const QString& notificationId) = 0;
};

} // namespace oap
