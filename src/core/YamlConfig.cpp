#include "core/YamlConfig.hpp"
#include "core/YamlMerge.hpp"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace {
// EQ graphic-EQ band count (matches oap::kNumBands from BiquadFilter.hpp; kept
// local so YamlConfig need not depend on the audio module).
constexpr int kEqBands = 10;
// Per-band gain clamp bound in dB (design §4.5).
constexpr float kEqGainLimitDb = 12.0f;
} // namespace

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
    root_["display"]["screen_size"] = 7.0;
    root_["display"]["theme"] = "default";
    root_["display"]["wallpaper_override"] = "";
    root_["display"]["clock_24h"] = false;
    root_["display"]["force_dark_mode"] = true;

    root_["connection"]["auto_connect_aa"] = true;
    root_["connection"]["bt_discoverable"] = true;
    root_["connection"]["gal_version"] = "6.0";
    root_["connection"]["wifi_ap"]["interface"] = "wlan0";
    root_["connection"]["wifi_ap"]["ssid"] = "OpenAutoProdigy";
    root_["connection"]["wifi_ap"]["password"] = "prodigy";
    root_["connection"]["wifi_ap"]["channel"] = 36;
    root_["connection"]["wifi_ap"]["band"] = "a";
    root_["connection"]["tcp_port"] = 5277;
    root_["connection"]["protocol_capture"]["enabled"] = false;
    root_["connection"]["protocol_capture"]["format"] = "jsonl";
    root_["connection"]["protocol_capture"]["include_media"] = false;
    root_["connection"]["protocol_capture"]["path"] = "/tmp/oaa-protocol-capture.jsonl";

    root_["audio"]["master_volume"] = 80;
    root_["audio"]["output_device"] = "auto";
    root_["audio"]["buffer_ms"]["media"] = 500;
    root_["audio"]["buffer_ms"]["speech"] = 500;
    root_["audio"]["buffer_ms"]["system"] = 500;
    root_["audio"]["microphone"]["device"] = "auto";
    root_["audio"]["microphone"]["gain"] = 1.0;

    root_["touch"]["device"] = "";

    root_["logging"]["verbose"] = false;
    root_["logging"]["debug_categories"] = YAML::Node(YAML::NodeType::Sequence);

    root_["phone"]["reject_sco_during_aa"] = false;  // design §6: flip only after live check L4
    root_["phone"]["settle_grace_ms"] = 2000;

    root_["video"]["fps"] = 30;
    root_["video"]["resolution"] = "720p";
    root_["video"]["dpi"] = 140;

    root_["video"]["codecs"] = YAML::Node(YAML::NodeType::Sequence);
    root_["video"]["codecs"].push_back("h265");
    root_["video"]["codecs"].push_back("h264");

    root_["video"]["decoder"] = YAML::Node(YAML::NodeType::Map);
    root_["video"]["decoder"]["h264"] = "auto";
    root_["video"]["decoder"]["h265"] = "auto";
    root_["video"]["decoder"]["vp9"] = "auto";
    root_["video"]["decoder"]["av1"] = "auto";

    root_["identity"]["head_unit_name"] = "OpenAuto Prodigy";
    root_["identity"]["manufacturer"] = "OpenAuto Project";
    root_["identity"]["model"] = "Raspberry Pi 4";
    root_["identity"]["car_model"] = "";
    root_["identity"]["car_year"] = "";
    root_["identity"]["left_hand_drive"] = true;
    // Stable head-unit identity for the External API (v1.1, ServerHello.
    // server_id). Empty until minted on first API start; see ApiServer::start().
    root_["identity"]["server_id"] = "";

    // External API v1 (ApiServer). Defaults mirror the api.proto documented
    // values; ApiServer still reads each key defensively with a fallback.
    root_["api"]["enabled"] = true;
    root_["api"]["tcp_port"] = 9810;
    root_["api"]["ws_port"] = 9811;
    root_["api"]["expose_lan"] = false;
    root_["api"]["max_queue_bytes"] = 1048576;
    root_["api"]["pairing_timeout_s"] = 120;
    root_["api"]["handshake_timeout_ms"] = 5000;

    root_["sensors"]["night_mode"]["source"] = "time";
    root_["sensors"]["night_mode"]["day_start"] = "07:00";
    root_["sensors"]["night_mode"]["night_start"] = "19:00";
    root_["sensors"]["night_mode"]["gpio_pin"] = 17;
    root_["sensors"]["night_mode"]["gpio_active_high"] = true;
    root_["sensors"]["gps"]["enabled"] = true;
    root_["sensors"]["gps"]["source"] = "none";

    root_["plugins"]["enabled"] = YAML::Node(YAML::NodeType::Sequence);
    root_["plugins"]["enabled"].push_back("org.openauto.android-auto");
    root_["plugins"]["disabled"] = YAML::Node(YAML::NodeType::Sequence);

    root_["plugin_config"] = YAML::Node(YAML::NodeType::Map);

    // EQ defaults
    root_["audio"]["equalizer"]["streams"]["media"]["preset"] = "Flat";
    root_["audio"]["equalizer"]["streams"]["navigation"]["preset"] = "Voice";
    // "system" (renamed from "phone", Task 5): EQ for AA system sounds, not
    // telephony. Legacy "phone:" EQ keys migrate to it (migrateEqPhoneToSystem).
    root_["audio"]["equalizer"]["streams"]["system"]["preset"] = "Voice";
    root_["audio"]["equalizer"]["user_presets"] = YAML::Node(YAML::NodeType::Sequence);

    // UI override defaults (0 = "not set, use auto-derived")
    root_["ui"]["scale"] = 0;
    root_["ui"]["fontScale"] = 0;
    root_["ui"]["tokens"]["rowH"] = 0;
    root_["ui"]["tokens"]["touchMin"] = 0;
    root_["ui"]["tokens"]["fontTitle"] = 0;
    root_["ui"]["tokens"]["fontBody"] = 0;
    root_["ui"]["tokens"]["fontSmall"] = 0;
    root_["ui"]["tokens"]["fontHeading"] = 0;
    root_["ui"]["tokens"]["fontTiny"] = 0;
    root_["ui"]["tokens"]["headerH"] = 0;
    root_["ui"]["tokens"]["iconSize"] = 0;
    root_["ui"]["tokens"]["radius"] = 0;
    root_["ui"]["tokens"]["tileW"] = 0;
    root_["ui"]["tokens"]["tileH"] = 0;
    root_["ui"]["tokens"]["trackThick"] = 0;
    root_["ui"]["tokens"]["trackThin"] = 0;
    root_["ui"]["tokens"]["knobSize"] = 0;
    root_["ui"]["tokens"]["knobSizeSmall"] = 0;
    root_["ui"]["tokens"]["radiusSmall"] = 0;
    root_["ui"]["tokens"]["radiusLarge"] = 0;
    root_["ui"]["tokens"]["albumArt"] = 0;
    root_["ui"]["tokens"]["callBtnSize"] = 0;
    root_["ui"]["tokens"]["overlayBtnW"] = 0;
    root_["ui"]["tokens"]["overlayBtnH"] = 0;
    root_["ui"]["fontFloor"] = 0;
    root_["ui"]["tokens"]["navbarThick"] = 0;

    // Widget home screen defaults
    root_["widget_config"]["version"] = 1;

    YAML::Node defaultPages(YAML::NodeType::Sequence);
    YAML::Node homePage;
    homePage["id"] = "home";
    homePage["layoutTemplate"] = "standard-3pane";
    homePage["order"] = 0;
    defaultPages.push_back(homePage);
    root_["widget_config"]["pages"] = defaultPages;

    YAML::Node defaultPlacements(YAML::NodeType::Sequence);

    YAML::Node clockPlacement;
    clockPlacement["instanceId"] = "clock-main";
    clockPlacement["widgetId"] = "org.openauto.clock";
    clockPlacement["pageId"] = "home";
    clockPlacement["paneId"] = "main";
    defaultPlacements.push_back(clockPlacement);

    YAML::Node aaPlacement;
    aaPlacement["instanceId"] = "aa-status-sub1";
    aaPlacement["widgetId"] = "org.openauto.aa-status";
    aaPlacement["pageId"] = "home";
    aaPlacement["paneId"] = "sub1";
    defaultPlacements.push_back(aaPlacement);

    YAML::Node npPlacement;
    npPlacement["instanceId"] = "now-playing-sub2";
    npPlacement["widgetId"] = "org.openauto.bt-now-playing";
    npPlacement["pageId"] = "home";
    npPlacement["paneId"] = "sub2";
    defaultPlacements.push_back(npPlacement);

    root_["widget_config"]["placements"] = defaultPlacements;

    // Grid-based widget config defaults (v4 with multi-dashboard support)
    root_["widget_grid"]["version"] = 4;
    root_["widget_grid"]["active_dashboard"] = "home";
    {
        YAML::Node home;
        home["id"] = "home";
        home["name"] = "Home";
        home["next_instance_id"] = 0;
        home["page_count"] = 2;
        home["placements"] = YAML::Node(YAML::NodeType::Sequence);
        YAML::Node seq(YAML::NodeType::Sequence);
        seq.push_back(home);
        root_["widget_grid"]["dashboards"] = seq;
    }

    // Home screen grid density
    root_["home"]["gridDensityBias"] = 0;

    // Navbar defaults
    root_["navbar"]["edge"] = "bottom";
    root_["navbar"]["show_during_aa"] = true;
    root_["navbar"]["gesture"]["tap_max_ms"] = 200;
    root_["navbar"]["gesture"]["short_hold_max_ms"] = 600;
}

