#include "PluginManager.hpp"
#include "PluginDiscovery.hpp"
#include "PluginLoader.hpp"
#include "IPlugin.hpp"
#include "IHostContext.hpp"
#include <QDebug>

namespace oap {

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
{
}

PluginManager::~PluginManager()
{
    shutdownAll();
}

void PluginManager::registerStaticPlugin(IPlugin* plugin)
{
    if (!plugin) return;

    PluginEntry entry;
    entry.plugin = plugin;
    entry.isStatic = true;

    // Create a synthetic manifest for static plugins
    entry.manifest.id = plugin->id();
    entry.manifest.name = plugin->name();
    entry.manifest.version = plugin->version();
    entry.manifest.apiVersion = plugin->apiVersion();
    entry.manifest.type = "full";
    entry.manifest.requiredServices = plugin->requiredServices();

    int idx = entries_.size();
    entries_.append(entry);
    idIndex_[plugin->id()] = idx;

    qInfo() << "Registered static plugin: " << plugin->id();
    emit pluginLoaded(plugin->id());
}

void PluginManager::discoverPlugins(const QString& pluginsDir)
{
    PluginDiscovery discovery;
    auto manifests = discovery.discover(pluginsDir);

    for (const auto& manifest : manifests) {
        // Skip if already registered (e.g., static plugin with same ID)
        if (idIndex_.contains(manifest.id)) {
            qDebug() << "Skipping discovered plugin " << manifest.id
                                      << " — already registered (static)";
            continue;
        }

        // Try to load the .so
        QString soPath = manifest.dirPath + "/lib" + manifest.id.split('.').last() + ".so";
        IPlugin* plugin = PluginLoader::load(soPath);
        if (!plugin) {
            emit pluginFailed(manifest.id, "Failed to load shared library");
            continue;
        }

        PluginEntry entry;
        entry.plugin = plugin;
        entry.manifest = manifest;
        entry.isStatic = false;

        int idx = entries_.size();
        entries_.append(entry);
        idIndex_[manifest.id] = idx;

        qInfo() << "Loaded dynamic plugin: " << manifest.id;
        emit pluginLoaded(manifest.id);
    }
}

void PluginManager::initializeAll(IHostContext* context)
{
    for (auto& entry : entries_) {
        if (entry.initialized) continue;

        qInfo() << "Initializing plugin: " << entry.manifest.id;

        bool ok = false;
        try {
            ok = entry.plugin->initialize(context);
        } catch (const std::exception& e) {
            qCritical() << "Plugin " << entry.manifest.id
                                      << " threw during initialize(): " << e.what();
        }

        if (ok) {
            entry.initialized = true;
            qInfo() << "Plugin initialized: " << entry.manifest.id;
            emit pluginInitialized(entry.manifest.id);
        } else {
            qCritical() << "Plugin " << entry.manifest.id
                                      << " failed to initialize — disabled";
            emit pluginFailed(entry.manifest.id, "initialize() returned false");
        }
    }
}

void PluginManager::shutdownAll()
{
    // Shutdown in reverse order
    for (int i = entries_.size() - 1; i >= 0; --i) {
        auto& entry = entries_[i];
        if (!entry.initialized) continue;

        qInfo() << "Shutting down plugin: " << entry.manifest.id;
        try {
            entry.plugin->shutdown();
        } catch (const std::exception& e) {
            qCritical() << "Plugin " << entry.manifest.id
                                      << " threw during shutdown(): " << e.what();
        }
        entry.initialized = false;
    }
}

QList<IPlugin*> PluginManager::plugins() const
{
    QList<IPlugin*> result;
    for (const auto& entry : entries_) {
        if (entry.initialized)
            result.append(entry.plugin);
    }
    return result;
}

IPlugin* PluginManager::plugin(const QString& id) const
{
    auto it = idIndex_.find(id);
    if (it == idIndex_.end()) return nullptr;
    return entries_[it.value()].plugin;
}

PluginManifest PluginManager::manifest(const QString& id) const
{
    auto it = idIndex_.find(id);
    if (it == idIndex_.end()) return {};
    return entries_[it.value()].manifest;
}

bool PluginManager::activatePlugin(const QString& pluginId)
{
    auto it = idIndex_.find(pluginId);
    if (it == idIndex_.end()) return false;
    auto& entry = entries_[it.value()];
    if (!entry.initialized) return false;

    if (activePluginId_ == pluginId) return true;

    // Deactivate current
    deactivateCurrentPlugin();

    activePluginId_ = pluginId;
    // onActivated() is called by the UI layer (PluginRuntimeContext) which owns the QML context
    emit pluginActivated(pluginId);
    return true;
}

void PluginManager::deactivateCurrentPlugin()
{
    if (activePluginId_.isEmpty()) return;
    QString prev = activePluginId_;
    activePluginId_.clear();
    emit pluginDeactivated(prev);
}

QString PluginManager::activePluginId() const
{
    return activePluginId_;
}

} // namespace oap
