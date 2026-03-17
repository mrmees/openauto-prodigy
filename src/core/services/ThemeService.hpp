#pragma once

#include "IThemeService.hpp"
#include "IConfigService.hpp"
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
/// Color tokens follow M3 (Material Design 3) naming with AA extensions.
class ThemeService : public QObject, public IThemeService {
    Q_OBJECT

    // --- Primary group ---
    Q_PROPERTY(QColor primary READ primary NOTIFY colorsChanged)
    Q_PROPERTY(QColor onPrimary READ onPrimary NOTIFY colorsChanged)
    Q_PROPERTY(QColor primaryContainer READ primaryContainer NOTIFY colorsChanged)
    Q_PROPERTY(QColor onPrimaryContainer READ onPrimaryContainer NOTIFY colorsChanged)

    // --- Secondary group ---
    Q_PROPERTY(QColor secondary READ secondary NOTIFY colorsChanged)
    Q_PROPERTY(QColor onSecondary READ onSecondary NOTIFY colorsChanged)
    Q_PROPERTY(QColor secondaryContainer READ secondaryContainer NOTIFY colorsChanged)
    Q_PROPERTY(QColor onSecondaryContainer READ onSecondaryContainer NOTIFY colorsChanged)

    // --- Tertiary group ---
    Q_PROPERTY(QColor tertiary READ tertiary NOTIFY colorsChanged)
    Q_PROPERTY(QColor onTertiary READ onTertiary NOTIFY colorsChanged)
    Q_PROPERTY(QColor tertiaryContainer READ tertiaryContainer NOTIFY colorsChanged)
    Q_PROPERTY(QColor onTertiaryContainer READ onTertiaryContainer NOTIFY colorsChanged)

    // --- Error group ---
    Q_PROPERTY(QColor error READ error NOTIFY colorsChanged)
    Q_PROPERTY(QColor onError READ onError NOTIFY colorsChanged)
    Q_PROPERTY(QColor errorContainer READ errorContainer NOTIFY colorsChanged)
    Q_PROPERTY(QColor onErrorContainer READ onErrorContainer NOTIFY colorsChanged)

    // --- Background & Surface ---
    Q_PROPERTY(QColor background READ background NOTIFY colorsChanged)
    Q_PROPERTY(QColor onBackground READ onBackground NOTIFY colorsChanged)
    Q_PROPERTY(QColor surface READ surface NOTIFY colorsChanged)
    Q_PROPERTY(QColor onSurface READ onSurface NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceVariant READ surfaceVariant NOTIFY colorsChanged)
    Q_PROPERTY(QColor onSurfaceVariant READ onSurfaceVariant NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceDim READ surfaceDim NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceBright READ surfaceBright NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceContainerLowest READ surfaceContainerLowest NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceContainerLow READ surfaceContainerLow NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceContainer READ surfaceContainer NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceContainerHigh READ surfaceContainerHigh NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceContainerHighest READ surfaceContainerHighest NOTIFY colorsChanged)

    // --- Outline ---
    Q_PROPERTY(QColor outline READ outline NOTIFY colorsChanged)
    Q_PROPERTY(QColor outlineVariant READ outlineVariant NOTIFY colorsChanged)

    // --- Inverse ---
    Q_PROPERTY(QColor inverseSurface READ inverseSurface NOTIFY colorsChanged)
    Q_PROPERTY(QColor inverseOnSurface READ inverseOnSurface NOTIFY colorsChanged)
    Q_PROPERTY(QColor inversePrimary READ inversePrimary NOTIFY colorsChanged)

    // --- Utility ---
    Q_PROPERTY(QColor scrim READ scrim NOTIFY colorsChanged)
    Q_PROPERTY(QColor shadow READ shadow NOTIFY colorsChanged)

    // --- Derived color properties (computed, not from YAML) ---
    Q_PROPERTY(QColor success READ success NOTIFY colorsChanged)
    Q_PROPERTY(QColor onSuccess READ onSuccess NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceTintHigh READ surfaceTintHigh NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceTintHighest READ surfaceTintHighest NOTIFY colorsChanged)
    Q_PROPERTY(QColor warning READ warning NOTIFY colorsChanged)
    Q_PROPERTY(QColor onWarning READ onWarning NOTIFY colorsChanged)

    Q_PROPERTY(bool nightMode READ nightMode WRITE setNightMode NOTIFY modeChanged)
    Q_PROPERTY(bool forceDarkMode READ forceDarkMode WRITE setForceDarkMode NOTIFY forceDarkModeChanged)

    Q_PROPERTY(QString currentThemeId READ currentThemeId NOTIFY currentThemeIdChanged)
    Q_PROPERTY(QStringList availableThemes READ availableThemes NOTIFY availableThemesChanged)
    Q_PROPERTY(QStringList availableThemeNames READ availableThemeNames NOTIFY availableThemesChanged)
    Q_PROPERTY(QString wallpaperSource READ wallpaperSource NOTIFY wallpaperChanged)
    Q_PROPERTY(QStringList availableWallpapers READ availableWallpapers NOTIFY availableWallpapersChanged)
    Q_PROPERTY(QStringList availableWallpaperNames READ availableWallpaperNames NOTIFY availableWallpapersChanged)

public:
    explicit ThemeService(QObject* parent = nullptr);

    /// Set the config service for persisting theme selection.
    void setConfigService(IConfigService* svc) { configService_ = svc; }

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
    /// Stores search paths for later rescan.
    void scanThemeDirectories(const QStringList& searchPaths);

