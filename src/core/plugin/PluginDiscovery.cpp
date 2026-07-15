#include "PluginDiscovery.hpp"
#include <QDir>
#include "../Logging.hpp"

namespace oap {

QList<PluginManifest> PluginDiscovery::discover(const QString& pluginsDir) const
{
    QList<PluginManifest> results;
    QDir dir(pluginsDir);

    if (!dir.exists()) {
        qCDebug(lcPlugin) << "Plugin directory does not exist: " << pluginsDir;
        return results;
    }

    for (const auto& entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString manifestPath = dir.absoluteFilePath(entry) + "/plugin.yaml";
        if (!QFile::exists(manifestPath))
            continue;

        auto manifest = PluginManifest::fromFile(manifestPath);
        if (!manifest.isValid()) {
            qCWarning(lcPlugin) << "Invalid plugin manifest in " << entry;
            continue;
        }

        if (!validateManifest(manifest)) {
            qCWarning(lcPlugin) << "Plugin " << manifest.id
                                        << " requires API v" << manifest.apiVersion
                                        << " (host is v" << HOST_API_VERSION << "), skipping";
            continue;
        }

        qCInfo(lcPlugin) << "Discovered plugin: " << manifest.id
                                 << " v" << manifest.version;
        results.append(manifest);
    }

    return results;
}

bool PluginDiscovery::validateManifest(const PluginManifest& manifest, int hostApiVersion)
{
    if (!manifest.isValid())
        return false;
    // Exact-match only: the C++ plugin ABI has no cross-version vtable
    // compatibility (see HOST_API_VERSION), so a manifest built against any
    // other host API version is rejected rather than accepted and mis-dispatched.
    return manifest.apiVersion == hostApiVersion;
}

} // namespace oap