void YamlConfig::load(const QString& filePath)
{
    YAML::Node defaults;
    initDefaults();
    defaults = YAML::Clone(root_);

    const std::string path = filePath.toStdString();
    const bool fileExists = QFile::exists(filePath);

    try {
        YAML::Node loaded = YAML::LoadFile(path);
        // Rename the legacy EQ "phone" stream key to "system" on the RAW user
        // YAML BEFORE the defaults merge (design §4.6, round-1 F5). The merge
        // injects a default "system" key, so a post-merge fallback read could
        // never see the migrated value — this placement is load-bearing.
        migrateEqPhoneToSystem(loaded);
        root_ = mergeYaml(defaults, loaded);
        migrateWidgetGridV3();
    } catch (const std::exception& e) {
        // Guarantee: load() never throws and always leaves a usable config.
        // Reset explicitly rather than trusting whatever partial state
        // merge/migration left root_ in.
        root_ = defaults;

        if (fileExists) {
            const std::string corruptPath = path + ".corrupt";
            if (::rename(path.c_str(), corruptPath.c_str()) == 0) {
                qWarning() << "[YamlConfig] Corrupt config" << filePath
                           << "moved aside to" << QString::fromStdString(corruptPath)
                           << "- falling back to defaults. Reason:" << e.what();
            } else {
                qWarning() << "[YamlConfig] Corrupt config" << filePath
                           << "- falling back to defaults. Reason:" << e.what()
                           << "- additionally failed to rename aside:" << strerror(errno);
            }
        }
        // Missing file (YAML::BadFile) is benign/expected (e.g. first boot) --
        // defaults are already applied, nothing to move aside, stay quiet.
    }
}

