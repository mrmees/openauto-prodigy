#pragma once

// ApiInboundState — head-unit-side cache of the companion phone app's
// fire-and-forget reports (GPS fix, phone battery, internet/SOCKS5 proxy
// availability, wall-clock time). Reports are client -> server only and never
// answered; the request handler validates each report and pushes it here.
//
// Q_PROPERTY surface is consumed by QML widgets (companion status). The
// proxyRouteChanged / timeReported signals are the imperative side channels
// for plumbing that isn't a simple property (proxy route sync, RTC-less clock
// stepping). Lives on the Qt main thread.

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace oap::api {

class ApiInboundState : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool gpsValid READ gpsValid NOTIFY gpsChanged)
    Q_PROPERTY(double gpsLat READ gpsLat NOTIFY gpsChanged)
    Q_PROPERTY(double gpsLon READ gpsLon NOTIFY gpsChanged)
    Q_PROPERTY(double gpsSpeedMps READ gpsSpeedMps NOTIFY gpsChanged)
    Q_PROPERTY(int phoneBattery READ phoneBattery NOTIFY batteryChanged)
    Q_PROPERTY(bool phoneCharging READ phoneCharging NOTIFY batteryChanged)
    Q_PROPERTY(bool internetAvailable READ internetAvailable NOTIFY internetChanged)
    Q_PROPERTY(QString proxyAddress READ proxyAddress NOTIFY internetChanged)
public:
    explicit ApiInboundState(QObject* parent = nullptr);

    bool gpsValid() const { return gpsValid_; }
    double gpsLat() const { return gpsLat_; }
    double gpsLon() const { return gpsLon_; }
    double gpsSpeedMps() const { return gpsSpeedMps_; }
    int phoneBattery() const { return phoneBattery_; }
    bool phoneCharging() const { return phoneCharging_; }
    bool internetAvailable() const { return internetAvailable_; }
    QString proxyAddress() const { return proxyAddress_; }

    // Setters used by ApiRequestHandlers after validation.
    void setGps(double lat, double lon, double speedMps);
    void setBattery(int percent, bool charging);
    // active == SOCKS5 proxy exposed by the phone; the proxy HOST is this
    // connection's peer address. Composes proxyAddress "socks5://<host>:<port>"
    // and emits proxyRouteChanged for the route plumbing.
    void setConnectivity(const QString& peerHost, bool active, quint16 port,
                         const QString& password);
    void setTime(qint64 unixMs);

signals:
    void gpsChanged();
    void batteryChanged();
    void internetChanged();
    void timeReported(qint64 unixMs);
    void proxyRouteChanged(bool active, QString host, quint16 port, QString password);

private:
    bool gpsValid_ = false;
    double gpsLat_ = 0.0;
    double gpsLon_ = 0.0;
    double gpsSpeedMps_ = 0.0;
    int phoneBattery_ = -1;
    bool phoneCharging_ = false;
    bool internetAvailable_ = false;
    QString proxyAddress_;
};

} // namespace oap::api
