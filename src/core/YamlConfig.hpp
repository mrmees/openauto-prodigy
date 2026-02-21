#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
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
    int displayWidth() const;
    void setDisplayWidth(int v);
    int displayHeight() const;
    void setDisplayHeight(int v);

    // Touch
    QString touchDevice() const;
    void setTouchDevice(const QString& v);

    // Connection
    bool autoConnectAA() const;
    void setAutoConnectAA(bool v);
    QString wifiSsid() const;
    void setWifiSsid(const QString& v);
    QString wifiPassword() const;
    void setWifiPassword(const QString& v);
    QString wifiInterface() const;
    void setWifiInterface(const QString& v);
    uint16_t tcpPort() const;
    void setTcpPort(uint16_t v);

    // Audio
    int masterVolume() const;
    void setMasterVolume(int v);

    // Video
    int videoFps() const;
    void setVideoFps(int v);
    QString videoResolution() const;
    void setVideoResolution(const QString& v);
    int videoDpi() const;
    void setVideoDpi(int v);

    // Identity
    QString headUnitName() const;
    void setHeadUnitName(const QString& v);
    QString manufacturer() const;
    void setManufacturer(const QString& v);
    QString model() const;
    void setModel(const QString& v);
    QString swVersion() const;
    void setSwVersion(const QString& v);
    QString carModel() const;
    void setCarModel(const QString& v);
    QString carYear() const;
    void setCarYear(const QString& v);
    bool leftHandDrive() const;
    void setLeftHandDrive(bool v);

    // Sensors — night mode
    QString nightModeSource() const;
    void setNightModeSource(const QString& v);
    QString nightModeDayStart() const;
    void setNightModeDayStart(const QString& v);
    QString nightModeNightStart() const;
    void setNightModeNightStart(const QString& v);
    int nightModeGpioPin() const;
    void setNightModeGpioPin(int v);
    bool nightModeGpioActiveHigh() const;
    void setNightModeGpioActiveHigh(bool v);

    // Sensors — GPS
    bool gpsEnabled() const;
    void setGpsEnabled(bool v);
    QString gpsSource() const;
    void setGpsSource(const QString& v);

    // Audio — microphone
    QString microphoneDevice() const;
    void setMicrophoneDevice(const QString& v);
    double microphoneGain() const;
    void setMicrophoneGain(double v);

    // Launcher tiles — each tile is a QVariantMap with {id, label, icon, action}
    QList<QVariantMap> launcherTiles() const;
    void setLauncherTiles(const QList<QVariantMap>& tiles);

    // Plugins
    QStringList enabledPlugins() const;
    void setEnabledPlugins(const QStringList& plugins);

    // Plugin-scoped config — single source of truth in root_ YAML tree.
    // NO separate cache. All reads/writes go through root_["plugin_config"].
    QVariant pluginValue(const QString& pluginId, const QString& key) const;
    void setPluginValue(const QString& pluginId, const QString& key, const QVariant& value);

    // Generic dot-path access (e.g. "connection.wifi_ap.ssid")
    QVariant valueByPath(const QString& dottedKey) const;
    bool setValueByPath(const QString& dottedKey, const QVariant& value);

private:
    YAML::Node root_;  // Single source of truth — NO shadow state

    void initDefaults();
    static YAML::Node buildDefaultsNode();
};

} // namespace oap
