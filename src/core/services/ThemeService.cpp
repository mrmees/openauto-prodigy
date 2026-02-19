#include "ThemeService.hpp"
#include <QDir>
#include <QFile>
#include <yaml-cpp/yaml.h>

namespace oap {

ThemeService::ThemeService(QObject* parent)
    : QObject(parent)
{
}

static QMap<QString, QColor> loadColorMap(const YAML::Node& node)
{
    QMap<QString, QColor> colors;
    if (!node.IsMap()) return colors;

    for (auto it = node.begin(); it != node.end(); ++it) {
        QString key = QString::fromStdString(it->first.as<std::string>());
        QString value = QString::fromStdString(it->second.as<std::string>());
        QColor c(value);
        if (c.isValid())
            colors[key] = c;
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

        dayColors_ = loadColorMap(root["day"]);
        nightColors_ = loadColorMap(root["night"]);

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

bool ThemeService::setTheme(const QString& themeId)
{
    Q_UNUSED(themeId);
    // Multi-theme directory scanning not yet implemented.
    // Currently only supports the loaded theme.
    return false;
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
