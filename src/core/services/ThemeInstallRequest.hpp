#pragma once

#include <QByteArray>
#include <QColor>
#include <QMap>
#include <QString>
#include <QVariantMap>

namespace oap {

/// Validated, normalized inputs for a theme install (ready for ThemeService::importCompanionTheme).
struct ThemeInstallRequest {
    QString name;
    QString seed;
    QMap<QString, QColor> dayColors;    // from manifest "light", keys converted camelCase -> hyphenated
    QMap<QString, QColor> nightColors;  // from manifest "dark"
    QByteArray wallpaperJpeg;           // empty when no wallpaper supplied
};

struct ThemeInstallParseResult {
    bool ok = false;
    QString error;               // human-readable reason when !ok
    ThemeInstallRequest request; // populated when ok
};

/// Validate + normalize an `install_theme` IPC payload.
///
/// `data` keys: "name" (1-64 chars), "seed" (optional), "light"/"dark"
/// (non-empty maps of camelCase-role -> hex color), "wallpaper_path" (optional).
/// When "wallpaper_path" is present it must canonicalize strictly under
/// `allowedWallpaperDir`, be a regular file, decode to <= maxWallpaperBytes,
/// and start with the JPEG magic bytes FF D8 FF.
ThemeInstallParseResult parseThemeInstall(const QVariantMap& data,
                                          const QString& allowedWallpaperDir,
                                          qint64 maxWallpaperBytes = 5 * 1024 * 1024);

} // namespace oap
