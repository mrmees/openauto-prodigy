#include "PluginDiscovery.hpp"
#include <QDir>
#include <boost/log/trivial.hpp>

namespace oap {

QList<PluginManifest> PluginDiscovery::discover(const QString& pluginsDir) const
{
    QList<PluginManifest> results;
    QDir dir(pluginsDir);

    if (!dir.exists()) {
        BOOST_LOG_TRIVIAL(debug) << "Plugin directory does not exist: " << pluginsDir.toStdString();
        return results;
    }

    for (const auto& entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString manifestPath = dir.absoluteFilePath(entry) + "/plugin.yaml";
        if (!QFile::exists(manifestPath))
            continue;

        auto manifest = PluginManifest::fromFile(manifestPath);
        if (!manifest.isValid()) {
            BOOST_LOG_TRIVIAL(warning) << "Invalid plugin manifest in " << entry.toStdString();
            continue;
        }

        if (!validateManifest(manifest)) {
            BOOST_LOG_TRIVIAL(warning) << "Plugin " << manifest.id.toStdString()
                                        << " requires API v" << manifest.apiVersion
                                        << " (host is v" << HOST_API_VERSION << "), skipping";
            continue;
        }

        BOOST_LOG_TRIVIAL(info) << "Discovered plugin: " << manifest.id.toStdString()
                                 << " v" << manifest.version.toStdString();
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