bool YamlConfig::save(const QString& filePath) const
{
    std::string content;
    try {
        std::ostringstream oss;
        oss << root_;
        content = oss.str();
    } catch (const std::exception& e) {
        qWarning() << "[YamlConfig] save: serialization failed for" << filePath
                   << "-" << e.what();
        return false;
    }

    const std::string path = filePath.toStdString();
    const std::string tmpPath = path + ".tmp";

    int fd = ::open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        qWarning() << "[YamlConfig] save: failed to open" << QString::fromStdString(tmpPath)
                   << "-" << strerror(errno);
        return false;
    }

    const char* data = content.data();
    size_t remaining = content.size();
    while (remaining > 0) {
        ssize_t written = ::write(fd, data, remaining);
        if (written < 0) {
            if (errno == EINTR) continue;
            qWarning() << "[YamlConfig] save: write failed for" << QString::fromStdString(tmpPath)
                       << "-" << strerror(errno);
            ::close(fd);
            ::unlink(tmpPath.c_str());
            return false;
        }
        data += written;
        remaining -= static_cast<size_t>(written);
    }

    if (::fsync(fd) != 0) {
        qWarning() << "[YamlConfig] save: fsync failed for" << QString::fromStdString(tmpPath)
                   << "-" << strerror(errno);
        ::close(fd);
        ::unlink(tmpPath.c_str());
        return false;
    }

    if (::close(fd) != 0) {
        qWarning() << "[YamlConfig] save: close failed for" << QString::fromStdString(tmpPath)
                   << "-" << strerror(errno);
        ::unlink(tmpPath.c_str());
        return false;
    }

    if (::rename(tmpPath.c_str(), path.c_str()) != 0) {
        qWarning() << "[YamlConfig] save: rename failed" << QString::fromStdString(tmpPath)
                   << "->" << QString::fromStdString(path) << "-" << strerror(errno);
        ::unlink(tmpPath.c_str());
        return false;
    }

    // Durability: fsync the parent directory so the rename itself survives a
    // power cut (round-2 F7 — a car's normal shutdown IS a power cut). If the
    // directory cannot be opened OR its fsync fails, the rename is NOT durably
    // committed, so report failure: the EQ save-debounce retry then stays armed
    // and re-attempts on the next tick. Only the close() stays best-effort.
    const std::string dirPath = QFileInfo(filePath).absolutePath().toStdString();
    int dfd = ::open(dirPath.c_str(), O_RDONLY | O_DIRECTORY);
    if (dfd < 0) {
        qWarning() << "[YamlConfig] save: cannot open parent dir to fsync"
                   << QString::fromStdString(dirPath) << "-" << strerror(errno);
        return false;
    }
    if (::fsync(dfd) != 0) {
        qWarning() << "[YamlConfig] save: parent dir fsync failed"
                   << QString::fromStdString(dirPath) << "-" << strerror(errno);
        ::close(dfd);   // best-effort
        return false;
    }
    ::close(dfd);       // best-effort

    return true;
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