    /// Import a companion-app theme: creates named theme dir, writes YAML + wallpaper, auto-switches.
    /// Returns true on success.
    bool importCompanionTheme(const QString& name, const QString& seed,
                              const QMap<QString, QColor>& dayColors,
                              const QMap<QString, QColor>& nightColors,
                              const QByteArray& wallpaperJpeg);

    /// Delete a user theme. Refuses to delete bundled themes (only themes under ~/.openauto/themes/).
    /// If the deleted theme is active, switches to "default" first.
    Q_INVOKABLE bool deleteTheme(const QString& themeId);

    /// Returns true if themeId is a user/companion theme (under ~/.openauto/themes/), false if bundled.
    Q_INVOKABLE bool isUserTheme(const QString& themeId) const;

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
    bool nightMode() const;
    void setNightMode(bool night);
    Q_INVOKABLE void toggleMode();

    // Force dark mode (HU override — AA uses realNightMode)
    bool forceDarkMode() const { return forceDarkMode_; }
    void setForceDarkMode(bool force);
    bool realNightMode() const { return nightMode_; }

    // --- Primary group ---
    QColor primary() const { return activeColor("primary"); }
    QColor onPrimary() const { return activeColor("on-primary"); }
    QColor primaryContainer() const { return activeColor("primary-container"); }
    QColor onPrimaryContainer() const { return activeColor("on-primary-container"); }

    // --- Secondary group ---
    QColor secondary() const { return activeColor("secondary"); }
    QColor onSecondary() const { return activeColor("on-secondary"); }
    QColor secondaryContainer() const { return activeColor("secondary-container"); }
    QColor onSecondaryContainer() const { return activeColor("on-secondary-container"); }

    // --- Tertiary group ---
    QColor tertiary() const { return activeColor("tertiary"); }
    QColor onTertiary() const { return activeColor("on-tertiary"); }
    QColor tertiaryContainer() const { return activeColor("tertiary-container"); }
    QColor onTertiaryContainer() const { return activeColor("on-tertiary-container"); }

    // --- Error group ---
    QColor error() const { return activeColor("error"); }
    QColor onError() const { return activeColor("on-error"); }
    QColor errorContainer() const { return activeColor("error-container"); }
    QColor onErrorContainer() const { return activeColor("on-error-container"); }

    // --- Background & Surface ---
    QColor background() const { return activeColor("background"); }
    QColor onBackground() const { return activeColor("on-background"); }
    QColor onSurface() const { return activeColor("on-surface"); }
    QColor surface() const { return activeColor("surface"); }
    QColor surfaceVariant() const { return activeColor("surface-variant"); }
    QColor onSurfaceVariant() const { return activeColor("on-surface-variant"); }
    QColor surfaceDim() const { return activeColor("surface-dim"); }
    QColor surfaceBright() const { return activeColor("surface-bright"); }
    QColor surfaceContainerLowest() const { return activeColor("surface-container-lowest"); }
    QColor surfaceContainerLow() const { return activeColor("surface-container-low"); }
    QColor surfaceContainer() const { return activeColor("surface-container"); }
    QColor surfaceContainerHigh() const { return activeColor("surface-container-high"); }
    QColor surfaceContainerHighest() const { return activeColor("surface-container-highest"); }

    // --- Outline ---
    QColor outline() const { return activeColor("outline"); }
    QColor outlineVariant() const;

    // --- Inverse ---
    QColor inverseSurface() const { return activeColor("inverse-surface"); }
    QColor inverseOnSurface() const { return activeColor("inverse-on-surface"); }
    QColor inversePrimary() const { return activeColor("inverse-primary"); }

    // --- Utility ---
    QColor scrim() const;
    QColor shadow() const { return activeColor("shadow"); }

    // --- Derived (computed, not from YAML) ---
    QColor success() const;
    QColor onSuccess() const;
    QColor surfaceTintHigh() const;
    QColor surfaceTintHighest() const;
    QColor warning() const;
    QColor onWarning() const;

    /// Read-only access to color maps (for IPC export without signal side-effects)
    const QMap<QString, QColor>& dayColors() const { return dayColors_; }
    const QMap<QString, QColor>& nightColors() const { return nightColors_; }

signals:
    void colorsChanged();
    void modeChanged();
    void forceDarkModeChanged();
    void currentThemeIdChanged();
    void availableThemesChanged();
    void wallpaperChanged();
    void availableWallpapersChanged();

private:
    QColor activeColor(const QString& key) const;
    static bool isAccentRole(const QString& key);
    void rescanThemes();

    IConfigService* configService_ = nullptr;
    QString themeId_;
    QString themeName_;
    QString fontFamily_;
    QString themeDirPath_;
    bool nightMode_ = false;
    bool forceDarkMode_ = false;

    QMap<QString, QColor> dayColors_;
    QMap<QString, QColor> nightColors_;

    QStringList availableThemes_;
    QStringList availableThemeNames_;
    QMap<QString, QString> themeDirectories_; // theme ID -> directory path
    QStringList searchPaths_;                 // stored for rescan after import/delete
    QString wallpaperSource_;
    QString wallpaperOverride_;

    QStringList availableWallpapers_;
    QStringList availableWallpaperNames_;

    void buildWallpaperList();
    void resolveWallpaper();
    void persistConnectedDeviceTheme();
};

} // namespace oap
