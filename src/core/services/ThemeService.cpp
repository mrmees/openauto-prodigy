#include "ThemeService.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <yaml-cpp/yaml.h>

namespace oap {

ThemeService::ThemeService(QObject* parent)
    : QObject(parent)
{
}

// Migration map: v1.0.0 theme keys → v2.0.0 AA wire token keys (hyphenated)
static const QMap<QString, QString> legacyKeyMap = {
    {"background", "background"},
    {"highlight", "primary"},
    {"control_background", "surface-variant"},
    {"control_foreground", "on-surface"},
    {"normal_font", "text-primary"},
    {"description_font", "text-secondary"},
    {"bar_background", "surface-container-low"},
    {"control_box_background", "surface"},
    {"divider", "outline-variant"},
    {"highlight_font", "inverse-surface"},
    // Keys that map to new tokens with no direct old equivalent
    // (special_font, gauge_indicator, icon, side_widget_background → dropped or derived)
};

static QMap<QString, QColor> loadColorMap(const YAML::Node& node, bool migrate)
{
    QMap<QString, QColor> colors;
    if (!node.IsMap()) return colors;

    for (auto it = node.begin(); it != node.end(); ++it) {
        QString key = QString::fromStdString(it->first.as<std::string>());
        QString value = QString::fromStdString(it->second.as<std::string>());
        QColor c(value);
        if (!c.isValid()) continue;

        if (migrate) {
            auto mapped = legacyKeyMap.find(key);
            if (mapped != legacyKeyMap.end())
                colors[mapped.value()] = c;
            // Also populate keys that need special handling
            if (key == "highlight_font")
                colors["inverse-on-surface"] = QColor("#ffffff");  // was always white-on-highlight
            if (key == "divider")
                colors["outline"] = QColor("#808080");  // no old equivalent
            if (key == "highlight") {
                // old highlight was used for red/yellow indicators too — provide defaults
                if (!colors.contains("red")) colors["red"] = QColor("#cc4444");
                if (!colors.contains("on-red")) colors["on-red"] = QColor("#ffffff");
                if (!colors.contains("yellow")) colors["yellow"] = QColor("#FF9800");
                if (!colors.contains("on-yellow")) colors["on-yellow"] = QColor("#000000");
            }
        } else {
            colors[key] = c;
        }
    }
    return colors;
}

bool ThemeService::loadTheme(const QString& themeDirPath)
{
    QString yamlPath = QDir(themeDirPath).filePath("theme.yaml");
    themeDirPath_ = themeDirPath;
    return loadThemeFile(yamlPath);
}

bool ThemeService::loadThemeFile(const QString& yamlPath)
{
    if (!QFile::exists(yamlPath))
        return false;

    try {
        YAML::Node root = YAML::LoadFile(yamlPath.toStdString());

        themeId_ = QString::fromStdString(root["id"].as<std::string>(""));
        themeName_ = QString::fromStdString(root["name"].as<std::string>(""));
        fontFamily_ = QString::fromStdString(root["font_family"].as<std::string>("Lato"));

        // Detect v1.0.0 themes by checking for old key names (e.g. "highlight")
        bool needsMigration = false;
        if (root["day"] && root["day"]["highlight"])
            needsMigration = true;

        dayColors_ = loadColorMap(root["day"], needsMigration);
        nightColors_ = loadColorMap(root["night"], needsMigration);

        if (needsMigration)
            qInfo() << "[Theme] Migrated v1 theme keys for:" << themeId_;

        emit colorsChanged();
        return true;
    } catch (const YAML::Exception&) {
        return false;
    }
}

QString ThemeService::currentThemeId() const
{
    return themeId_;
}

QColor ThemeService::color(const QString& name) const
{
    return activeColor(name);
}

QString ThemeService::fontFamily() const
{
    return fontFamily_;
}

QString ThemeService::iconPath(const QString& relativePath) const
{
    if (themeDirPath_.isEmpty())
        return {};
    QString path = QDir(themeDirPath_).filePath("icons/" + relativePath);
    if (QFile::exists(path))
        return path;
    return {};
}

void ThemeService::scanThemeDirectories(const QStringList& searchPaths)
{
    availableThemes_.clear();
    availableThemeNames_.clear();
    themeDirectories_.clear();

    for (const QString& searchPath : searchPaths) {
        QDir dir(searchPath);
        if (!dir.exists()) continue;

        const auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& entry : entries) {
            QString themeDir = dir.absoluteFilePath(entry);
            QString yamlPath = QDir(themeDir).filePath("theme.yaml");

            if (!QFile::exists(yamlPath)) continue;

            try {
                YAML::Node root = YAML::LoadFile(yamlPath.toStdString());
                QString id = QString::fromStdString(root["id"].as<std::string>(""));
                QString name = QString::fromStdString(root["name"].as<std::string>(""));

                if (id.isEmpty()) continue;

                // First seen ID wins (user themes searched before bundled)
                if (themeDirectories_.contains(id)) continue;

                availableThemes_.append(id);
                availableThemeNames_.append(name);
                themeDirectories_[id] = themeDir;
            } catch (const YAML::Exception&) {
                continue;
            }
        }
    }

