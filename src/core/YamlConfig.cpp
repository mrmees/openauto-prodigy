#include "core/YamlConfig.hpp"
#include "core/YamlMerge.hpp"
#include <QFile>
#include <QDir>
#include <fstream>

namespace oap {

YamlConfig::YamlConfig()
{
    initDefaults();
}

void YamlConfig::initDefaults()
{
    root_ = YAML::Node(YAML::NodeType::Map);

    root_["hardware_profile"] = "rpi4";

    root_["display"]["brightness"] = 80;
    root_["display"]["theme"] = "default";
    root_["display"]["orientation"] = "landscape";

    root_["connection"]["auto_connect_aa"] = true;
    root_["connection"]["bt_discoverable"] = true;
    root_["connection"]["wifi_ap"]["ssid"] = "OpenAutoProdigy";
    root_["connection"]["wifi_ap"]["password"] = "changeme123";
    root_["connection"]["wifi_ap"]["channel"] = 36;
    root_["connection"]["wifi_ap"]["band"] = "a";
    root_["connection"]["tcp_port"] = 5288;

    root_["audio"]["master_volume"] = 80;
    root_["audio"]["output_device"] = "auto";
    root_["audio"]["microphone"]["device"] = "auto";
    root_["audio"]["microphone"]["gain"] = 1.0;

    root_["video"]["fps"] = 60;
    root_["video"]["resolution"] = "720p";
    root_["video"]["dpi"] = 140;

    root_["identity"]["head_unit_name"] = "OpenAuto Prodigy";
    root_["identity"]["manufacturer"] = "OpenAuto Project";
    root_["identity"]["model"] = "Raspberry Pi 4";
    root_["identity"]["sw_version"] = "0.3.0";
    root_["identity"]["car_model"] = "";
    root_["identity"]["car_year"] = "";
    root_["identity"]["left_hand_drive"] = true;

    root_["sensors"]["night_mode"]["source"] = "time";
    root_["sensors"]["night_mode"]["day_start"] = "07:00";
    root_["sensors"]["night_mode"]["night_start"] = "19:00";
    root_["sensors"]["night_mode"]["gpio_pin"] = 17;
    root_["sensors"]["night_mode"]["gpio_active_high"] = true;
    root_["sensors"]["gps"]["enabled"] = true;
    root_["sensors"]["gps"]["source"] = "none";

    root_["nav_strip"]["order"] = YAML::Node(YAML::NodeType::Sequence);
    root_["nav_strip"]["order"].push_back("org.openauto.android-auto");
    root_["nav_strip"]["show_labels"] = true;

    root_["plugins"]["enabled"] = YAML::Node(YAML::NodeType::Sequence);
    root_["plugins"]["enabled"].push_back("org.openauto.android-auto");
    root_["plugins"]["disabled"] = YAML::Node(YAML::NodeType::Sequence);

    root_["plugin_config"] = YAML::Node(YAML::NodeType::Map);
}

void YamlConfig::load(const QString& filePath)
{
    YAML::Node defaults;
    initDefaults();
    defaults = YAML::Clone(root_);

    YAML::Node loaded = YAML::LoadFile(filePath.toStdString());
    root_ = mergeYaml(defaults, loaded);
}

void YamlConfig::save(const QString& filePath) const
{
    std::ofstream fout(filePath.toStdString());
    fout << root_;
}

// --- Hardware profile ---

QString YamlConfig::hardwareProfile() const
{
    return QString::fromStdString(root_["hardware_profile"].as<std::string>("rpi4"));
}

void YamlConfig::setHardwareProfile(const QString& v)
{
    root_["hardware_profile"] = v.toStdString();
}

// --- Display ---

int YamlConfig::displayBrightness() const
{
    return root_["display"]["brightness"].as<int>(80);
}

void YamlConfig::setDisplayBrightness(int v)
{
    root_["display"]["brightness"] = v;
}

QString YamlConfig::theme() const
{
    return QString::fromStdString(root_["display"]["theme"].as<std::string>("default"));
}

void YamlConfig::setTheme(const QString& v)
{
    root_["display"]["theme"] = v.toStdString();
}

// --- Connection ---

bool YamlConfig::autoConnectAA() const
{
    return root_["connection"]["auto_connect_aa"].as<bool>(true);
}

void YamlConfig::setAutoConnectAA(bool v)
{
    root_["connection"]["auto_connect_aa"] = v;
}

QString YamlConfig::wifiSsid() const
{
    return QString::fromStdString(
        root_["connection"]["wifi_ap"]["ssid"].as<std::string>("OpenAutoProdigy"));
}

void YamlConfig::setWifiSsid(const QString& v)
{
    root_["connection"]["wifi_ap"]["ssid"] = v.toStdString();
}

QString YamlConfig::wifiPassword() const
{
    return QString::fromStdString(
        root_["connection"]["wifi_ap"]["password"].as<std::string>("changeme123"));
}

void YamlConfig::setWifiPassword(const QString& v)
{
    root_["connection"]["wifi_ap"]["password"] = v.toStdString();
}

QString YamlConfig::wifiInterface() const
{
    return QString::fromStdString(
        root_["connection"]["wifi_ap"]["interface"].as<std::string>("wlan0"));
}

void YamlConfig::setWifiInterface(const QString& v)
{
    root_["connection"]["wifi_ap"]["interface"] = v.toStdString();
}

uint16_t YamlConfig::tcpPort() const
{
    return static_cast<uint16_t>(root_["connection"]["tcp_port"].as<int>(5288));
}

void YamlConfig::setTcpPort(uint16_t v)
{
    root_["connection"]["tcp_port"] = static_cast<int>(v);
}

// --- Audio ---

int YamlConfig::masterVolume() const
{
    return root_["audio"]["master_volume"].as<int>(80);
}

void YamlConfig::setMasterVolume(int v)
{
    root_["audio"]["master_volume"] = v;
}

// --- Video ---

int YamlConfig::videoFps() const
{
    return root_["video"]["fps"].as<int>(60);
}

void YamlConfig::setVideoFps(int v)
{
    root_["video"]["fps"] = v;
}

QString YamlConfig::videoResolution() const
{
    return QString::fromStdString(root_["video"]["resolution"].as<std::string>("720p"));
}

void YamlConfig::setVideoResolution(const QString& v)
{
    root_["video"]["resolution"] = v.toStdString();
}

int YamlConfig::videoDpi() const
{
    return root_["video"]["dpi"].as<int>(140);
}

void YamlConfig::setVideoDpi(int v)
{
    root_["video"]["dpi"] = v;
}

// --- Identity ---

QString YamlConfig::headUnitName() const
{
    return QString::fromStdString(root_["identity"]["head_unit_name"].as<std::string>("OpenAuto Prodigy"));
}

void YamlConfig::setHeadUnitName(const QString& v)
{
    root_["identity"]["head_unit_name"] = v.toStdString();
}

QString YamlConfig::manufacturer() const
{
    return QString::fromStdString(root_["identity"]["manufacturer"].as<std::string>("OpenAuto Project"));
}

void YamlConfig::setManufacturer(const QString& v)
{
    root_["identity"]["manufacturer"] = v.toStdString();
}

QString YamlConfig::model() const
{
    return QString::fromStdString(root_["identity"]["model"].as<std::string>("Raspberry Pi 4"));
}

void YamlConfig::setModel(const QString& v)
{
    root_["identity"]["model"] = v.toStdString();
}

QString YamlConfig::swVersion() const
{
    return QString::fromStdString(root_["identity"]["sw_version"].as<std::string>("0.3.0"));
}

void YamlConfig::setSwVersion(const QString& v)
{
    root_["identity"]["sw_version"] = v.toStdString();
}

QString YamlConfig::carModel() const
{
    return QString::fromStdString(root_["identity"]["car_model"].as<std::string>(""));
}

void YamlConfig::setCarModel(const QString& v)
{
    root_["identity"]["car_model"] = v.toStdString();
}

QString YamlConfig::carYear() const
{
    return QString::fromStdString(root_["identity"]["car_year"].as<std::string>(""));
}

void YamlConfig::setCarYear(const QString& v)
{
    root_["identity"]["car_year"] = v.toStdString();
}

bool YamlConfig::leftHandDrive() const
{
    return root_["identity"]["left_hand_drive"].as<bool>(true);
}

void YamlConfig::setLeftHandDrive(bool v)
{
    root_["identity"]["left_hand_drive"] = v;
}

// --- Sensors: night mode ---

QString YamlConfig::nightModeSource() const
{
    return QString::fromStdString(root_["sensors"]["night_mode"]["source"].as<std::string>("time"));
}

void YamlConfig::setNightModeSource(const QString& v)
{
    root_["sensors"]["night_mode"]["source"] = v.toStdString();
}

QString YamlConfig::nightModeDayStart() const
{
    return QString::fromStdString(root_["sensors"]["night_mode"]["day_start"].as<std::string>("07:00"));
}

void YamlConfig::setNightModeDayStart(const QString& v)
{
    root_["sensors"]["night_mode"]["day_start"] = v.toStdString();
}

QString YamlConfig::nightModeNightStart() const
{
    return QString::fromStdString(root_["sensors"]["night_mode"]["night_start"].as<std::string>("19:00"));
}

void YamlConfig::setNightModeNightStart(const QString& v)
{
    root_["sensors"]["night_mode"]["night_start"] = v.toStdString();
}

int YamlConfig::nightModeGpioPin() const
{
    return root_["sensors"]["night_mode"]["gpio_pin"].as<int>(17);
}

void YamlConfig::setNightModeGpioPin(int v)
{
    root_["sensors"]["night_mode"]["gpio_pin"] = v;
}

bool YamlConfig::nightModeGpioActiveHigh() const
{
    return root_["sensors"]["night_mode"]["gpio_active_high"].as<bool>(true);
}

void YamlConfig::setNightModeGpioActiveHigh(bool v)
{
    root_["sensors"]["night_mode"]["gpio_active_high"] = v;
}

// --- Sensors: GPS ---

bool YamlConfig::gpsEnabled() const
{
    return root_["sensors"]["gps"]["enabled"].as<bool>(true);
}

void YamlConfig::setGpsEnabled(bool v)
{
    root_["sensors"]["gps"]["enabled"] = v;
}

QString YamlConfig::gpsSource() const
{
    return QString::fromStdString(root_["sensors"]["gps"]["source"].as<std::string>("none"));
}

void YamlConfig::setGpsSource(const QString& v)
{
    root_["sensors"]["gps"]["source"] = v.toStdString();
}

// --- Audio: microphone ---

QString YamlConfig::microphoneDevice() const
{
    return QString::fromStdString(root_["audio"]["microphone"]["device"].as<std::string>("auto"));
}

void YamlConfig::setMicrophoneDevice(const QString& v)
{
    root_["audio"]["microphone"]["device"] = v.toStdString();
}

double YamlConfig::microphoneGain() const
{
    return root_["audio"]["microphone"]["gain"].as<double>(1.0);
}

void YamlConfig::setMicrophoneGain(double v)
{
    root_["audio"]["microphone"]["gain"] = v;
}

// --- Plugins ---

QStringList YamlConfig::enabledPlugins() const
{
    QStringList result;
    if (root_["plugins"]["enabled"].IsSequence()) {
        for (const auto& node : root_["plugins"]["enabled"])
            result.append(QString::fromStdString(node.as<std::string>()));
    }
    return result;
}

void YamlConfig::setEnabledPlugins(const QStringList& plugins)
{
    root_["plugins"]["enabled"] = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& p : plugins)
        root_["plugins"]["enabled"].push_back(p.toStdString());
}

