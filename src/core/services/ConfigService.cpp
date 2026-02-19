#include "ConfigService.hpp"
#include "core/YamlConfig.hpp"

namespace oap {

ConfigService::ConfigService(YamlConfig* config, const QString& configPath)
    : config_(config), configPath_(configPath)
{
}

QVariant ConfigService::value(const QString& key) const
{
    // Map dot-notation keys to YamlConfig typed accessors
    if (key == "display.brightness") return config_->displayBrightness();
    if (key == "display.theme") return config_->theme();
    if (key == "connection.auto_connect_aa") return config_->autoConnectAA();
    if (key == "connection.wifi_ap.ssid") return config_->wifiSsid();
    if (key == "connection.wifi_ap.password") return config_->wifiPassword();
    if (key == "connection.tcp_port") return static_cast<int>(config_->tcpPort());
    if (key == "audio.master_volume") return config_->masterVolume();
    if (key == "video.fps") return config_->videoFps();
    if (key == "hardware_profile") return config_->hardwareProfile();
    return {};
}

void ConfigService::setValue(const QString& key, const QVariant& val)
{
    if (key == "display.brightness") config_->setDisplayBrightness(val.toInt());
    else if (key == "display.theme") config_->setTheme(val.toString());
    else if (key == "connection.auto_connect_aa") config_->setAutoConnectAA(val.toBool());
    else if (key == "connection.wifi_ap.ssid") config_->setWifiSsid(val.toString());
    else if (key == "connection.wifi_ap.password") config_->setWifiPassword(val.toString());
    else if (key == "connection.tcp_port") config_->setTcpPort(static_cast<uint16_t>(val.toInt()));
    else if (key == "audio.master_volume") config_->setMasterVolume(val.toInt());
    else if (key == "video.fps") config_->setVideoFps(val.toInt());
    else if (key == "hardware_profile") config_->setHardwareProfile(val.toString());
}

QVariant ConfigService::pluginValue(const QString& pluginId, const QString& key) const
{
    return config_->pluginValue(pluginId, key);
}

void ConfigService::setPluginValue(const QString& pluginId, const QString& key, const QVariant& value)
{
    config_->setPluginValue(pluginId, key, value);
}

void ConfigService::save()
{
    config_->save(configPath_);
}

} // namespace oap
