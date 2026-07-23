#pragma once

// ApiInboundState — head-unit-side cache of the companion phone app's
// fire-and-forget reports (GPS fix, phone battery, internet/SOCKS5 proxy
// availability, wall-clock time). Reports are client -> server only and never
// answered; the request handler validates each report and pushes it here.
//
// Q_PROPERTY surface is consumed by QML widgets (companion status); the names
// are kept from the retired legacy listener for QML/IPC stability (§B0). The
// proxyRouteChanged / timeReported signals are the imperative side channels
// for plumbing that isn't a simple property (proxy route sync, RTC-less clock
// stepping). Lives on the Qt main thread.

#include <QObject>
#include <QString>
#include <QElapsedTimer>
#include <QTimer>
#include <QtGlobal>

namespace oap::api {

class ApiInboundState : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)        // any live report owner
    Q_PROPERTY(bool gpsValid READ gpsValid NOTIFY gpsChanged)
    Q_PROPERTY(bool gpsStale READ gpsStale NOTIFY gpsChanged)
    Q_PROPERTY(double gpsLat READ gpsLat NOTIFY gpsChanged)
    Q_PROPERTY(double gpsLon READ gpsLon NOTIFY gpsChanged)
    Q_PROPERTY(double gpsSpeed READ gpsSpeedMps NOTIFY gpsChanged)           // legacy alias
    Q_PROPERTY(double gpsSpeedMps READ gpsSpeedMps NOTIFY gpsChanged)
    Q_PROPERTY(double gpsAccuracy READ gpsAccuracy NOTIFY gpsChanged)
    Q_PROPERTY(double gpsBearing READ gpsBearing NOTIFY gpsChanged)
    Q_PROPERTY(int phoneBattery READ phoneBattery NOTIFY batteryChanged)
    Q_PROPERTY(bool phoneCharging READ phoneCharging NOTIFY batteryChanged)
    Q_PROPERTY(bool internetAvailable READ internetAvailable NOTIFY internetChanged)
    Q_PROPERTY(bool proxyActive READ proxyActive NOTIFY internetChanged)     // NEW (fixes dead proxyStatus read)
    Q_PROPERTY(QString proxyAddress READ proxyAddress NOTIFY internetChanged)
public:
    explicit ApiInboundState(QObject* parent = nullptr);

    bool connected() const { return connected_; }
    bool gpsValid() const { return gpsValid_; }
    // Stale = no fix yet, or effective age (wall-clock since receipt + the
    // phone-reported age at receipt) exceeds the staleness window.
    bool gpsStale() const;
    double gpsLat() const { return gpsLat_; }
    double gpsLon() const { return gpsLon_; }
    double gpsSpeedMps() const { return gpsSpeedMps_; }
    double gpsAccuracy() const { return gpsAccuracy_; }
    double gpsBearing() const { return gpsBearing_; }
    int phoneBattery() const { return phoneBattery_; }
    bool phoneCharging() const { return phoneCharging_; }
    bool internetAvailable() const { return internetAvailable_; }
    bool proxyActive() const { return proxyActive_; }
    QString proxyAddress() const { return proxyAddress_; }

    // Test seam: shrink the 30 s staleness window so tests need not sleep.
    void setStaleThresholdMs(int ms) { staleThresholdMs_ = ms; }

    // Setters used by ApiRequestHandlers after validation.
    void setGps(double lat, double lon, double speedMps,
                double bearingDeg, double accuracyM, quint32 ageMs);
    void setBattery(int percent, bool charging);
    // active == SOCKS5 proxy exposed by the phone; the proxy HOST is this
    // connection's peer address. Composes proxyAddress "socks5://<host>:<port>"
    // and emits proxyRouteChanged for the route plumbing.
    void setConnectivity(const QString& peerHost, bool active, quint16 port,
                         const QString& password);
    void setTime(qint64 unixMs);
    // ianaId is already validated (QTimeZone::isTimeZoneIdAvailable) by the
    // caller (ApiRequestHandlers) -- mirrors setTime()'s store-nothing style.
    void setTimezone(const QString& ianaId);

    // Owner-disconnect clears: the companion session that sourced a report
    // closed, so its cached state is no longer trustworthy.
    void clearGps();       // gpsValid=false, stops the stale ticker, gpsChanged
    void clearBattery();   // phoneBattery=-1, batteryChanged
    // Presence of any live report owner; drives `connected`. Handlers call it
    // after (re)computing whether any report type still has an owning session.
    void setOwnerPresent(bool present);

signals:
    void connectedChanged();
    void gpsChanged();
    void batteryChanged();
    void internetChanged();
    void timeReported(qint64 unixMs);
    void timezoneReported(const QString& ianaId);
    void proxyRouteChanged(bool active, QString host, quint16 port, QString password);

private:
    bool connected_ = false;
    bool gpsValid_ = false;
    double gpsLat_ = 0.0;
    double gpsLon_ = 0.0;
    double gpsSpeedMps_ = 0.0;
    double gpsAccuracy_ = 0.0;
    double gpsBearing_ = 0.0;
    int phoneBattery_ = -1;
    bool phoneCharging_ = false;
    bool internetAvailable_ = false;
    bool proxyActive_ = false;
    QString proxyAddress_;
    bool hasConnectivityTuple_ = false;
    QString proxyHost_;
    quint16 proxyPort_ = 0;
    QString proxyPassword_;

    // Staleness: fixTimer_ is restarted on each accepted fix and fixAgeMs_
    // carries the phone-reported age at receipt; effective age =
    // fixTimer_.elapsed() + fixAgeMs_. staleTicker_ re-emits gpsChanged every
    // 5 s while a fix is held so QML gpsStale bindings flip without a new
    // report; it is stopped on clearGps.
    QElapsedTimer fixTimer_;
    quint32 fixAgeMs_ = 0;
    int staleThresholdMs_ = 30000;
    QTimer staleTicker_;
};

} // namespace oap::api
