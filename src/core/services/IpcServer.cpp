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
#include <QPointer>
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
    clients_.clear();
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
        clients_.insert(socket, {});
        connect(socket, &QLocalSocket::readyRead, this, &IpcServer::onReadyRead);
        connect(socket, &QLocalSocket::disconnected, this, &IpcServer::onDisconnected);
    }
}

void IpcServer::onReadyRead()
{
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket) return;

    auto it = clients_.find(socket);
    if (it == clients_.end())
        it = clients_.insert(socket, {});
    if (it->continuationQueued)
        return;

    processClient(socket);
}

void IpcServer::processClient(QLocalSocket* socket)
{
    auto it = clients_.find(socket);
    if (it == clients_.end())
        return;
    it->continuationQueued = false;

    int framesRemaining = MaxFramesPerTurn;
    while (framesRemaining > 0) {
        qsizetype newline = it->input.indexOf('\n');
        if (newline >= 0) {
            if (newline > MaxFrameSize) {
                closeClient(socket, "oversized request frame");
                return;
            }

            const QByteArray request = it->input.left(newline);
            it->input.remove(0, newline + 1);
            const QByteArray response = handleRequest(request) + '\n';
            if (socket->bytesToWrite() + response.size() > MaxPendingOutput) {
                closeClient(socket, "pending response limit exceeded");
                return;
            }
            if (socket->write(response) != response.size()) {
                closeClient(socket, "response write failed");
                return;
            }
            --framesRemaining;
            continue;
        }

        if (it->input.size() > MaxFrameSize) {
            closeClient(socket, "oversized partial request");
            return;
        }
        if (socket->bytesAvailable() <= 0)
            break;

        // Retain no more than one byte beyond the per-frame payload limit.
        // That byte may be the terminating newline; any other byte proves the
        // current partial frame is oversized.
        const qint64 readLimit = static_cast<qint64>(MaxFrameSize + 1 - it->input.size());
        const QByteArray chunk = socket->read(readLimit);
        if (chunk.isEmpty())
            break;
        it->input.append(chunk);
    }

    socket->flush();

    // Yield after a bounded number of requests even when one client supplied
    // a large coalesced batch. Other sockets and UI work then get an event-loop
    // turn before this client's remaining frames are processed.
    if (it != clients_.end()
        && (it->input.contains('\n') || socket->bytesAvailable() > 0)) {
        scheduleClientContinuation(socket);
    }
}

void IpcServer::scheduleClientContinuation(QLocalSocket* socket)
{
    auto it = clients_.find(socket);
    if (it == clients_.end() || it->continuationQueued)
        return;
    it->continuationQueued = true;
    const QPointer<QLocalSocket> guardedSocket(socket);
    QMetaObject::invokeMethod(this, [this, guardedSocket] {
        if (guardedSocket)
            processClient(guardedSocket);
    }, Qt::QueuedConnection);
}

void IpcServer::closeClient(QLocalSocket* socket, const char* reason)
{
    qCWarning(lcCore) << "IpcServer: Closing client:" << reason;
    clients_.remove(socket);
    socket->abort();
}

void IpcServer::onDisconnected()
{
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (socket) {
        clients_.remove(socket);
        socket->deleteLater();
    }
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

    const bool hasVerbose = data.contains("verbose");
    const bool hasCategories = data.contains("categories");
    if (!hasVerbose && !hasCategories)
        return R"({"ok":false,"error":"Logging request must include verbose or categories"})";

    for (auto it = data.cbegin(); it != data.cend(); ++it) {
        if (it.key() != QLatin1String("verbose") && it.key() != QLatin1String("categories"))
            return R"({"ok":false,"error":"Unrecognized logging request field"})";
    }

    bool verbose = false;
    if (hasVerbose) {
        const QVariant verboseValue = data.value("verbose");
        if (verboseValue.typeId() != QMetaType::Bool)
            return R"({"ok":false,"error":"logging.verbose must be a boolean"})";
        verbose = verboseValue.toBool();
    }

    QStringList categories;
    if (hasCategories) {
        const QVariant categoriesValue = data.value("categories");
        if (categoriesValue.typeId() != QMetaType::QVariantList)
            return R"({"ok":false,"error":"logging.categories must be a list"})";

        const QVariantList categoryValues = categoriesValue.toList();
        for (const QVariant& category : categoryValues) {
            if (category.typeId() != QMetaType::QString)
                return R"({"ok":false,"error":"logging.categories entries must be strings"})";
            categories.append(category.toString());
        }

        QString categoryError;
        if (!oap::validateDebugCategories(categories, &categoryError)) {
            QJsonObject response;
            response["ok"] = false;
            response["error"] = categoryError;
            return QJsonDocument(response).toJson(QJsonDocument::Compact);
        }
    }

    // Validate every field before modifying configuration or runtime logging.
    // A category selection is the non-verbose mode even when verbose is also
    // present, so it deterministically wins without a temporary mutation.
    if (hasCategories) {
        // A category selection is the non-verbose logging mode. Persist both
        // sides of that decision so live behavior and restart behavior agree.
        config_->setLoggingDebugCategories(categories);
        config_->setLoggingVerbose(false);
        qCInfo(lcCore) << "Logging debug categories set to" << categories << "(via IPC)";
    } else {
        if (!config_->setValueByPath("logging.verbose", verbose))
            return R"({"ok":false,"error":"Failed to write logging.verbose"})";
        qCInfo(lcCore) << "Logging verbose set to" << verbose << "(via IPC)";
    }

    oap::applyLoggingPolicy(hasCategories ? false : verbose,
                            hasCategories ? categories : config_->loggingDebugCategories());

    if (!config_->save(configPath_))
        return R"({"ok":false,"error":"Failed to persist logging config"})";

    return R"({"ok":true})";
}

} // namespace oap
