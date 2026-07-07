#include "core/widget/WebWidgetManifest.hpp"

#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>
#include <yaml-cpp/yaml.h>

namespace oap {

bool WebWidgetManifest::isValid() const
{
    // id is used verbatim as a URL path segment and resolver key.
    static const QRegularExpression safeId(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*\\z"));
    if (id.isEmpty() || name.isEmpty() || !safeId.match(id).hasMatch())
        return false;
    if (entry.isEmpty() || entry.startsWith(u'/') || entry.contains(QStringLiteral("..")))
        return false;
    if (minCols < 1 || minRows < 1 || maxCols < minCols || maxRows < minRows)
        return false;
    if (defaultCols < minCols || defaultCols > maxCols
        || defaultRows < minRows || defaultRows > maxRows)
        return false;
    return true;
}

WebWidgetManifest WebWidgetManifest::fromFile(const QString& filePath)
{
    WebWidgetManifest m;
    try {
        YAML::Node root = YAML::LoadFile(filePath.toStdString());

        m.id = QString::fromStdString(root["id"].as<std::string>(""));
        m.name = QString::fromStdString(root["name"].as<std::string>(""));
        m.entry = QString::fromStdString(root["entry"].as<std::string>("index.html"));
        m.category = QString::fromStdString(root["category"].as<std::string>("status"));
        m.description = QString::fromStdString(root["description"].as<std::string>(""));
        m.icon = QString::fromStdString(root["icon"].as<std::string>(""));

        if (root["size"] && root["size"].IsMap()) {
            const auto& s = root["size"];
            m.minCols = s["minCols"].as<int>(m.minCols);
            m.minRows = s["minRows"].as<int>(m.minRows);
            m.maxCols = s["maxCols"].as<int>(m.maxCols);
            m.maxRows = s["maxRows"].as<int>(m.maxRows);
            m.defaultCols = s["defaultCols"].as<int>(m.defaultCols);
            m.defaultRows = s["defaultRows"].as<int>(m.defaultRows);
        }
        m.dirPath = QFileInfo(filePath).absolutePath();
    } catch (const YAML::Exception& e) {
        qWarning() << "WebWidgetManifest: failed to parse" << filePath << ":" << e.what();
        return {};
    }
    return m;
}

} // namespace oap
