#pragma once

#include <QObject>
#include <QQmlContext>

class QQmlEngine;

namespace oap {

class IPlugin;

/// Manages a plugin's QML lifecycle: child context, activation state.
/// Shell owns this object. Created when a plugin is activated, destroyed
/// when deactivated. Prevents QML context property leaks between plugins.
class PluginRuntimeContext : public QObject {
    Q_OBJECT
public:
    PluginRuntimeContext(IPlugin* plugin, QQmlEngine* engine, QObject* parent = nullptr);
    ~PluginRuntimeContext() override;

    /// Create child QQmlContext, call plugin's onActivated().
    /// The child context is where the plugin exposes its QML bindings.
    void activate();

    /// Call plugin's onDeactivated(), destroy child context.
    void deactivate();

    QQmlContext* qmlContext() const { return childContext_; }
    bool isActive() const { return active_; }
    IPlugin* plugin() const { return plugin_; }

private:
    IPlugin* plugin_;
    QQmlEngine* engine_;
    QQmlContext* childContext_ = nullptr;
    bool active_ = false;
};

} // namespace oap
