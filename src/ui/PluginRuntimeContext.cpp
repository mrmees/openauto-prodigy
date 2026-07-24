#include "PluginRuntimeContext.hpp"
#include "core/plugin/IPlugin.hpp"
#include <QQmlEngine>
#include "../core/Logging.hpp"

namespace oap {

PluginRuntimeContext::PluginRuntimeContext(IPlugin* plugin, QQmlEngine* engine, QObject* parent)
    : QObject(parent)
    , plugin_(plugin)
    , engine_(engine)
{
}

PluginRuntimeContext::~PluginRuntimeContext()
{
    deactivatePlugin();
    retireContext();
}

void PluginRuntimeContext::activate()
{
    if (state_ != LifecycleState::Inactive)
        return;

    qCDebug(lcUI) << "Activating plugin context: " << plugin_->id();

    // Create a child context so plugin bindings don't pollute the root context
    childContext_ = new QQmlContext(engine_->rootContext(), this);
    state_ = LifecycleState::Active;

    // Let the plugin expose its objects to this context
    plugin_->onActivated(childContext_);
}

void PluginRuntimeContext::deactivatePlugin()
{
    if (state_ != LifecycleState::Active)
        return;

    qCDebug(lcUI) << "Deactivating plugin runtime: " << plugin_->id();

    // Runtime ownership/focus changes synchronously. The child context remains
    // valid until its outgoing QML view is destroyed on the deferred edge.
    state_ = LifecycleState::PluginDeactivated;
    plugin_->onDeactivated();
}

void PluginRuntimeContext::retireContext()
{
    if (state_ == LifecycleState::Retired)
        return;

    deactivatePlugin();

    delete childContext_;
    childContext_ = nullptr;
    state_ = LifecycleState::Retired;
}

bool PluginRuntimeContext::isActive() const
{
    return state_ == LifecycleState::Active;
}

} // namespace oap
