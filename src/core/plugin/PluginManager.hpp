#pragma once

#include "PluginManifest.hpp"
#include <QObject>
#include <QList>
#include <QMap>
#include <QString>

namespace oap {

class IPlugin;
class IHostContext;

/// Lifecycle orchestration for plugins.
/// Manages: Discover → Load → Initialize → (Activate ↔ Deactivate) → Shutdown
class PluginManager : public QObject {
    Q_OBJECT
public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager() override;

    /// Register a static (compiled-in) plugin. PluginManager does NOT own it.
    void registerStaticPlugin(IPlugin* plugin);

    /// Scan a directory for dynamic plugins and validate their manifests.
    void discoverPlugins(const QString& pluginsDir);

    /// Initialize all registered plugins (static + discovered).
    /// Plugins whose initialize() returns false are disabled and logged.
    void initializeAll(IHostContext* context);

    /// Shut down all initialized plugins in reverse order.
    void shutdownAll();

    /// Get all loaded plugins (both static and dynamic).
    QList<IPlugin*> plugins() const;

    /// Look up a plugin by ID.
    IPlugin* plugin(const QString& id) const;

    /// Get the manifest for a plugin (static plugins get a synthetic manifest).
    PluginManifest manifest(const QString& id) const;

signals:
    void pluginLoaded(const QString& pluginId);
    void pluginInitialized(const QString& pluginId);
    void pluginFailed(const QString& pluginId, const QString& reason);

private:
    struct PluginEntry {
        IPlugin* plugin = nullptr;
        PluginManifest manifest;
        bool isStatic = false;
        bool initialized = false;
    };

    QList<PluginEntry> entries_;
    QMap<QString, int> idIndex_;  // plugin ID -> index in entries_
};

} // namespace oap
