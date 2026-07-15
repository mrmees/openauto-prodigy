#include "PluginLoader.hpp"
#include "IPlugin.hpp"
#include <QLibrary>
#include <QPluginLoader>
#include "../Logging.hpp"

namespace oap {

PluginLoader::LoadResult PluginLoader::load(const QString& soPath)
{
    auto* loader = new QPluginLoader(soPath);
    // Qt 6 sets QLibrary::PreventUnloadHint by DEFAULT on plugin loads —
    // clear it BEFORE instance() so a rejected binary can actually be
    // unloaded from the address space (the whole point of the ABI gate).
    loader->setLoadHints(loader->loadHints() & ~QLibrary::PreventUnloadHint);
    QObject* instance = loader->instance();
    if (!instance) {
        qCCritical(lcPlugin) << "Failed to load plugin: " << soPath
                             << " — " << loader->errorString();
        delete loader;
        return {};
    }

    auto* plugin = qobject_cast<IPlugin*>(instance);
    if (!plugin) {
        qCCritical(lcPlugin) << "Loaded object from " << soPath
                             << " does not implement IPlugin";
        if (!loader->unload())
            qCWarning(lcPlugin) << "unload failed for" << soPath;
        delete loader;
        return {};
    }

    return {loader, plugin};
}

} // namespace oap
