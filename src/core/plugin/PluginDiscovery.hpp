#pragma once

#include "PluginManifest.hpp"
#include <QList>
#include <QString>

namespace oap {

/// Scans a directory for plugin subdirectories containing plugin.yaml manifests.
/// Pure file scanning + parsing — no .so loading, fully unit-testable.
class PluginDiscovery {
public:
    // v2: the IHostContext vtable changed in the B2 teardown
    // (companionListenerService() removed, 2026-07-14). The C++ plugin ABI has
    // no cross-version vtable compatibility, so acceptance is exact-match — a
    // stale v1 .so must be rejected, not mis-dispatched (see validateManifest).
    static constexpr int HOST_API_VERSION = 2;

    /// Scan pluginsDir for subdirectories containing plugin.yaml.
    /// Returns list of parsed and validated manifests.
    QList<PluginManifest> discover(const QString& pluginsDir) const;

    /// Validate a manifest against host API version.
    /// Returns true if compatible.
    static bool validateManifest(const PluginManifest& manifest, int hostApiVersion = HOST_API_VERSION);
};

} // namespace oap
