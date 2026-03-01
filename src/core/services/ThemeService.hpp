#pragma once

#include "IThemeService.hpp"
#include <QObject>
#include <QColor>
#include <QMap>
#include <QString>
#include <QStringList>

namespace oap {

/// ThemeService loads theme definitions from YAML files and exposes colors
/// as Q_PROPERTYs for direct QML binding.
///
/// Theme YAML format:
///   id: theme-id
///   name: Display Name
///   font_family: "Font Name"
///   day:
///     background: "#rrggbb"
///     highlight: "#rrggbb"
///     ...
///   night:
///     background: "#rrggbb"
///     ...
///
/// Supports day/night mode switching.
class ThemeService : public QObject, public IThemeService {
    Q_OBJECT

    // Color properties — match old ThemeController names for smooth QML migration
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor highlightColor READ highlightColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor controlBackgroundColor READ controlBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor controlForegroundColor READ controlForegroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor normalFontColor READ normalFontColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor specialFontColor READ specialFontColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor descriptionFontColor READ descriptionFontColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor barBackgroundColor READ barBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor controlBoxBackgroundColor READ controlBoxBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor gaugeIndicatorColor READ gaugeIndicatorColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor iconColor READ iconColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor sideWidgetBackgroundColor READ sideWidgetBackgroundColor NOTIFY colorsChanged)
    Q_PROPERTY(QColor barShadowColor READ barShadowColor NOTIFY colorsChanged)

    Q_PROPERTY(bool nightMode READ nightMode WRITE setNightMode NOTIFY modeChanged)

    Q_PROPERTY(QStringList availableThemes READ availableThemes NOTIFY availableThemesChanged)
    Q_PROPERTY(QStringList availableThemeNames READ availableThemeNames NOTIFY availableThemesChanged)
    Q_PROPERTY(QString wallpaperSource READ wallpaperSource NOTIFY wallpaperChanged)
    Q_PROPERTY(QStringList availableWallpapers READ availableWallpapers NOTIFY availableWallpapersChanged)
    Q_PROPERTY(QStringList availableWallpaperNames READ availableWallpaperNames NOTIFY availableWallpapersChanged)

public:
    explicit ThemeService(QObject* parent = nullptr);

    /// Load a theme from a directory containing theme.yaml.
    /// Returns false if the file doesn't exist or can't be parsed.
    bool loadTheme(const QString& themeDirPath);

    /// Load from an explicit YAML file path.
    bool loadThemeFile(const QString& yamlPath);

    // IThemeService
    QString currentThemeId() const override;
    QColor color(const QString& name) const override;
    QString fontFamily() const override;
    QString iconPath(const QString& relativePath) const override;
    Q_INVOKABLE bool setTheme(const QString& themeId) override;

    /// Scan directories for theme subdirectories containing theme.yaml.
    /// User themes searched first (first seen ID wins for deduplication).
    void scanThemeDirectories(const QStringList& searchPaths);

    /// Available theme IDs (populated by scanThemeDirectories)
    QStringList availableThemes() const { return availableThemes_; }

    /// Available theme display names (parallel to availableThemes)
    QStringList availableThemeNames() const { return availableThemeNames_; }

    /// Current theme wallpaper as file:// URL, or empty string if none
    QString wallpaperSource() const { return wallpaperSource_; }

    /// Available wallpaper values: "" for None, file:// URLs for images
    QStringList availableWallpapers() const { return availableWallpapers_; }

    /// Available wallpaper display names: "None", "Default", "Ocean", etc.
    QStringList availableWallpaperNames() const { return availableWallpaperNames_; }

    /// Set wallpaper independently of theme selection
    Q_INVOKABLE void setWallpaper(const QString& wallpaperPath);

    // Day/Night mode
    bool nightMode() const { return nightMode_; }
    void setNightMode(bool night);
    Q_INVOKABLE void toggleMode();

    // Color accessors
    QColor backgroundColor() const { return activeColor("background"); }
    QColor highlightColor() const { return activeColor("highlight"); }
    QColor controlBackgroundColor() const { return activeColor("control_background"); }
    QColor controlForegroundColor() const { return activeColor("control_foreground"); }
    QColor normalFontColor() const { return activeColor("normal_font"); }
    QColor specialFontColor() const { return activeColor("special_font"); }
    QColor descriptionFontColor() const { return activeColor("description_font"); }
    QColor barBackgroundColor() const { return activeColor("bar_background"); }
    QColor controlBoxBackgroundColor() const { return activeColor("control_box_background"); }
    QColor gaugeIndicatorColor() const { return activeColor("gauge_indicator"); }
    QColor iconColor() const { return activeColor("icon"); }
    QColor sideWidgetBackgroundColor() const { return activeColor("side_widget_background"); }
    QColor barShadowColor() const { return activeColor("bar_shadow"); }

    /// Read-only access to color maps (for IPC export without signal side-effects)
    const QMap<QString, QColor>& dayColors() const { return dayColors_; }
    const QMap<QString, QColor>& nightColors() const { return nightColors_; }

signals:
    void colorsChanged();
    void modeChanged();
    void availableThemesChanged();
    void wallpaperChanged();
    void availableWallpapersChanged();

private:
    QColor activeColor(const QString& key) const;

    QString themeId_;
    QString themeName_;
    QString fontFamily_;
    QString themeDirPath_;
    bool nightMode_ = false;

    QMap<QString, QColor> dayColors_;
    QMap<QString, QColor> nightColors_;

    QStringList availableThemes_;
    QStringList availableThemeNames_;
    QMap<QString, QString> themeDirectories_; // theme ID -> directory path
    QString wallpaperSource_;

    QStringList availableWallpapers_;
    QStringList availableWallpaperNames_;

    void buildWallpaperList();
};

} // namespace oap
