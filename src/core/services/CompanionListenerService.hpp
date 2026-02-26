#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QByteArray>
#include <QUuid>
#include "SystemServiceClient.hpp"

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
    Q_PROPERTY(QString qrCodeDataUri READ qrCodeDataUri NOTIFY qrCodeChanged)

public:
    explicit CompanionListenerService(QObject* parent = nullptr);
    ~CompanionListenerService() override;

    bool start(int port = 9876);
    void stop();
    bool isListening() const;

    void setSharedSecret(const QString& secret);
    void setWifiSsid(const QString& ssid) { wifiSsid_ = ssid; }
    void setSystemServiceClient(SystemServiceClient* client) { systemClient_ = client; }

    /// Load vehicle_id from ~/.openauto/vehicle.id, or generate a new UUID v4.
    void loadOrGenerateVehicleId();
    QString vehicleId() const { return vehicleId_; }

    /// Generate a 6-digit pairing PIN and derive+store shared secret.
    /// Returns the PIN string for display to the user.
    Q_INVOKABLE QString generatePairingPin();

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
    QString qrCodeDataUri() const;

signals:
    void connectedChanged();
    void gpsChanged();
    void batteryChanged();
    void internetChanged();
    void qrCodeChanged();
    void timeAdjusted(qint64 oldTimeMs, qint64 newTimeMs, qint64 deltaMs);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    void sendChallenge();
    bool validateHello(const QJsonObject& msg);
    void handleStatus(const QJsonObject& msg);
    bool verifyMac(const QJsonObject& msg, const QByteArray& rawLine);
    void adjustClock(qint64 phoneTimeMs);
    QByteArray computeHmac(const QByteArray& key, const QByteArray& data);
    QByteArray generateQrPng(const QString& payload);
    QString socks5Password() const;

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
    QString qrCodeDataUri_;
    QString wifiSsid_ = QStringLiteral("OpenAutoProdigy");
    SystemServiceClient* systemClient_ = nullptr;
    QString vehicleId_;
    int listenPort_ = 9876;

    // Time safety
    int backwardJumpCount_ = 0;
    qint64 lastBackwardTarget_ = 0;
};

} // namespace oap
