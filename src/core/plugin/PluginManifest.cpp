#include "PluginManifest.hpp"
#include <yaml-cpp/yaml.h>
#include <QFileInfo>
#include <boost/log/trivial.hpp>

namespace oap {

bool PluginManifest::isValid() const
{
    return !id.isEmpty() && !name.isEmpty() && !version.isEmpty() && apiVersion > 0;
}

PluginManifest PluginManifest::fromFile(const QString& filePath)
{
    PluginManifest m;

    try {
        YAML::Node root = YAML::LoadFile(filePath.toStdString());

        m.id = QString::fromStdString(root["id"].as<std::string>(""));
        m.name = QString::fromStdString(root["name"].as<std::string>(""));
        m.version = QString::fromStdString(root["version"].as<std::string>(""));
        m.apiVersion = root["api_version"].as<int>(0);
        m.type = QString::fromStdString(root["type"].as<std::string>("full"));
        m.description = QString::fromStdString(root["description"].as<std::string>(""));
        m.author = QString::fromStdString(root["author"].as<std::string>(""));
        m.icon = QString::fromStdString(root["icon"].as<std::string>(""));

        // Required services (presence-only)
        if (root["requires"] && root["requires"]["services"]) {
            for (const auto& svc : root["requires"]["services"])
                m.requiredServices.append(QString::fromStdString(svc.as<std::string>()));
        }

        // Settings schema
        if (root["settings"] && root["settings"].IsSequence()) {
            for (const auto& s : root["settings"]) {
                PluginSettingDef def;
                def.key = QString::fromStdString(s["key"].as<std::string>(""));
                def.type = QString::fromStdString(s["type"].as<std::string>("string"));
                def.label = QString::fromStdString(s["label"].as<std::string>(""));

                if (s["default"]) {
                    if (def.type == "bool")
                        def.defaultValue = s["default"].as<bool>(false);
                    else if (def.type == "int")
                        def.defaultValue = s["default"].as<int>(0);
                    else
                        def.defaultValue = QString::fromStdString(s["default"].as<std::string>(""));
                }

                if (s["options"] && s["options"].IsSequence()) {
                    for (const auto& opt : s["options"])
                        def.options.append(QString::fromStdString(opt.as<std::string>()));
                }

                m.settings.append(def);
            }
        }

        // Nav strip config
        if (root["nav_strip"]) {
            m.navStripOrder = root["nav_strip"]["order"].as<int>(99);
            m.navStripVisible = root["nav_strip"]["visible"].as<bool>(true);
        }

        m.dirPath = QFileInfo(filePath).absolutePath();

    } catch (const YAML::Exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to parse plugin manifest " << filePath.toStdString()
                                  << ": " << e.what();
        return {};
    }

    return m;
}

} // namespace oap
