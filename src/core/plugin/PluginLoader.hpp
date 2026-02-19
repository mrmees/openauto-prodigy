#pragma once

#include <QString>

namespace oap {

class IPlugin;

/// Thin wrapper around QPluginLoader for loading dynamic plugin .so files.
/// Integration-test only — too fragile for unit tests.
class PluginLoader {
public:
    /// Load a plugin from a shared object file. Returns nullptr on failure.
    /// Caller does NOT own the returned pointer (QPluginLoader manages it).
    static IPlugin* load(const QString& soPath);
};

} // namespace oap
