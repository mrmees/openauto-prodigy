#include "PluginManager.hpp"
#include "PluginDiscovery.hpp"
#include "PluginLoader.hpp"
#include "IPlugin.hpp"
#include "IHostContext.hpp"
#include <boost/log/trivial.hpp>

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

    BOOST_LOG_TRIVIAL(info) << "Registered static plugin: " << plugin->id().toStdString();
    emit pluginLoaded(plugin->id());
}

void PluginManager::discoverPlugins(const QString& pluginsDir)
{
    PluginDiscovery discovery;
    auto manifests = discovery.discover(pluginsDir);

    for (const auto& manifest : manifests) {
        // Skip if already registered (e.g., static plugin with same ID)
        if (idIndex_.contains(manifest.id)) {
            BOOST_LOG_TRIVIAL(debug) << "Skipping discovered plugin " << manifest.id.toStdString()
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

        BOOST_LOG_TRIVIAL(info) << "Loaded dynamic plugin: " << manifest.id.toStdString();
        emit pluginLoaded(manifest.id);
    }
}

void PluginManager::initializeAll(IHostContext* context)
{
    for (auto& entry : entries_) {
        if (entry.initialized) continue;

        BOOST_LOG_TRIVIAL(info) << "Initializing plugin: " << entry.manifest.id.toStdString();

        bool ok = false;
        try {
            ok = entry.plugin->initialize(context);
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Plugin " << entry.manifest.id.toStdString()
                                      << " threw during initialize(): " << e.what();
        }

        if (ok) {
            entry.initialized = true;
            BOOST_LOG_TRIVIAL(info) << "Plugin initialized: " << entry.manifest.id.toStdString();
            emit pluginInitialized(entry.manifest.id);
        } else {
            BOOST_LOG_TRIVIAL(error) << "Plugin " << entry.manifest.id.toStdString()
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

        BOOST_LOG_TRIVIAL(info) << "Shutting down plugin: " << entry.manifest.id.toStdString();
        try {
            entry.plugin->shutdown();
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Plugin " << entry.manifest.id.toStdString()
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

} // namespace oap
