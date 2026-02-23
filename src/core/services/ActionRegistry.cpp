#include "ActionRegistry.hpp"
#include <QDebug>

namespace oap {

ActionRegistry::ActionRegistry(QObject* parent) : QObject(parent) {}

void ActionRegistry::registerAction(const QString& actionId, Handler handler)
{
    handlers_[actionId] = std::move(handler);
}

void ActionRegistry::unregisterAction(const QString& actionId)
{
    handlers_.remove(actionId);
}

bool ActionRegistry::dispatch(const QString& actionId, const QVariant& payload)
{
    auto it = handlers_.find(actionId);
    if (it == handlers_.end()) {
        qDebug() << "ActionRegistry: unknown action '"
                                  << actionId << "'";
        return false;
    }
    it.value()(payload);
    emit actionDispatched(actionId);
    return true;
}

QStringList ActionRegistry::registeredActions() const
{
    return handlers_.keys();
}

} // namespace oap
