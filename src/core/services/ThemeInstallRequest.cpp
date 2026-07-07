#include "core/services/ThemeInstallRequest.hpp"

#include <QFile>
#include <QFileInfo>

namespace oap {

// Companion sends camelCase M3 role names; theme.yaml uses hyphenated keys.
// Insert a hyphen before each uppercase letter and lowercase it (matches the
// legacy CompanionListenerService::applyReceivedTheme conversion exactly).
static QString camelToHyphen(const QString& in) {
    QString out;
    for (QChar ch : in) {
        if (ch.isUpper()) { out += '-'; out += ch.toLower(); }
        else              { out += ch; }
    }
    return out;
}

static bool parseColorMap(const QVariantMap& in, QMap<QString, QColor>& out, QString& err) {
    if (in.isEmpty()) { err = QStringLiteral("empty color scheme"); return false; }
    for (auto it = in.constBegin(); it != in.constEnd(); ++it) {
        const QColor c(it.value().toString());
        if (!c.isValid()) { err = QStringLiteral("invalid color for '%1'").arg(it.key()); return false; }
        out.insert(camelToHyphen(it.key()), c);
    }
    return true;
}

ThemeInstallParseResult parseThemeInstall(const QVariantMap& data,
                                          const QString& allowedWallpaperDir,
                                          qint64 maxWallpaperBytes) {
    ThemeInstallParseResult r;
    ThemeInstallRequest& req = r.request;

    req.name = data.value(QStringLiteral("name")).toString().trimmed();
    if (req.name.isEmpty() || req.name.size() > 64) {
        r.error = QStringLiteral("name must be 1-64 characters");
        return r;
    }
    req.seed = data.value(QStringLiteral("seed")).toString();

    QString err;
    if (!parseColorMap(data.value(QStringLiteral("light")).toMap(), req.dayColors, err)) {
        r.error = QStringLiteral("light: ") + err;
        return r;
    }
    if (!parseColorMap(data.value(QStringLiteral("dark")).toMap(), req.nightColors, err)) {
        r.error = QStringLiteral("dark: ") + err;
        return r;
    }

    const QString wp = data.value(QStringLiteral("wallpaper_path")).toString();
    if (!wp.isEmpty()) {
        const QFileInfo fi(wp);
        const QString canon = fi.canonicalFilePath();
        const QString allowedCanon = QFileInfo(allowedWallpaperDir).canonicalFilePath();
        // Strict containment: must be a regular file whose canonical path lives
        // directly under the canonical allowed dir (trailing '/' blocks sibling-prefix escapes).
        if (canon.isEmpty() || allowedCanon.isEmpty()
            || !canon.startsWith(allowedCanon + QLatin1Char('/'))
            || !fi.isFile()) {
            r.error = QStringLiteral("wallpaper path is invalid");
            return r;
        }
        QFile f(canon);
        if (!f.open(QIODevice::ReadOnly)) {
            r.error = QStringLiteral("cannot read wallpaper");
            return r;
        }
        const QByteArray bytes = f.readAll();
        f.close();
        if (bytes.size() > maxWallpaperBytes) {
            r.error = QStringLiteral("wallpaper too large");
            return r;
        }
        if (bytes.size() < 3
            || static_cast<unsigned char>(bytes[0]) != 0xFF
            || static_cast<unsigned char>(bytes[1]) != 0xD8
            || static_cast<unsigned char>(bytes[2]) != 0xFF) {
            r.error = QStringLiteral("wallpaper is not a JPEG");
            return r;
        }
        req.wallpaperJpeg = bytes;
    }

    r.ok = true;
    return r;
}

} // namespace oap
