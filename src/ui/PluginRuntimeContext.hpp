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

    /// Call the plugin deactivation hook exactly once while retaining the child
    /// QQmlContext for an outgoing QML view still finishing its dispatch.
    void deactivatePlugin();

    /// Destroy the retained child context. Ensures the plugin hook ran first.
    void retireContext();

    QQmlContext* qmlContext() const { return childContext_; }
    bool isActive() const;
    IPlugin* plugin() const { return plugin_; }

private:
    enum class LifecycleState {
        Inactive,
        Active,
        PluginDeactivated,
        Retired,
    };

    IPlugin* plugin_;
    QQmlEngine* engine_;
    QQmlContext* childContext_ = nullptr;
    LifecycleState state_ = LifecycleState::Inactive;
};

} // namespace oap
