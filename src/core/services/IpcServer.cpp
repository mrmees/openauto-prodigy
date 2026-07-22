#include "IpcServer.hpp"
#include "../YamlConfig.hpp"
#include "ThemeService.hpp"
#include "core/services/ThemeInstallRequest.hpp"
#include "AudioService.hpp"
#include "core/api/ApiInboundState.hpp"
#include "../plugin/PluginManager.hpp"
#include "../plugin/IPlugin.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QLockFile>
#include <QRegularExpression>
#include "../Logging.hpp"
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

bool IpcServer::isExplicitlyStaleSocketError(QLocalSocket::LocalSocketError error)
{
    return error == QLocalSocket::ConnectionRefusedError
        || error == QLocalSocket::ServerNotFoundError;
}

bool IpcServer::acquireOwnership(const QString& socketPath)
{
    if (server_ || ownershipLock_) return false;

    // The lock closes the probe/remove race between two app processes. A
    // second current-version process fails here and therefore never touches
    // the first process's live socket. QLockFile also recovers a lock whose
    // recorded owner is no longer running.
    ownershipLock_ = std::make_unique<QLockFile>(socketPath + QStringLiteral(".lock"));
    ownershipLock_->setStaleLockTime(0);
    if (!ownershipLock_->tryLock(0)) {
        qCWarning(lcCore) << "IpcServer: Socket ownership is already held for"
                          << socketPath;
        ownershipLock_.reset();
        return false;
    }

    // A pre-lock-file application can own the socket without owning this
    // lock. Detect that legacy owner now, before the caller initializes
    // hardware. Explicit no-listener errors are safe to defer to the later
    // stale-path recovery; ambiguous errors fail closed and preserve the path.
    QLocalSocket probe;
    probe.connectToServer(socketPath);
    if (probe.waitForConnected(250)) {
        qCWarning(lcCore) << "IpcServer: A live legacy listener already owns"
                          << socketPath;
        ownershipLock_.reset();
        return false;
    }
    if (!isExplicitlyStaleSocketError(probe.error())) {
        qCWarning(lcCore) << "IpcServer: Early socket ownership probe was inconclusive for"
                          << socketPath << "— preserving pathname; error:"
                          << probe.errorString();
        ownershipLock_.reset();
        return false;
    }

    socketPath_ = socketPath;
    qCInfo(lcCore) << "IpcServer: Acquired socket ownership for" << socketPath_;
    return true;
}

bool IpcServer::startListening()
{
    if (server_ || !ownershipLock_ || socketPath_.isEmpty()) return false;

    server_ = new QLocalServer(this);
    server_->setSocketOptions(QLocalServer::WorldAccessOption);

    connect(server_, &QLocalServer::newConnection, this, &IpcServer::onNewConnection);

    if (!server_->listen(socketPath_)
        && server_->serverError() == QAbstractSocket::AddressInUseError) {
        // A pre-lock-file version of the application may still own the socket.
        // Probe it before removing anything. A refused connection means the
        // filesystem entry is genuinely stale and safe to replace.
        QLocalSocket probe;
        probe.connectToServer(socketPath_);
        if (probe.waitForConnected(250)) {
            qCWarning(lcCore) << "IpcServer: A live listener already owns"
                              << socketPath_;
        } else if (!isExplicitlyStaleSocketError(probe.error())) {
            qCWarning(lcCore) << "IpcServer: Socket ownership probe was inconclusive for"
                              << socketPath_ << "— preserving pathname; error:"
                              << probe.errorString();
        } else if (QLocalServer::removeServer(socketPath_)
                   && server_->listen(socketPath_)) {
            qCInfo(lcCore) << "IpcServer: Recovered stale socket" << socketPath_;
            return true;
        }
    }

    if (!server_->isListening()) {
        qCWarning(lcCore) << "IpcServer: Failed to listen on" << socketPath_
                   << "—" << server_->errorString();
        delete server_;
        server_ = nullptr;
        ownershipLock_.reset();
        socketPath_.clear();
        return false;
    }

    qCInfo(lcCore) << "IpcServer: Listening on" << socketPath_;
    return true;
}

bool IpcServer::start(const QString& socketPath)
{
    return acquireOwnership(socketPath) && startListening();
}

