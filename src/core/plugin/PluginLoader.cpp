#include "PluginLoader.hpp"
#include "IPlugin.hpp"
#include <QPluginLoader>
#include <QDebug>

namespace oap {

IPlugin* PluginLoader::load(const QString& soPath)
{
    QPluginLoader loader(soPath);
    QObject* instance = loader.instance();
    if (!instance) {
        qCritical() << "Failed to load plugin: " << soPath
                                  << " — " << loader.errorString();
        return nullptr;
    }

    auto* plugin = qobject_cast<IPlugin*>(instance);
    if (!plugin) {
        qCritical() << "Loaded object from " << soPath
                                  << " does not implement IPlugin";
        return nullptr;
    }

    return plugin;
}

} // namespace oap
