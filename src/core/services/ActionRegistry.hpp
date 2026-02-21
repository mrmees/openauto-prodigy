#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <functional>

namespace oap {

/// Registry for named actions. Actions are synchronous command handlers
/// (as opposed to EventBus topics which are async notifications).
/// Thread-safe for registration; dispatch must happen on the main thread.
class ActionRegistry : public QObject {
    Q_OBJECT
public:
    using Handler = std::function<void(const QVariant& payload)>;

    explicit ActionRegistry(QObject* parent = nullptr);

    void registerAction(const QString& actionId, Handler handler);
    void unregisterAction(const QString& actionId);
    Q_INVOKABLE bool dispatch(const QString& actionId, const QVariant& payload = {});
    QStringList registeredActions() const;

signals:
    void actionDispatched(const QString& actionId);

private:
    QHash<QString, Handler> handlers_;
};

} // namespace oap