// --- Touch ---

QString YamlConfig::touchDevice() const
{
    return QString::fromStdString(root_["touch"]["device"].as<std::string>(""));
}

void YamlConfig::setTouchDevice(const QString& v)
{
    root_["touch"]["device"] = v.toStdString();
}

// --- Logging ---

bool YamlConfig::loggingVerbose() const
{
    return root_["logging"]["verbose"].as<bool>(false);
}

void YamlConfig::setLoggingVerbose(bool v)
{
    root_["logging"]["verbose"] = v;
}

QStringList YamlConfig::loggingDebugCategories() const
{
    QStringList categories;
    const YAML::Node node = root_["logging"]["debug_categories"];
    if (!node.IsSequence())
        return categories;

    for (const auto& category : node) {
        if (category.IsScalar())
            categories.append(QString::fromStdString(category.as<std::string>()));
    }
    return categories;
}

void YamlConfig::setLoggingDebugCategories(const QStringList& categories)
{
    YAML::Node sequence(YAML::NodeType::Sequence);
    for (const QString& category : categories)
        sequence.push_back(category.toStdString());
    root_["logging"]["debug_categories"] = sequence;
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
        root_["connection"]["wifi_ap"]["password"].as<std::string>("prodigy"));
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
    return static_cast<uint16_t>(root_["connection"]["tcp_port"].as<int>(5277));
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
    return root_["video"]["fps"].as<int>(30);
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

// --- Video: codec config ---

QStringList YamlConfig::videoCodecs() const
{
    QStringList result;
    auto codecs = root_["video"]["codecs"];
    if (codecs.IsSequence()) {
        for (const auto& node : codecs)
            result.append(QString::fromStdString(node.as<std::string>()));
    }
    if (result.isEmpty()) {
        result << "h264" << "h265";
    }
    return result;
}

