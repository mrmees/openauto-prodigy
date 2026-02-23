#include "IpcServer.hpp"
#include "../YamlConfig.hpp"
#include "ThemeService.hpp"
#include "AudioService.hpp"
#include "CompanionListenerService.hpp"
#include "../plugin/PluginManager.hpp"
#include "../plugin/IPlugin.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QDebug>
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace oap {

IpcServer::IpcServer(QObject* parent)
    : QObject(parent)
{
}

IpcServer::~IpcServer()
{
    stop();
}

bool IpcServer::start(const QString& socketPath)
{
    if (server_) return false;

    // Remove stale socket file
    QFile::remove(socketPath);

    server_ = new QLocalServer(this);
    server_->setSocketOptions(QLocalServer::WorldAccessOption);

    connect(server_, &QLocalServer::newConnection, this, &IpcServer::onNewConnection);

    if (!server_->listen(socketPath)) {
        qWarning() << "IpcServer: Failed to listen on" << socketPath
                   << "—" << server_->errorString();
        delete server_;
        server_ = nullptr;
        return false;
    }

    qInfo() << "IpcServer: Listening on" << socketPath;
    return true;
}

void IpcServer::stop()
{
    if (server_) {
        server_->close();
        delete server_;
        server_ = nullptr;
    }
}

void IpcServer::setConfig(YamlConfig* config, const QString& configPath)
{
    config_ = config;
    configPath_ = configPath;
}

void IpcServer::setThemeService(ThemeService* themeService)
{
    themeService_ = themeService;
}

void IpcServer::setAudioService(AudioService* audioService)
{
    audioService_ = audioService;
}

void IpcServer::setPluginManager(PluginManager* pluginManager)
{
    pluginManager_ = pluginManager;
}

void IpcServer::setCompanionListenerService(CompanionListenerService* svc)
{
    companion_ = svc;
}

void IpcServer::onNewConnection()
{
    while (auto* socket = server_->nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, &IpcServer::onReadyRead);
        connect(socket, &QLocalSocket::disconnected, this, &IpcServer::onDisconnected);
    }
}

void IpcServer::onReadyRead()
{
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;

    QByteArray data = socket->readAll();
    QByteArray response = handleRequest(data);
    socket->write(response + "\n");
    socket->flush();
}

void IpcServer::onDisconnected()
{
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (socket)
        socket->deleteLater();
}

QByteArray IpcServer::handleRequest(const QByteArray& request)
{
    QJsonDocument doc = QJsonDocument::fromJson(request);
    if (!doc.isObject()) {
        return R"({"error":"Invalid JSON"})";
    }

    QJsonObject obj = doc.object();
    QString command = obj.value("command").toString();
    QVariantMap data = obj.value("data").toObject().toVariantMap();

    if (command == QLatin1String("get_config"))
        return handleGetConfig();
    if (command == QLatin1String("set_config"))
        return handleSetConfig(data);
    if (command == QLatin1String("get_theme"))
        return handleGetTheme();
    if (command == QLatin1String("set_theme"))
        return handleSetTheme(data);
    if (command == QLatin1String("list_plugins"))
        return handleListPlugins();
    if (command == QLatin1String("status"))
        return handleStatus();
    if (command == QLatin1String("get_audio_devices"))
        return handleGetAudioDevices();
    if (command == QLatin1String("get_audio_config"))
        return handleGetAudioConfig();
    if (command == QLatin1String("set_audio_config"))
        return handleSetAudioConfig(data);
    if (command == QLatin1String("companion_status"))
        return handleCompanionStatus();

    return R"({"error":"Unknown command"})";
}

