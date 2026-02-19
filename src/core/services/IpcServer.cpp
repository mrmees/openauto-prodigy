#include "IpcServer.hpp"
#include "../YamlConfig.hpp"
#include "ThemeService.hpp"
#include "../plugin/PluginManager.hpp"
#include "../plugin/IPlugin.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>

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

void IpcServer::setPluginManager(PluginManager* pluginManager)
{
    pluginManager_ = pluginManager;
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
    // Return current theme as JSON (placeholder — theme file reading)
    QJsonObject obj;
    obj["day"] = QJsonObject();
    obj["night"] = QJsonObject();
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QByteArray IpcServer::handleSetTheme(const QVariantMap& data)
{
    Q_UNUSED(data)
    // TODO: Write theme YAML and reload ThemeService
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

} // namespace oap
