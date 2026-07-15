#pragma once

#include <QString>
#include <QVariant>

namespace oap {

class IConfigService {
public:
    virtual ~IConfigService() = default;

    /// Read a top-level config value by dot-notation key (e.g., "display.brightness").
    /// Returns invalid QVariant if key not found.
    /// Thread-safe.
    virtual QVariant value(const QString& key) const = 0;

    /// Write a top-level config value.
    /// Must be called from the main thread (single-writer rule).
    virtual void setValue(const QString& key, const QVariant& value) = 0;

    /// Read a plugin-scoped config value.
    /// Each plugin's config is isolated under its ID namespace.
    /// Returns invalid QVariant if key not found.
    /// Thread-safe.
    virtual QVariant pluginValue(const QString& pluginId, const QString& key) const = 0;

    /// Write a plugin-scoped config value.
    /// Must be called from the main thread (single-writer rule).
    virtual void setPluginValue(const QString& pluginId, const QString& key, const QVariant& value) = 0;

    /// Flush config to disk. Returns true on success (round-2 F7 — callers can
    /// surface/retry failures instead of silently discarding the result).
    /// Must be called from the main thread.
    virtual bool save() = 0;
};

} // namespace oap