QByteArray IpcServer::handleGetConfig()
{
    if (!config_) return R"({"error":"Config not available"})";

    QJsonObject obj;
    obj["wifi_ssid"] = config_->wifiSsid();
    obj["wifi_password"] = config_->wifiPassword();
    obj["tcp_port"] = config_->tcpPort();
    obj["video_fps"] = config_->videoFps();
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QByteArray IpcServer::handleSetConfig(const QVariantMap& data)
{
    if (!config_) return R"({"error":"Config not available"})";

    if (data.contains("wifi_ssid"))
        config_->setWifiSsid(data.value("wifi_ssid").toString());
    if (data.contains("wifi_password"))
        config_->setWifiPassword(data.value("wifi_password").toString());
    if (data.contains("tcp_port"))
        config_->setTcpPort(data.value("tcp_port").toInt());
    if (data.contains("video_fps"))
        config_->setVideoFps(data.value("video_fps").toInt());

    config_->save(configPath_);

    return R"({"ok":true})";
}

QByteArray IpcServer::handleGetTheme()
{
    if (!themeService_) return R"({"error":"Theme service not available"})";

    QJsonObject obj;
    obj["id"] = themeService_->currentThemeId();
    obj["font_family"] = themeService_->fontFamily();
    obj["night_mode"] = themeService_->nightMode();

    // Read color maps directly — no mode toggling, no signal emission
    QJsonObject dayObj;
    for (auto it = themeService_->dayColors().begin(); it != themeService_->dayColors().end(); ++it) {
        dayObj[it.key()] = it.value().name();
    }

    QJsonObject nightObj;
    for (auto it = themeService_->nightColors().begin(); it != themeService_->nightColors().end(); ++it) {
        nightObj[it.key()] = it.value().name();
    }

    obj["day"] = dayObj;
    obj["night"] = nightObj;

    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QByteArray IpcServer::handleSetTheme(const QVariantMap& data)
{
    if (!themeService_) return R"({"ok":false,"error":"Theme service not available"})";

    // Determine theme directory (with path traversal protection)
    QString themeId = data.value("id", "default").toString();
    static QRegularExpression validId("^[A-Za-z0-9._-]{1,64}$");
    if (!validId.match(themeId).hasMatch() || themeId == "." || themeId == "..")
        return R"({"ok":false,"error":"Invalid theme ID"})";

    QString themeDir = QDir::homePath() + "/.openauto/themes/" + themeId;
    QDir().mkpath(themeDir);
    QString yamlPath = themeDir + "/theme.yaml";

    // Build YAML content
    YAML::Node root;
    root["id"] = themeId.toStdString();
    root["name"] = data.value("name", themeId).toString().toStdString();
    if (data.contains("font_family"))
        root["font_family"] = data.value("font_family").toString().toStdString();

    // Day colors
    QVariantMap dayColors = data.value("day").toMap();
    for (auto it = dayColors.begin(); it != dayColors.end(); ++it) {
        root["day"][it.key().toStdString()] = it.value().toString().toStdString();
    }

    // Night colors
    QVariantMap nightColors = data.value("night").toMap();
    for (auto it = nightColors.begin(); it != nightColors.end(); ++it) {
        root["night"][it.key().toStdString()] = it.value().toString().toStdString();
    }

    // Write YAML file
    std::ofstream fout(yamlPath.toStdString());
    if (!fout.is_open())
        return R"({"ok":false,"error":"Cannot write theme file"})";
    fout << root;
    fout.close();

    // Reload theme
    if (!themeService_->loadTheme(themeDir))
        return R"({"ok":false,"error":"Theme reload failed"})";

    return R"({"ok":true})";
}

QByteArray IpcServer::handleListPlugins()
{
    QJsonArray arr;
    if (pluginManager_) {
        for (auto* plugin : pluginManager_->plugins()) {
            QJsonObject p;
            p["id"] = plugin->id();
            p["name"] = plugin->name();
            p["version"] = plugin->version();
            arr.append(p);
        }
    }
    QJsonObject obj;
    obj["plugins"] = arr;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QByteArray IpcServer::handleStatus()
{
    QJsonObject obj;
    obj["version"] = QStringLiteral("0.1.0");
    obj["plugin_count"] = pluginManager_ ? pluginManager_->plugins().count() : 0;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QByteArray IpcServer::handleGetAudioDevices()
{
    if (!audioService_) return R"({"error":"Audio service not available"})";

    QJsonArray outputs, inputs;
    auto* registry = audioService_->deviceRegistry();
    if (registry) {
        for (const auto& dev : registry->outputDevices()) {
            QJsonObject d;
            d["nodeName"] = dev.nodeName;
            d["description"] = dev.description;
            outputs.append(d);
        }
        for (const auto& dev : registry->inputDevices()) {
            QJsonObject d;
            d["nodeName"] = dev.nodeName;
            d["description"] = dev.description;
            inputs.append(d);
        }
    }

    QJsonObject obj;
    obj["outputs"] = outputs;
    obj["inputs"] = inputs;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QByteArray IpcServer::handleGetAudioConfig()
{
    if (!audioService_) return R"({"error":"Audio service not available"})";

    QJsonObject obj;
    obj["output_device"] = audioService_->outputDevice();
    obj["input_device"] = audioService_->inputDevice();
    obj["master_volume"] = audioService_->masterVolume();
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QByteArray IpcServer::handleSetAudioConfig(const QVariantMap& data)
{
    if (!audioService_) return R"({"error":"Audio service not available"})";

    if (data.contains("output_device"))
        audioService_->setOutputDevice(data.value("output_device").toString());
    if (data.contains("input_device"))
        audioService_->setInputDevice(data.value("input_device").toString());
    if (data.contains("master_volume"))
        audioService_->setMasterVolume(data.value("master_volume").toInt());

    // Persist to config if available
    if (config_) {
        if (data.contains("output_device"))
            config_->setValueByPath("audio.output_device", data.value("output_device").toString());
        if (data.contains("input_device"))
            config_->setValueByPath("audio.input_device", data.value("input_device").toString());
        if (data.contains("master_volume"))
            config_->setMasterVolume(data.value("master_volume").toInt());
        config_->save(configPath_);
    }

    return R"({"ok":true})";
}

QByteArray IpcServer::handleCompanionStatus()
{
    if (!companion_) return R"({"error":"Companion service not available"})";

    QJsonObject obj;
    obj["connected"] = companion_->isConnected();
    obj["gps_lat"] = companion_->gpsLat();
    obj["gps_lon"] = companion_->gpsLon();
    obj["gps_speed"] = companion_->gpsSpeed();
    obj["battery"] = companion_->phoneBattery();
    obj["charging"] = companion_->isPhoneCharging();
    obj["internet"] = companion_->isInternetAvailable();
    obj["proxy"] = companion_->proxyAddress();
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

} // namespace oap
