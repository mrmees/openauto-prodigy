#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <yaml-cpp/yaml.h>

namespace oap {

class YamlConfig {
public:
    YamlConfig();

    void load(const QString& filePath);
    void save(const QString& filePath) const;

    // Hardware profile
    QString hardwareProfile() const;
    void setHardwareProfile(const QString& v);

    // Display
    int displayBrightness() const;
    void setDisplayBrightness(int v);
    QString theme() const;
    void setTheme(const QString& v);

    // Connection
    bool autoConnectAA() const;
    void setAutoConnectAA(bool v);
    QString wifiSsid() const;
    void setWifiSsid(const QString& v);
    QString wifiPassword() const;
    void setWifiPassword(const QString& v);
    uint16_t tcpPort() const;
    void setTcpPort(uint16_t v);

    // Audio
    int masterVolume() const;
    void setMasterVolume(int v);

    // Video
    int videoFps() const;
    void setVideoFps(int v);

    // Plugins
    QStringList enabledPlugins() const;
    void setEnabledPlugins(const QStringList& plugins);

    // Plugin-scoped config — single source of truth in root_ YAML tree.
    // NO separate cache. All reads/writes go through root_["plugin_config"].
    QVariant pluginValue(const QString& pluginId, const QString& key) const;
    void setPluginValue(const QString& pluginId, const QString& key, const QVariant& value);

private:
    YAML::Node root_;  // Single source of truth — NO shadow state

    void initDefaults();
};

} // namespace oap