    emit availableThemesChanged();
    buildWallpaperList();
}

bool ThemeService::setTheme(const QString& themeId)
{
    auto it = themeDirectories_.find(themeId);
    if (it == themeDirectories_.end())
        return false;

    if (!loadTheme(it.value()))
        return false;

    resolveWallpaper();
    return true;
}

void ThemeService::setNightMode(bool night)
{
    if (nightMode_ == night) return;
    nightMode_ = night;
    emit modeChanged();
    emit colorsChanged();
}

void ThemeService::toggleMode()
{
    setNightMode(!nightMode_);
}

void ThemeService::buildWallpaperList()
{
    availableWallpapers_.clear();
    availableWallpaperNames_.clear();

    // Fixed entries: "Theme Default" and "None"
    availableWallpapers_.append(QString());            // "" = theme default
    availableWallpaperNames_.append(QStringLiteral("Theme Default"));

    availableWallpapers_.append(QStringLiteral("none"));
    availableWallpaperNames_.append(QStringLiteral("None"));

    // Scan ~/.openauto/wallpapers/ for custom images
    QString userWpDir = QDir::homePath() + "/.openauto/wallpapers";
    QDir wpDir(userWpDir);
    if (wpDir.exists()) {
        QStringList filters;
        filters << "*.jpg" << "*.jpeg" << "*.png" << "*.webp";
        auto entries = wpDir.entryInfoList(filters, QDir::Files, QDir::Name);
        for (const QFileInfo& fi : entries) {
            QString fileUrl = QStringLiteral("file://") + fi.absoluteFilePath();
            availableWallpapers_.append(fileUrl);
            // Display name: strip extension, replace underscores/hyphens with spaces
            QString displayName = fi.completeBaseName();
            displayName.replace('_', ' ');
            displayName.replace('-', ' ');
            // Title case: capitalize first letter of each word
            QStringList words = displayName.split(' ', Qt::SkipEmptyParts);
            for (QString& word : words) {
                if (!word.isEmpty())
                    word[0] = word[0].toUpper();
            }
            availableWallpaperNames_.append(words.join(' '));
        }
    }

    emit availableWallpapersChanged();
}

void ThemeService::refreshWallpapers()
{
    buildWallpaperList();
}

void ThemeService::setWallpaperOverride(const QString& override)
{
    if (wallpaperOverride_ == override) return;
    wallpaperOverride_ = override;
    resolveWallpaper();
}

void ThemeService::resolveWallpaper()
{
    QString oldWp = wallpaperSource_;

    if (wallpaperOverride_ == "none") {
        wallpaperSource_.clear();
    } else if (!wallpaperOverride_.isEmpty()) {
        wallpaperSource_ = wallpaperOverride_;  // custom image
    } else {
        // Theme default — look for wallpaper.jpg in current theme dir
        if (!themeDirPath_.isEmpty()) {
            QString wpPath = QDir(themeDirPath_).filePath("wallpaper.jpg");
            if (QFile::exists(wpPath))
                wallpaperSource_ = "file://" + QFileInfo(wpPath).absoluteFilePath();
            else
                wallpaperSource_.clear();
        } else {
            wallpaperSource_.clear();
        }
    }

    if (wallpaperSource_ != oldWp)
        emit wallpaperChanged();
}

// Known AA token keys (the 16 base tokens used in our theme system, hyphenated)
static const QSet<QString> knownAATokenKeys = {
    "primary", "on-surface", "surface", "surface-variant",
    "surface-container", "surface-container-low", "inverse-surface", "inverse-on-surface",
    "outline", "outline-variant", "background", "text-primary",
    "text-secondary", "red", "on-red", "yellow", "on-yellow"
};

