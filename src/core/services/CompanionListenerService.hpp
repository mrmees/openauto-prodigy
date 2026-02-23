#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QByteArray>

namespace oap {

class CompanionListenerService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(double gpsLat READ gpsLat NOTIFY gpsChanged)
    Q_PROPERTY(double gpsLon READ gpsLon NOTIFY gpsChanged)
    Q_PROPERTY(double gpsSpeed READ gpsSpeed NOTIFY gpsChanged)
    Q_PROPERTY(double gpsAccuracy READ gpsAccuracy NOTIFY gpsChanged)
    Q_PROPERTY(double gpsBearing READ gpsBearing NOTIFY gpsChanged)
    Q_PROPERTY(bool gpsStale READ isGpsStale NOTIFY gpsChanged)
    Q_PROPERTY(int phoneBattery READ phoneBattery NOTIFY batteryChanged)
    Q_PROPERTY(bool phoneCharging READ isPhoneCharging NOTIFY batteryChanged)
    Q_PROPERTY(bool internetAvailable READ isInternetAvailable NOTIFY internetChanged)
    Q_PROPERTY(QString proxyAddress READ proxyAddress NOTIFY internetChanged)

public:
    explicit CompanionListenerService(QObject* parent = nullptr);
    ~CompanionListenerService() override;

    bool start(int port = 9876);
    void stop();
    bool isListening() const;

    void setSharedSecret(const QString& secret);

    // Q_PROPERTY getters
    bool isConnected() const;
    double gpsLat() const;
    double gpsLon() const;
    double gpsSpeed() const;
    double gpsAccuracy() const;
    double gpsBearing() const;
    bool isGpsStale() const;
    int phoneBattery() const;
    bool isPhoneCharging() const;
    bool isInternetAvailable() const;
    QString proxyAddress() const;

signals:
    void connectedChanged();
    void gpsChanged();
    void batteryChanged();
    void internetChanged();
    void timeAdjusted(qint64 oldTimeMs, qint64 newTimeMs, qint64 deltaMs);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    void sendChallenge();
    bool validateHello(const QJsonObject& msg);
    void handleStatus(const QJsonObject& msg);
    bool verifyMac(const QJsonObject& msg);
    void adjustClock(qint64 phoneTimeMs);
    QByteArray computeHmac(const QByteArray& key, const QByteArray& data);

    QTcpServer* server_ = nullptr;
    QTcpSocket* client_ = nullptr;
    QString sharedSecret_;
    QByteArray sessionKey_;
    QByteArray currentNonce_;
    qint64 lastSeq_ = -1;

    // State
    double gpsLat_ = 0.0;
    double gpsLon_ = 0.0;
    double gpsSpeed_ = 0.0;
    double gpsAccuracy_ = 0.0;
    double gpsBearing_ = 0.0;
    int gpsAgeMs_ = -1;
    int phoneBattery_ = -1;
    bool phoneCharging_ = false;
    bool internetAvailable_ = false;
    QString proxyAddress_;

    // Time safety
    int backwardJumpCount_ = 0;
    qint64 lastBackwardTarget_ = 0;
};

} // namespace oap