QString YamlConfig::videoDecoder(const QString& codec) const
{
    return QString::fromStdString(
        root_["video"]["decoder"][codec.toStdString()].as<std::string>("auto"));
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

// --- Audio: per-stream buffer sizing ---

int YamlConfig::audioBufferMs(const QString& streamType) const
{
    int fallback = 500;
    return root_["audio"]["buffer_ms"][streamType.toStdString()].as<int>(fallback);
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

// --- EQ config ---

QString YamlConfig::eqStreamPreset(const QString& streamName) const
{
    auto node = root_["audio"]["equalizer"]["streams"][streamName.toStdString()]["preset"];
    if (node.IsDefined() && node.IsScalar())
        return QString::fromStdString(node.as<std::string>());
    return QStringLiteral("Flat");
}

void YamlConfig::setEqStreamPreset(const QString& streamName, const QString& presetName)
{
    root_["audio"]["equalizer"]["streams"][streamName.toStdString()]["preset"] = presetName.toStdString();
}

QList<float> YamlConfig::validatedGains(const YAML::Node& node)
{
    // Valid iff a Sequence of exactly kEqBands finite float scalars. Any other
    // shape (absent, wrong length, non-scalar, unparseable, NaN/Inf) is
    // rejected wholesale so a malformed file can never poison the filter chain.
    if (!node.IsDefined() || !node.IsSequence() || static_cast<int>(node.size()) != kEqBands)
        return {};

    QList<float> gains;
    gains.reserve(kEqBands);
    for (const auto& g : node) {
        if (!g.IsScalar())
            return {};
        float v;
        try {
            v = g.as<float>();
        } catch (...) {
            return {};   // unparseable scalar
        }
        if (!std::isfinite(v))
            return {};   // .nan / .inf / -.inf
        gains.append(std::clamp(v, -kEqGainLimitDb, kEqGainLimitDb));
    }
    return gains;
}

QList<float> YamlConfig::eqStreamGains(const QString& streamName) const
{
    return validatedGains(
        root_["audio"]["equalizer"]["streams"][streamName.toStdString()]["gains"]);
}

void YamlConfig::setEqStreamGains(const QString& streamName, const QList<float>& gains)
{
    YAML::Node seq(YAML::NodeType::Sequence);
    for (float g : gains)
        seq.push_back(g);
    root_["audio"]["equalizer"]["streams"][streamName.toStdString()]["gains"] = seq;
}

bool YamlConfig::eqStreamBypassed(const QString& streamName) const
{
    auto node = root_["audio"]["equalizer"]["streams"][streamName.toStdString()]["bypassed"];
    if (node.IsDefined() && node.IsScalar())
        return node.as<bool>(false);
    return false;
}

void YamlConfig::setEqStreamBypassed(const QString& streamName, bool bypassed)
{
    root_["audio"]["equalizer"]["streams"][streamName.toStdString()]["bypassed"] = bypassed;
}

QList<YamlConfig::EqUserPreset> YamlConfig::eqUserPresets() const
{
    QList<EqUserPreset> result;
    auto node = root_["audio"]["equalizer"]["user_presets"];
    if (!node.IsDefined() || !node.IsSequence())
        return result;

    for (const auto& entry : node) {
        EqUserPreset preset;
        if (entry["name"].IsDefined())
            preset.name = QString::fromStdString(entry["name"].as<std::string>());

        // Same validator as eqStreamGains: an invalid gains array drops the
        // whole preset (design §4.5 / round-2 F5) rather than loading garbage.
        const QList<float> gains = validatedGains(entry["gains"]);
        if (static_cast<int>(gains.size()) != kEqBands) {
            qWarning() << "[YamlConfig] Dropping EQ user preset with invalid gains:"
                       << (preset.name.isEmpty() ? QStringLiteral("<unnamed>") : preset.name);
            continue;
        }
        for (int i = 0; i < kEqBands; ++i)
            preset.gains[i] = gains[i];
        result.append(preset);
    }
    return result;
}

void YamlConfig::setEqUserPresets(const QList<EqUserPreset>& presets)
{
    YAML::Node node(YAML::NodeType::Sequence);
    for (const auto& preset : presets) {
        YAML::Node entry;
        entry["name"] = preset.name.toStdString();
        YAML::Node gains(YAML::NodeType::Sequence);
        for (int i = 0; i < 10; ++i)
            gains.push_back(preset.gains[i]);
        entry["gains"] = gains;
        node.push_back(entry);
    }
    root_["audio"]["equalizer"]["user_presets"] = node;
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
        if (valNode.IsSequence()) {
            QStringList list;
            for (const auto& item : valNode)
                list << QString::fromStdString(item.as<std::string>());
            return QVariant(list);
        }

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
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        root_["plugin_config"][idStr][keyStr] = value.toLongLong();
        break;
    case QMetaType::Double:
    case QMetaType::Float:
        root_["plugin_config"][idStr][keyStr] = value.toDouble();
        break;
    case QMetaType::QStringList: {
        // QVariant(QStringList).toString() is "" — must emit a real sequence
        // (media player's last_queue; bench 2026-07-09).
        YAML::Node seq(YAML::NodeType::Sequence);
        for (const QString& s : value.toStringList())
            seq.push_back(s.toStdString());
        root_["plugin_config"][idStr][keyStr] = seq;
        break;
    }
    default:
        root_["plugin_config"][idStr][keyStr] = value.toString().toStdString();
        break;
    }
}

// --- Widget home screen config ---

int YamlConfig::widgetConfigVersion() const
{
    return root_["widget_config"]["version"].as<int>(1);
}

QList<PageDescriptor> YamlConfig::widgetPages() const
{
    QList<PageDescriptor> result;
    auto pages = root_["widget_config"]["pages"];
    if (pages.IsSequence()) {
        for (const auto& node : pages) {
            PageDescriptor page;
            if (node["id"]) page.id = QString::fromStdString(node["id"].as<std::string>());
            if (node["layoutTemplate"]) page.layoutTemplate = QString::fromStdString(node["layoutTemplate"].as<std::string>());
            if (node["order"]) page.order = node["order"].as<int>(0);
            result.append(page);
        }
    }
    if (result.isEmpty()) {
        PageDescriptor defaultPage;
        defaultPage.id = QStringLiteral("home");
        defaultPage.layoutTemplate = QStringLiteral("standard-3pane");
        defaultPage.order = 0;
        result.append(defaultPage);
    }
    return result;
}

void YamlConfig::setWidgetPages(const QList<PageDescriptor>& pages)
{
    YAML::Node node(YAML::NodeType::Sequence);
    for (const auto& page : pages) {
        YAML::Node p;
        p["id"] = page.id.toStdString();
        p["layoutTemplate"] = page.layoutTemplate.toStdString();
        p["order"] = page.order;
        node.push_back(p);
    }
    root_["widget_config"]["pages"] = node;
}

QList<WidgetPlacement> YamlConfig::widgetPlacements() const
{
    QList<WidgetPlacement> result;
    auto placements = root_["widget_config"]["placements"];
    if (placements.IsSequence()) {
        for (const auto& node : placements) {
            WidgetPlacement p;
            if (node["instanceId"]) p.instanceId = QString::fromStdString(node["instanceId"].as<std::string>());
            if (node["widgetId"]) p.widgetId = QString::fromStdString(node["widgetId"].as<std::string>());
            if (node["pageId"]) p.pageId = QString::fromStdString(node["pageId"].as<std::string>());
            if (node["paneId"]) p.paneId = QString::fromStdString(node["paneId"].as<std::string>());
            if (node["config"] && node["config"].IsMap()) {
                for (auto it = node["config"].begin(); it != node["config"].end(); ++it) {
                    QString key = QString::fromStdString(it->first.as<std::string>());
                    if (it->second.IsScalar()) {
                        try {
                            double d = it->second.as<double>();
                            p.config[key] = d;
                        } catch (...) {
                            p.config[key] = QString::fromStdString(it->second.as<std::string>());
                        }
                    }
                }
            }
            result.append(p);
        }
    }
    if (result.isEmpty()) {
        // Return defaults
        WidgetPlacement clock;
        clock.instanceId = QStringLiteral("clock-main");
        clock.widgetId = QStringLiteral("org.openauto.clock");
        clock.pageId = QStringLiteral("home");
        clock.paneId = QStringLiteral("main");
        result.append(clock);

        WidgetPlacement aa;
        aa.instanceId = QStringLiteral("aa-status-sub1");
        aa.widgetId = QStringLiteral("org.openauto.aa-status");
        aa.pageId = QStringLiteral("home");
        aa.paneId = QStringLiteral("sub1");
        result.append(aa);

        WidgetPlacement np;
        np.instanceId = QStringLiteral("now-playing-sub2");
        np.widgetId = QStringLiteral("org.openauto.bt-now-playing");
        np.pageId = QStringLiteral("home");
        np.paneId = QStringLiteral("sub2");
        result.append(np);
    }
    return result;
}

void YamlConfig::setWidgetPlacements(const QList<WidgetPlacement>& placements)
{
    // Validate: deduplicate by composite key (pageId:paneId), keep first
    QSet<QString> seenKeys;
    QList<WidgetPlacement> validated;
    for (const auto& p : placements) {
        QString key = p.compositeKey();
        if (!seenKeys.contains(key)) {
            seenKeys.insert(key);
            validated.append(p);
        }
    }

    YAML::Node node(YAML::NodeType::Sequence);
    for (const auto& p : validated) {
        YAML::Node n;
        n["instanceId"] = p.instanceId.toStdString();
        n["widgetId"] = p.widgetId.toStdString();
        n["pageId"] = p.pageId.toStdString();
        n["paneId"] = p.paneId.toStdString();
        if (!p.config.isEmpty()) {
            YAML::Node configNode;
            for (auto it = p.config.begin(); it != p.config.end(); ++it) {
                std::string key = it.key().toStdString();
                if (it.value().typeId() == QMetaType::Double || it.value().typeId() == QMetaType::Float)
                    configNode[key] = it.value().toDouble();
                else
                    configNode[key] = it.value().toString().toStdString();
            }
            n["config"] = configNode;
        }
        node.push_back(n);
    }
    root_["widget_config"]["placements"] = node;
}

// --- Home screen grid density ---

int YamlConfig::gridDensityBias() const
{
    return std::clamp(root_["home"]["gridDensityBias"].as<int>(0), -1, 1);
}

void YamlConfig::setGridDensityBias(int bias)
{
    root_["home"]["gridDensityBias"] = std::clamp(bias, -1, 1);
}

// --- Grid-based widget config (v4 — multi-dashboard) ---

QList<GridPlacement> YamlConfig::placementsFromNode(const YAML::Node& placements)
{
    QList<GridPlacement> result;
    if (!placements.IsDefined() || !placements.IsSequence())
        return result;

    for (const auto& node : placements) {
        GridPlacement p;
        p.instanceId = QString::fromStdString(node["instance_id"].as<std::string>(""));
        p.widgetId = QString::fromStdString(node["widget_id"].as<std::string>(""));
        p.col = node["col"].as<int>(0);
        p.row = node["row"].as<int>(0);
        p.colSpan = node["col_span"].as<int>(1);
        p.rowSpan = node["row_span"].as<int>(1);
        p.opacity = node["opacity"].as<double>(0.25);
        p.page = node["page"].as<int>(0); // default 0 for v2 compat
        p.visible = true; // visibility is runtime state, not persisted

        // Read per-instance config map
        if (node["config"].IsDefined() && node["config"].IsMap()) {
            for (auto it = node["config"].begin(); it != node["config"].end(); ++it) {
                QString key = QString::fromStdString(it->first.as<std::string>());
                const auto& val = it->second;
                if (val.IsScalar()) {
                    const std::string s = val.Scalar();
                    // Try bool
                    if (s == "true") { p.config[key] = true; continue; }
                    if (s == "false") { p.config[key] = false; continue; }
                    // Try int
                    try {
                        int i = val.as<int>();
                        // Verify it round-trips as int (not a float like "1.5")
                        if (std::to_string(i) == s) {
                            p.config[key] = i;
                            continue;
                        }
                    } catch (...) {}
                    // Try double
                    try {
                        double d = val.as<double>();
                        p.config[key] = d;
                        continue;
                    } catch (...) {}
                    // Fall back to string
                    p.config[key] = QString::fromStdString(s);
                }
            }
        }

        result.append(p);
    }
    return result;
}

YAML::Node YamlConfig::placementsToNode(const QList<GridPlacement>& placements)
{
    YAML::Node node(YAML::NodeType::Sequence);
    for (const auto& p : placements) {
        YAML::Node n;
        n["instance_id"] = p.instanceId.toStdString();
        n["widget_id"] = p.widgetId.toStdString();
        n["col"] = p.col;
        n["row"] = p.row;
        n["col_span"] = p.colSpan;
        n["row_span"] = p.rowSpan;
        n["opacity"] = p.opacity;
        n["page"] = p.page;

        // Write per-instance config map (skip if empty for backward compat)
        if (!p.config.isEmpty()) {
            YAML::Node configNode;
            for (auto it = p.config.constBegin(); it != p.config.constEnd(); ++it) {
                std::string key = it.key().toStdString();
                switch (it.value().typeId()) {
                case QMetaType::Bool:
                    configNode[key] = it.value().toBool();
                    break;
                case QMetaType::Int:
                    configNode[key] = it.value().toInt();
                    break;
                case QMetaType::Double:
                case QMetaType::Float:
                    configNode[key] = it.value().toDouble();
                    break;
                default:
                    configNode[key] = it.value().toString().toStdString();
                    break;
                }
            }
            n["config"] = configNode;
        }

        node.push_back(n);
    }
    return node;
}

QList<DashboardConfig> YamlConfig::dashboards() const
{
    QList<DashboardConfig> result;
    auto seq = root_["widget_grid"]["dashboards"];
    if (!seq.IsDefined() || !seq.IsSequence()) return result;
    for (const auto& n : seq) {
        DashboardConfig d;
        d.id = QString::fromStdString(n["id"].as<std::string>(""));
        d.name = QString::fromStdString(n["name"].as<std::string>(d.id.toStdString()));
        d.nextInstanceId = n["next_instance_id"].as<int>(0);
        d.pageCount = n["page_count"].as<int>(1);
        d.placements = placementsFromNode(n["placements"]);
        if (!d.id.isEmpty()) result.append(d);
    }
    return result;
}

void YamlConfig::setDashboards(const QList<DashboardConfig>& list)
{
    root_["widget_grid"]["version"] = 4;
    YAML::Node seq(YAML::NodeType::Sequence);
    for (const auto& d : list) {
        YAML::Node n;
        n["id"] = d.id.toStdString();
        n["name"] = d.name.toStdString();
        n["next_instance_id"] = d.nextInstanceId;
        n["page_count"] = d.pageCount;
        n["placements"] = placementsToNode(d.placements);
        seq.push_back(n);
    }
    root_["widget_grid"]["dashboards"] = seq;
}

QString YamlConfig::activeDashboardId() const
{
    return QString::fromStdString(
        root_["widget_grid"]["active_dashboard"].as<std::string>("home"));
}

void YamlConfig::setActiveDashboardId(const QString& id)
{
    root_["widget_grid"]["active_dashboard"] = id.toStdString();
}

void YamlConfig::migrateWidgetGridV3()
{
    YAML::Node wg = root_["widget_grid"];
    // A flat placements sequence only ever appears in pre-v4 (versionless or
    // v2/v3) shapes -- v4 files always nest placements under dashboards[].
    // Gate strictly on version==3 missed flat v2/versionless configs, which
    // then loaded as an empty default dashboard and the next save clobbered
    // the user's placements on disk. Migrate whenever the flat shape is
    // present, regardless of the (possibly absent/wrong) version tag.
    const bool flatShape = wg["placements"].IsDefined() && wg["placements"].IsSequence();
    if (!wg.IsDefined() || (!flatShape && wg["version"].as<int>(4) != 3)) return;

    YAML::Node home;
    home["id"] = "home";
    home["name"] = "Home";
    home["next_instance_id"] = wg["next_instance_id"].as<int>(0);
    home["page_count"] = wg["page_count"].as<int>(2);
    home["placements"] = (wg["placements"].IsDefined() && wg["placements"].IsSequence())
                             ? wg["placements"] : YAML::Node(YAML::NodeType::Sequence);
    YAML::Node seq(YAML::NodeType::Sequence);
    seq.push_back(home);
    wg["dashboards"] = seq;                 // replaces the defaults-merged empty v4 skeleton
    wg["active_dashboard"] = "home";
    wg["version"] = 4;
    wg.remove("next_instance_id");
    wg.remove("page_count");
    wg.remove("placements");
}

void YamlConfig::migrateEqPhoneToSystem(YAML::Node& loaded)
{
    // Legacy "phone" EQ stream key -> "system" (Task 5, honest labeling). Runs
    // on the raw user YAML before mergeYaml; an existing "system" key wins so a
    // hand-authored/newer config is never clobbered.
    auto streams = loaded["audio"]["equalizer"]["streams"];
    if (!streams || !streams.IsMap()) return;
    if (streams["phone"]) {
        if (!streams["system"])
            streams["system"] = YAML::Clone(streams["phone"]);
        streams.remove("phone");
    }
}

int YamlConfig::gridSavedCols() const
{
    return root_["widget_grid"]["grid_cols"].as<int>(0);
}

int YamlConfig::gridSavedRows() const
{
    return root_["widget_grid"]["grid_rows"].as<int>(0);
}

void YamlConfig::setGridSavedDims(int cols, int rows)
{
    root_["widget_grid"]["grid_cols"] = cols;
    root_["widget_grid"]["grid_rows"] = rows;
}

// --- Generic dot-path access ---

static QVariant yamlScalarToVariant(const YAML::Node& node,
                                    bool preserveString = false)
{
    if (!node.IsScalar()) return {};

    const std::string s = node.Scalar();
    if (preserveString)
        return QVariant(QString::fromStdString(s));

    if (s == "true") return QVariant(true);
    if (s == "false") return QVariant(false);

    bool intOk = false;
    int i = QString::fromStdString(s).toInt(&intOk);
    if (intOk) return QVariant(i);

    bool dblOk = false;
    double d = QString::fromStdString(s).toDouble(&dblOk);
    if (dblOk) return QVariant(d);

    return QVariant(QString::fromStdString(s));
}

QVariant YamlConfig::valueByPath(const QString& dottedKey) const
{
    if (dottedKey.isEmpty()) return {};

    QStringList parts = dottedKey.split('.');

    YAML::Node node = YAML::Clone(root_);
    for (const auto& part : parts) {
        if (!node.IsMap()) return {};
        node.reset(node[part.toStdString()]);
        if (!node.IsDefined() || node.IsNull()) return {};
    }

    return yamlScalarToVariant(
        node, dottedKey == QStringLiteral("connection.gal_version"));
}

YAML::Node YamlConfig::buildDefaultsNode()
{
    YamlConfig tmp;
    return YAML::Clone(tmp.root_);
}

bool YamlConfig::setValueByPath(const QString& dottedKey, const QVariant& value)
{
    if (dottedKey.isEmpty()) return false;

    QStringList parts = dottedKey.split('.');

    // Validate against DEFAULTS tree, not merged root_
    // Path must exist AND resolve to a scalar (leaf) node — reject writes to maps/sequences
    {
        YAML::Node defaults = buildDefaultsNode();
        for (const auto& part : parts) {
            if (!defaults.IsMap()) return false;
            defaults.reset(defaults[part.toStdString()]);
            if (!defaults.IsDefined()) return false;
        }
        if (!defaults.IsScalar()) return false;
    }

    // Path exists in schema — navigate the real tree and set the value
    YAML::Node node = root_;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!node.IsMap()) return false;
        node.reset(node[parts[i].toStdString()]);
    }

    std::string leafKey = parts.last().toStdString();
    switch (value.typeId()) {
    case QMetaType::Bool:
        node[leafKey] = value.toBool();
        break;
    case QMetaType::Int:
        node[leafKey] = value.toInt();
        break;
    case QMetaType::Double:
    case QMetaType::Float:
        node[leafKey] = value.toDouble();
        break;
    default:
        node[leafKey] = value.toString().toStdString();
        break;
    }

    return true;
}

} // namespace oap
