#include "PluginDiscovery.hpp"
#include <QDir>
#include <QDebug>

namespace oap {

QList<PluginManifest> PluginDiscovery::discover(const QString& pluginsDir) const
{
    QList<PluginManifest> results;
    QDir dir(pluginsDir);

    if (!dir.exists()) {
        qDebug() << "Plugin directory does not exist: " << pluginsDir;
        return results;
    }

    for (const auto& entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString manifestPath = dir.absoluteFilePath(entry) + "/plugin.yaml";
        if (!QFile::exists(manifestPath))
            continue;

        auto manifest = PluginManifest::fromFile(manifestPath);
        if (!manifest.isValid()) {
            qWarning() << "Invalid plugin manifest in " << entry;
            continue;
        }

        if (!validateManifest(manifest)) {
            qWarning() << "Plugin " << manifest.id
                                        << " requires API v" << manifest.apiVersion
                                        << " (host is v" << HOST_API_VERSION << "), skipping";
            continue;
        }

        qInfo() << "Discovered plugin: " << manifest.id
                                 << " v" << manifest.version;
        results.append(manifest);
    }

    return results;
}

bool PluginDiscovery::validateManifest(const PluginManifest& manifest, int hostApiVersion)
{
    if (!manifest.isValid())
        return false;
    return manifest.apiVersion <= hostApiVersion;
}

} // namespace oap
