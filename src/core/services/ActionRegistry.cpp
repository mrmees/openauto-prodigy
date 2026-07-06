#include "ActionRegistry.hpp"
#include "../Logging.hpp"

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
        qCDebug(lcCore) << "ActionRegistry: unknown action '"
                                  << actionId << "'";
        return false;
    }
    // Copy the handler before invoking: the handler may trigger session
    // teardown that calls unregisterAction() for this very actionId, which
    // would destroy the std::function stored in handlers_ while it is still
    // executing (use-after-free on the captured state). Invoking a local
    // copy keeps the underlying handler alive for the duration of the call.
    Handler h = it.value();
    h(payload);
    emit actionDispatched(actionId);
    return true;
}

QStringList ActionRegistry::registeredActions() const
{
    return handlers_.keys();
}

} // namespace oap
