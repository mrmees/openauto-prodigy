#include "PluginRuntimeContext.hpp"
#include "core/plugin/IPlugin.hpp"
#include <QQmlEngine>
#include <boost/log/trivial.hpp>

namespace oap {

PluginRuntimeContext::PluginRuntimeContext(IPlugin* plugin, QQmlEngine* engine, QObject* parent)
    : QObject(parent)
    , plugin_(plugin)
    , engine_(engine)
{
}

PluginRuntimeContext::~PluginRuntimeContext()
{
    if (active_)
        deactivate();
}

void PluginRuntimeContext::activate()
{
    if (active_) return;

    BOOST_LOG_TRIVIAL(debug) << "Activating plugin context: " << plugin_->id().toStdString();

    // Create a child context so plugin bindings don't pollute the root context
    childContext_ = new QQmlContext(engine_->rootContext(), this);

    // Let the plugin expose its objects to this context
    plugin_->onActivated(childContext_);

    active_ = true;
}

void PluginRuntimeContext::deactivate()
{
    if (!active_) return;

    BOOST_LOG_TRIVIAL(debug) << "Deactivating plugin context: " << plugin_->id().toStdString();

    // Let the plugin clean up before we destroy the context
    plugin_->onDeactivated();

    // Destroy the child context — prevents QObject leaks
    delete childContext_;
    childContext_ = nullptr;

    active_ = false;
}

} // namespace oap
