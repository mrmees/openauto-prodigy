#include "PluginLoader.hpp"
#include "IPlugin.hpp"
#include <QPluginLoader>
#include <boost/log/trivial.hpp>

namespace oap {

IPlugin* PluginLoader::load(const QString& soPath)
{
    QPluginLoader loader(soPath);
    QObject* instance = loader.instance();
    if (!instance) {
        BOOST_LOG_TRIVIAL(error) << "Failed to load plugin: " << soPath.toStdString()
                                  << " — " << loader.errorString().toStdString();
        return nullptr;
    }

    auto* plugin = qobject_cast<IPlugin*>(instance);
    if (!plugin) {
        BOOST_LOG_TRIVIAL(error) << "Loaded object from " << soPath.toStdString()
                                  << " does not implement IPlugin";
        return nullptr;
    }

    return plugin;
}

} // namespace oap
