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
///     primary: "#rrggbb"
///     on-surface: "#rrggbb"
///     ...
///   night:
///     primary: "#rrggbb"
///     ...
///
/// Supports day/night mode switching.
/// Color tokens follow AA (Android Auto) wire protocol naming.
class ThemeService : public QObject, public IThemeService {
    Q_OBJECT

    // 16 base AA wire token color properties
    Q_PROPERTY(QColor primary READ primary NOTIFY colorsChanged)
    Q_PROPERTY(QColor onSurface READ onSurface NOTIFY colorsChanged)
    Q_PROPERTY(QColor surface READ surface NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceVariant READ surfaceVariant NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceContainer READ surfaceContainer NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceContainerLow READ surfaceContainerLow NOTIFY colorsChanged)
    Q_PROPERTY(QColor inverseSurface READ inverseSurface NOTIFY colorsChanged)
    Q_PROPERTY(QColor inverseOnSurface READ inverseOnSurface NOTIFY colorsChanged)
    Q_PROPERTY(QColor outline READ outline NOTIFY colorsChanged)
    Q_PROPERTY(QColor outlineVariant READ outlineVariant NOTIFY colorsChanged)
    Q_PROPERTY(QColor background READ background NOTIFY colorsChanged)
    Q_PROPERTY(QColor textPrimary READ textPrimary NOTIFY colorsChanged)
    Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY colorsChanged)
    Q_PROPERTY(QColor red READ red NOTIFY colorsChanged)
    Q_PROPERTY(QColor onRed READ onRed NOTIFY colorsChanged)
    Q_PROPERTY(QColor yellow READ yellow NOTIFY colorsChanged)
    Q_PROPERTY(QColor onYellow READ onYellow NOTIFY colorsChanged)

    // 5 derived color properties (computed, not from YAML)
    Q_PROPERTY(QColor scrim READ scrim NOTIFY colorsChanged)
    Q_PROPERTY(QColor pressed READ pressed NOTIFY colorsChanged)
    Q_PROPERTY(QColor barShadow READ barShadow NOTIFY colorsChanged)
    Q_PROPERTY(QColor success READ success NOTIFY colorsChanged)
    Q_PROPERTY(QColor onSuccess READ onSuccess NOTIFY colorsChanged)

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

    /// Apply AA theming tokens (ARGB uint32 values) with separate day/night maps.
    /// Always caches to connected-device YAML; only updates live colors when
    /// connected-device theme is active.
    void applyAATokens(const QMap<QString, uint32_t>& dayTokens,
                        const QMap<QString, uint32_t>& nightTokens) override;

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

    /// Available wallpaper override values: "" (theme default), "none", file:// URLs
    QStringList availableWallpapers() const { return availableWallpapers_; }

    /// Available wallpaper display names: "Theme Default", "None", custom files...
    QStringList availableWallpaperNames() const { return availableWallpaperNames_; }

    /// Set wallpaper override (empty = theme default, "none" = no wallpaper, file:// = custom)
    Q_INVOKABLE void setWallpaperOverride(const QString& override);

    /// Re-scan wallpaper directories and rebuild the available wallpapers list
    Q_INVOKABLE void refreshWallpapers();

    // Day/Night mode
    bool nightMode() const { return nightMode_; }
    void setNightMode(bool night);
    Q_INVOKABLE void toggleMode();

    // 16 base AA wire token color accessors (hyphenated keys match AA wire format)
    QColor primary() const { return activeColor("primary"); }
    QColor onSurface() const { return activeColor("on-surface"); }
    QColor surface() const { return activeColor("surface"); }
    QColor surfaceVariant() const { return activeColor("surface-variant"); }
    QColor surfaceContainer() const { return activeColor("surface-container"); }
    QColor surfaceContainerLow() const { return activeColor("surface-container-low"); }
    QColor inverseSurface() const { return activeColor("inverse-surface"); }
    QColor inverseOnSurface() const { return activeColor("inverse-on-surface"); }
    QColor outline() const { return activeColor("outline"); }
    QColor outlineVariant() const;
    QColor background() const { return activeColor("background"); }
    QColor textPrimary() const { return activeColor("text-primary"); }
    QColor textSecondary() const { return activeColor("text-secondary"); }
    QColor red() const { return activeColor("red"); }
    QColor onRed() const { return activeColor("on-red"); }
    QColor yellow() const { return activeColor("yellow"); }
    QColor onYellow() const { return activeColor("on-yellow"); }

    // 5 derived color accessors (computed, not from YAML)
    QColor scrim() const;
    QColor pressed() const;
    QColor barShadow() const;
    QColor success() const;
    QColor onSuccess() const;

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
    QString wallpaperOverride_;

    QStringList availableWallpapers_;
    QStringList availableWallpaperNames_;

    void buildWallpaperList();
    void resolveWallpaper();
    void persistConnectedDeviceTheme();
};

} // namespace oap