void ThemeService::applyAATokens(const QMap<QString, uint32_t>& dayTokens,
                                  const QMap<QString, uint32_t>& nightTokens)
{
    // Build color maps from ARGB tokens, filtering through known keys
    QMap<QString, QColor> newDayColors;
    for (auto it = dayTokens.constBegin(); it != dayTokens.constEnd(); ++it) {
        if (knownAATokenKeys.contains(it.key()))
            newDayColors[it.key()] = QColor::fromRgba(it.value());
    }

    QMap<QString, QColor> newNightColors;
    for (auto it = nightTokens.constBegin(); it != nightTokens.constEnd(); ++it) {
        if (knownAATokenKeys.contains(it.key()))
            newNightColors[it.key()] = QColor::fromRgba(it.value());
    }

    // Always load connected-device theme colors for caching
    // (even if it's not the active theme)
    QMap<QString, QColor> cacheDayColors = dayColors_;
    QMap<QString, QColor> cacheNightColors = nightColors_;

    // If connected-device is NOT active, load its YAML to get current cached colors
    if (themeId_ != "connected-device") {
        QString themeDir;
        auto dirIt = themeDirectories_.find("connected-device");
        if (dirIt != themeDirectories_.end()) {
            themeDir = dirIt.value();
        } else {
            themeDir = QDir::homePath() + "/.openauto/themes/connected-device";
        }

        QString yamlPath = QDir(themeDir).filePath("theme.yaml");
        if (QFile::exists(yamlPath)) {
            try {
                YAML::Node root = YAML::LoadFile(yamlPath.toStdString());
                cacheDayColors = loadColorMap(root["day"], false);
                cacheNightColors = loadColorMap(root["night"], false);
            } catch (const YAML::Exception&) {
                // Use empty maps if can't load
                cacheDayColors.clear();
                cacheNightColors.clear();
            }
        } else {
            cacheDayColors.clear();
            cacheNightColors.clear();
        }
    }

    // Merge new tokens into cache maps
    for (auto it = newDayColors.constBegin(); it != newDayColors.constEnd(); ++it)
        cacheDayColors[it.key()] = it.value();
    for (auto it = newNightColors.constBegin(); it != newNightColors.constEnd(); ++it)
        cacheNightColors[it.key()] = it.value();

    // Always persist to connected-device YAML
    // Temporarily swap colors for persistence, then restore if not active
    QMap<QString, QColor> savedDay = dayColors_;
    QMap<QString, QColor> savedNight = nightColors_;
    dayColors_ = cacheDayColors;
    nightColors_ = cacheNightColors;
    persistConnectedDeviceTheme();

    if (themeId_ == "connected-device") {
        // Keep updated colors live
        qInfo() << "[Theme] Applied" << newDayColors.size() << "day +"
                << newNightColors.size() << "night tokens (live)";
        emit colorsChanged();
    } else {
        // Restore original colors — live theme unchanged
        dayColors_ = savedDay;
        nightColors_ = savedNight;
        qInfo() << "[Theme] Cached" << newDayColors.size() << "day +"
                << newNightColors.size() << "night tokens (theme not active)";
    }
}

void ThemeService::persistConnectedDeviceTheme()
{
    // Find connected-device theme directory
    QString themeDir;
    auto dirIt = themeDirectories_.find("connected-device");
    if (dirIt != themeDirectories_.end()) {
        themeDir = dirIt.value();
    } else {
        themeDir = QDir::homePath() + "/.openauto/themes/connected-device";
    }

    QDir().mkpath(themeDir);
    QString yamlPath = QDir(themeDir).filePath("theme.yaml");

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "id" << YAML::Value << "connected-device";
    out << YAML::Key << "name" << YAML::Value << "Connected Device";
    out << YAML::Key << "version" << YAML::Value << "2.0.0";
    out << YAML::Key << "font_family" << YAML::Value << fontFamily_.toStdString();

    // Write day colors
    out << YAML::Key << "day" << YAML::Value << YAML::BeginMap;
    for (auto it = dayColors_.constBegin(); it != dayColors_.constEnd(); ++it) {
        out << YAML::Key << it.key().toStdString()
            << YAML::Value << it.value().name(QColor::HexArgb).toStdString();
    }
    out << YAML::EndMap;

    // Write night colors
    out << YAML::Key << "night" << YAML::Value << YAML::BeginMap;
    for (auto it = nightColors_.constBegin(); it != nightColors_.constEnd(); ++it) {
        out << YAML::Key << it.key().toStdString()
            << YAML::Value << it.value().name(QColor::HexArgb).toStdString();
    }
    out << YAML::EndMap;
    out << YAML::EndMap;

    QFile file(yamlPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(out.c_str());
        file.close();
    }
}

QColor ThemeService::outlineVariant() const
{
    QColor c = activeColor("outline-variant");
    if (c == QColor(Qt::transparent))
        return QColor(255, 255, 255, 38);  // ~15% white fallback
    return c;
}

QColor ThemeService::scrim() const
{
    return QColor(0, 0, 0, 180);
}

QColor ThemeService::pressed() const
{
    QColor c = activeColor("on-surface");
    c.setAlpha(26);
    return c;
}

QColor ThemeService::barShadow() const
{
    return QColor(0, 0, 0, 128);
}

QColor ThemeService::success() const
{
    return QColor("#4CAF50");
}

QColor ThemeService::onSuccess() const
{
    return QColor("#FFFFFF");
}

QColor ThemeService::activeColor(const QString& key) const
{
    const auto& colors = nightMode_ ? nightColors_ : dayColors_;
    auto it = colors.find(key);
    if (it != colors.end())
        return it.value();

    // Fall back to day colors if night doesn't have the key
    if (nightMode_) {
        auto dayIt = dayColors_.find(key);
        if (dayIt != dayColors_.end())
            return dayIt.value();
    }

    return QColor(Qt::transparent);
}

} // namespace oap
