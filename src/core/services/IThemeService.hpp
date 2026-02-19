#pragma once

#include <QObject>
#include <QString>
#include <QColor>

namespace oap {

class IThemeService {
public:
    virtual ~IThemeService() = default;

    /// Current theme identifier (e.g., "default", "dark-blue").
    /// Thread-safe.
    virtual QString currentThemeId() const = 0;

    /// Look up a color by semantic name (e.g., "background.primary", "accent.main").
    /// Returns transparent color if name not found.
    /// Thread-safe.
    virtual QColor color(const QString& name) const = 0;

    /// Primary font family name.
    /// Thread-safe.
    virtual QString fontFamily() const = 0;

    /// Resolve a theme-relative icon path to an absolute file URL.
    /// Returns empty URL if icon not found.
    /// Thread-safe.
    virtual QString iconPath(const QString& relativePath) const = 0;

    /// Set the active theme by ID. Loads theme YAML and emits changes.
    /// Must be called from the main thread.
    /// Returns false if theme not found.
    virtual bool setTheme(const QString& themeId) = 0;
};

} // namespace oap
