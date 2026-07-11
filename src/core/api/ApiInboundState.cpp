#include "core/api/ApiInboundState.hpp"

namespace oap::api {

ApiInboundState::ApiInboundState(QObject* parent) : QObject(parent) {
    staleTicker_.setInterval(5000);
    QObject::connect(&staleTicker_, &QTimer::timeout, this, [this] {
        // While a fix is held, re-emit so QML gpsStale bindings re-evaluate
        // (the fix crosses the staleness boundary without a new report).
        if (gpsValid_)
            emit gpsChanged();
    });
}

bool ApiInboundState::gpsStale() const {
    if (!gpsValid_)
        return true;   // no fix yet -> treated as stale
    const qint64 effectiveAgeMs =
        fixTimer_.elapsed() + static_cast<qint64>(fixAgeMs_);
    return effectiveAgeMs > static_cast<qint64>(staleThresholdMs_);
}

void ApiInboundState::setGps(double lat, double lon, double speedMps,
                             double bearingDeg, double accuracyM, quint32 ageMs) {
    gpsValid_ = true;
    gpsLat_ = lat;
    gpsLon_ = lon;
    gpsSpeedMps_ = speedMps;
    gpsBearing_ = bearingDeg;
    gpsAccuracy_ = accuracyM;
    fixAgeMs_ = ageMs;
    fixTimer_.restart();
    if (!staleTicker_.isActive())
        staleTicker_.start();
    emit gpsChanged();
}

void ApiInboundState::clearGps() {
    staleTicker_.stop();
    fixTimer_.invalidate();
    fixAgeMs_ = 0;
    const bool wasValid = gpsValid_;
    gpsValid_ = false;
    // Last lat/lon are left in place; gpsValid/gpsStale gate any read. Emit so
    // a binding on gpsValid/gpsStale re-evaluates when the owner drops.
    if (wasValid)
        emit gpsChanged();
}

void ApiInboundState::setBattery(int percent, bool charging) {
    phoneBattery_ = percent;
    phoneCharging_ = charging;
    emit batteryChanged();
}

void ApiInboundState::clearBattery() {
    phoneBattery_ = -1;
    phoneCharging_ = false;
    emit batteryChanged();
}

void ApiInboundState::setOwnerPresent(bool present) {
    if (connected_ == present)
        return;
    connected_ = present;
    emit connectedChanged();
}

void ApiInboundState::setConnectivity(const QString& peerHost, bool active,
                                      quint16 port, const QString& password) {
    internetAvailable_ = active;
    proxyActive_ = active;
    if (active)
        // Compose the head unit's SOCKS5 route: the proxy host is the phone's
        // (this connection's) peer address (CompanionListenerService.cpp:448).
        proxyAddress_ = QStringLiteral("socks5://%1:%2").arg(peerHost).arg(port);
    else
        proxyAddress_.clear();

    emit internetChanged();
    emit proxyRouteChanged(active, peerHost, port, password);
}

void ApiInboundState::setTime(qint64 unixMs) {
    // RTC-less head unit: no stored property, just the imperative sync signal.
    emit timeReported(unixMs);
}

void ApiInboundState::setTimezone(const QString& ianaId) {
    // No stored property here either -- same store-nothing style as setTime().
    emit timezoneReported(ianaId);
}

} // namespace oap::api
