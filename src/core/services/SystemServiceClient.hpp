#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QJsonObject>
#include <QMap>
#include <functional>

namespace oap {

class SystemServiceClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(QJsonObject health READ health NOTIFY healthChanged)

public:
    explicit SystemServiceClient(QObject* parent = nullptr);

    bool isConnected() const;
    QJsonObject health() const;

    Q_INVOKABLE void getHealth();
    Q_INVOKABLE void applyConfig(const QString& section);
    Q_INVOKABLE void restartService(const QString& name);
    Q_INVOKABLE void getStatus();

signals:
    void connectedChanged();
    void healthChanged();
    void configApplied(const QString& section, bool ok, const QString& error);
    void serviceRestarted(const QString& name, bool ok);
    void statusReceived(const QJsonObject& status);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QLocalSocket::LocalSocketError error);

private:
    void connectToService();
    void sendRequest(const QString& method, const QJsonObject& params = {});
    void handleResponse(const QJsonObject& response);

    QLocalSocket* socket_ = nullptr;
    QByteArray readBuffer_;
    QJsonObject health_;
    int nextId_ = 1;
    QMap<QString, QString> pendingMethods_;  // id -> method name
};

} // namespace oap