// --- Plugin-scoped config ---

QVariant YamlConfig::pluginValue(const QString& pluginId, const QString& key) const
{
    auto pluginNode = root_["plugin_config"][pluginId.toStdString()];
    if (!pluginNode.IsDefined() || pluginNode.IsNull())
        return {};

    auto valNode = pluginNode[key.toStdString()];
    if (!valNode.IsDefined() || valNode.IsNull())
        return {};

    // Try to return the most specific type
    try {
        if (valNode.IsScalar()) {
            // Try bool first
            try {
                bool b = valNode.as<bool>();
                auto s = valNode.as<std::string>();
                if (s == "true" || s == "false")
                    return QVariant(b);
            } catch (...) {}

            // Try int
            try {
                int i = valNode.as<int>();
                return QVariant(i);
            } catch (...) {}

            // Try double
            try {
                double d = valNode.as<double>();
                return QVariant(d);
            } catch (...) {}

            // Fall back to string
            return QVariant(QString::fromStdString(valNode.as<std::string>()));
        }
    } catch (...) {}

    return {};
}

void YamlConfig::setPluginValue(const QString& pluginId, const QString& key, const QVariant& value)
{
    auto idStr = pluginId.toStdString();
    auto keyStr = key.toStdString();

    switch (value.typeId()) {
    case QMetaType::Bool:
        root_["plugin_config"][idStr][keyStr] = value.toBool();
        break;
    case QMetaType::Int:
        root_["plugin_config"][idStr][keyStr] = value.toInt();
        break;
    case QMetaType::Double:
    case QMetaType::Float:
        root_["plugin_config"][idStr][keyStr] = value.toDouble();
        break;
    default:
        root_["plugin_config"][idStr][keyStr] = value.toString().toStdString();
        break;
    }
}

} // namespace oap
