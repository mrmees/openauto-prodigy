#pragma once

#include <QString>

class QPluginLoader;

namespace oap {

class IPlugin;

/// Thin wrapper around QPluginLoader for loading dynamic plugin .so files.
class PluginLoader {
public:
    /// Result of a dynamic load. `loader` is heap-allocated and OWNED BY THE
    /// CALLER (PluginManager stores it in the PluginEntry); it is the handle
    /// for unload-on-rejection. Both members null on failure.
    struct LoadResult {
        QPluginLoader* loader = nullptr;
        IPlugin* plugin = nullptr;
        bool ok() const { return loader && plugin; }
    };

    static LoadResult load(const QString& soPath);
};

} // namespace oap
