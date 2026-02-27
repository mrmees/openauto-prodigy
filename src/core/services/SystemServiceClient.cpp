#include "SystemServiceClient.hpp"
#include <QJsonDocument>
#include <QTimer>

namespace oap {

static const QString SOCKET_PATH = QStringLiteral("/run/openauto/system.sock");

SystemServiceClient::SystemServiceClient(QObject* parent)
    : QObject(parent)
{
    socket_ = new QLocalSocket(this);
    connect(socket_, &QLocalSocket::connected, this, &SystemServiceClient::onConnected);
    connect(socket_, &QLocalSocket::disconnected, this, &SystemServiceClient::onDisconnected);
    connect(socket_, &QLocalSocket::readyRead, this, &SystemServiceClient::onReadyRead);
    connect(socket_, &QLocalSocket::errorOccurred, this, &SystemServiceClient::onError);

    connectToService();
}

bool SystemServiceClient::isConnected() const
{
    return socket_->state() == QLocalSocket::ConnectedState;
}

QJsonObject SystemServiceClient::health() const { return health_; }

void SystemServiceClient::getHealth() { sendRequest("get_health"); }
void SystemServiceClient::getStatus() { sendRequest("get_status"); }

void SystemServiceClient::applyConfig(const QString& section)
{
    QJsonObject params;
    params["section"] = section;
    sendRequest("apply_config", params);
}

void SystemServiceClient::restartService(const QString& name)
{
    QJsonObject params;
    params["name"] = name;
    sendRequest("restart_service", params);
}

QString SystemServiceClient::routeState() const { return routeState_; }
QString SystemServiceClient::routeError() const { return routeError_; }

void SystemServiceClient::setProxyRoute(bool active, const QString& host, int port,
                                       const QString& password)
{
    QJsonObject params;
    params["active"] = active;
    if (active) {
        params["host"] = host;
        params["port"] = port;
        params["user"] = QStringLiteral("oap");
        params["password"] = password;
    }
    sendRequest("set_proxy_route", params);
}

void SystemServiceClient::getProxyStatus()
{
    sendRequest("get_proxy_status");
}

void SystemServiceClient::connectToService()
{
    if (socket_->state() != QLocalSocket::UnconnectedState)
        return;
    socket_->connectToServer(SOCKET_PATH);
}

void SystemServiceClient::onConnected()
{
    qInfo() << "SystemServiceClient: connected to daemon";
    emit connectedChanged();
    getHealth();
}

void SystemServiceClient::onDisconnected()
{
    qInfo() << "SystemServiceClient: disconnected from daemon";
    emit connectedChanged();
    // Retry after 5 seconds
    QTimer::singleShot(5000, this, &SystemServiceClient::connectToService);
}

void SystemServiceClient::onError(QLocalSocket::LocalSocketError error)
{
    if (error == QLocalSocket::ServerNotFoundError ||
        error == QLocalSocket::ConnectionRefusedError) {
        // Daemon not running yet, retry
        QTimer::singleShot(5000, this, &SystemServiceClient::connectToService);
    }
}

void SystemServiceClient::onReadyRead()
{
    readBuffer_ += socket_->readAll();
    while (true) {
        int idx = readBuffer_.indexOf('\n');
        if (idx < 0) break;
        QByteArray line = readBuffer_.left(idx);
        readBuffer_ = readBuffer_.mid(idx + 1);

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error == QJsonParseError::NoError)
            handleResponse(doc.object());
    }
}

void SystemServiceClient::sendRequest(const QString& method, const QJsonObject& params)
{
    if (!isConnected()) return;

    QString id = QString::number(nextId_++);
    pendingMethods_[id] = method;

    QJsonObject msg;
    msg["id"] = id;
    msg["method"] = method;
    if (!params.isEmpty())
        msg["params"] = params;

    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n";
    socket_->write(data);
    socket_->flush();
}

void SystemServiceClient::handleResponse(const QJsonObject& response)
{
    QString id = response["id"].toString();
    QString method = pendingMethods_.take(id);

    if (method == "get_health") {
        health_ = response["result"].toObject();
        emit healthChanged();
    } else if (method == "apply_config") {
        QJsonObject result = response.contains("error")
            ? response["error"].toObject()
            : response["result"].toObject();
        bool ok = result["ok"].toBool();
        QString error = result.value("message").toString(result.value("error").toString());
        emit configApplied(method, ok, error);
    } else if (method == "restart_service") {
        QJsonObject result = response["result"].toObject();
        emit serviceRestarted(result["name"].toString(), result["ok"].toBool());
    } else if (method == "get_status") {
        emit statusReceived(response["result"].toObject());
    } else if (method == "set_proxy_route" || method == "get_proxy_status") {
        QJsonObject result = response["result"].toObject();
        QString newState = result["state"].toString(routeState_);
        QString newError = result.value("error").toString();
        if (newState != routeState_ || newError != routeError_) {
            qInfo() << "[system-service] route state update:"
                    << method
                    << "state=" << newState
                    << "error=" << (newError.isEmpty() ? "<none>" : newError);
        }
        if (newState != routeState_ || newError != routeError_) {
            routeState_ = newState;
            routeError_ = newError;
            emit routeChanged();
        }
    }
}

} // namespace oap