void IpcServer::stop()
{
    if (server_) {
        server_->close();
        delete server_;
        server_ = nullptr;
    }
    ownershipLock_.reset();
    socketPath_.clear();
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

void IpcServer::setInboundState(oap::api::ApiInboundState* state)
{
    inbound_ = state;
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
    if (command == QLatin1String("install_theme"))
        return handleInstallTheme(data);
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
    if (command == QLatin1String("get_logging"))
        return handleGetLogging();
    if (command == QLatin1String("set_logging"))
        return handleSetLogging(data);

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
    obj["protocol_capture_enabled"] =
        config_->valueByPath("connection.protocol_capture.enabled").toBool();
    obj["protocol_capture_format"] =
        config_->valueByPath("connection.protocol_capture.format").toString();
    obj["protocol_capture_include_media"] =
        config_->valueByPath("connection.protocol_capture.include_media").toBool();
    obj["protocol_capture_path"] =
        config_->valueByPath("connection.protocol_capture.path").toString();
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
    if (data.contains("protocol_capture_enabled"))
        config_->setValueByPath(
            "connection.protocol_capture.enabled",
            data.value("protocol_capture_enabled").toBool());
    if (data.contains("protocol_capture_format"))
        config_->setValueByPath(
            "connection.protocol_capture.format",
            data.value("protocol_capture_format").toString());
    if (data.contains("protocol_capture_include_media"))
        config_->setValueByPath(
            "connection.protocol_capture.include_media",
            data.value("protocol_capture_include_media").toBool());
    if (data.contains("protocol_capture_path"))
        config_->setValueByPath(
            "connection.protocol_capture.path",
            data.value("protocol_capture_path").toString());

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

QByteArray IpcServer::handleInstallTheme(const QVariantMap& data)
{
    if (!themeService_)
        return R"({"ok":false,"error":"Theme service not available"})";

    // Temp dir Flask writes the wallpaper into; the path in `data` must resolve here.
    // MUST match UPLOAD_TMP_DIR in web-config/server.py — a mismatch rejects every
    // wallpaper upload via parseThemeInstall's path-safety check. Keep in sync.
    const QString uploadDir = QStringLiteral("/tmp/oap-theme-upload");
    const ThemeInstallParseResult res = parseThemeInstall(data, uploadDir);
    if (!res.ok) {
        const QJsonObject o{{"ok", false}, {"error", res.error}};
        return QJsonDocument(o).toJson(QJsonDocument::Compact);
    }

    const ThemeInstallRequest& req = res.request;
    const bool ok = themeService_->importCompanionTheme(
        req.name, req.seed, req.dayColors, req.nightColors, req.wallpaperJpeg);

    QJsonObject o;
    o["ok"] = ok;
    if (ok)
        o["slug"] = ThemeService::slugify(req.name);
    else
        o["error"] = QStringLiteral("theme import failed");
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
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
    obj["version"] = QStringLiteral(OAP_VERSION);
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

    // Persist device selections immediately. Master volume is deliberately
    // NOT written here — the centralized debounced path (main.cpp, driven by
    // masterVolumeChanged) is the single writer for audio.master_volume; an
    // unconditional save here would flush the stale in-memory value first.
    if (config_) {
        bool persisted = false;
        bool persistOk = true;
        if (data.contains("output_device")) {
            persistOk = config_->setValueByPath("audio.output_device",
                            data.value("output_device").toString()) && persistOk;
            persisted = true;
        }
        if (data.contains("input_device")) {
            persistOk = config_->setValueByPath("audio.microphone.device",
                            data.value("input_device").toString()) && persistOk;
            persisted = true;
        }
        if (persisted) {
            persistOk = config_->save(configPath_) && persistOk;
            if (!persistOk)
                return R"({"ok":false,"error":"Failed to persist audio config"})";
        }
    }

    return R"({"ok":true})";
}

QByteArray IpcServer::handleCompanionStatus()
{
    // Companion phone state over API v1 (ApiInboundState). Key names predate
    // v1 and stay stable for the web-config panel's consumers.
    if (!inbound_) return R"({"error":"Companion service not available"})";

    QJsonObject obj;
    obj["connected"] = inbound_->connected();
    obj["gps_lat"] = inbound_->gpsLat();
    obj["gps_lon"] = inbound_->gpsLon();
    obj["gps_speed"] = inbound_->gpsSpeedMps();
    obj["gps_stale"] = inbound_->gpsStale();
    obj["battery"] = inbound_->phoneBattery();
    obj["charging"] = inbound_->phoneCharging();
    obj["internet"] = inbound_->internetAvailable();
    obj["proxy"] = inbound_->proxyAddress();
    obj["source"] = "api";
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QByteArray IpcServer::handleGetLogging()
{
    QJsonObject obj;
    obj["verbose"] = oap::isVerbose();

    // The category list is persisted configuration, rather than a Logging
    // runtime query, so the web panel can show exactly what will survive a
    // restart even while verbose currently overrides it.
    QJsonArray cats;
    if (config_) {
        for (const QString& cat : config_->loggingDebugCategories())
            cats.append(cat);
    }
    obj["debug_categories"] = cats;

    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QByteArray IpcServer::handleSetLogging(const QVariantMap& data)
{
    if (!config_)
        return R"({"ok":false,"error":"Config not available"})";

    bool changed = false;
    if (data.contains("verbose")) {
        bool verbose = data.value("verbose").toBool();
        if (!config_->setValueByPath("logging.verbose", verbose))
            return R"({"ok":false,"error":"Failed to write logging.verbose"})";
        if (verbose)
            oap::setVerbose(true);
        else
            oap::setDebugCategories(config_->loggingDebugCategories());
        changed = true;
        qCInfo(lcCore) << "Logging verbose set to" << verbose << "(via IPC)";
    }

    if (data.contains("categories")) {
        QStringList categories;
        for (const QVariant& v : data.value("categories").toList())
            categories.append(v.toString());
        oap::setDebugCategories(categories);
        // A category selection is the non-verbose logging mode. Persist both
        // sides of that decision so live behavior and restart behavior agree.
        config_->setLoggingDebugCategories(categories);
        config_->setLoggingVerbose(false);
        changed = true;
        qCInfo(lcCore) << "Logging debug categories set to" << categories << "(via IPC)";
    }

    if (changed && !config_->save(configPath_))
        return R"({"ok":false,"error":"Failed to persist logging config"})";

    return R"({"ok":true})";
}

} // namespace oap
