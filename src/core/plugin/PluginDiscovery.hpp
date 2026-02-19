#pragma once

#include "PluginManifest.hpp"
#include <QList>
#include <QString>

namespace oap {

/// Scans a directory for plugin subdirectories containing plugin.yaml manifests.
/// Pure file scanning + parsing — no .so loading, fully unit-testable.
class PluginDiscovery {
public:
    static constexpr int HOST_API_VERSION = 1;

    /// Scan pluginsDir for subdirectories containing plugin.yaml.
    /// Returns list of parsed and validated manifests.
    QList<PluginManifest> discover(const QString& pluginsDir) const;

    /// Validate a manifest against host API version.
    /// Returns true if compatible.
    static bool validateManifest(const PluginManifest& manifest, int hostApiVersion = HOST_API_VERSION);
};

} // namespace oap
