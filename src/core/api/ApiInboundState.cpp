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
    // Reset ALL fields, not just the valid flag (legacy parity): unconditional
    // readers such as IpcServer::handleCompanionStatus return lat/lon/etc.
    // directly, so retained coordinates would leak after the owner drops.
    gpsLat_ = 0.0;
    gpsLon_ = 0.0;
    gpsSpeedMps_ = 0.0;
    gpsBearing_ = 0.0;
    gpsAccuracy_ = 0.0;
    const bool wasValid = gpsValid_;
    gpsValid_ = false;
    // Emit once so bindings on gpsValid/gpsStale re-evaluate when the owner
    // drops; idempotent — a clear on already-invalid state emits nothing.
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
    const QString address = active
        ? QStringLiteral("socks5://%1:%2").arg(peerHost).arg(port)
        : QString();
    const bool observableChanged = internetAvailable_ != active
        || proxyActive_ != active || proxyAddress_ != address;
    const bool routeChanged = !hasConnectivityTuple_
        || proxyHost_ != peerHost || proxyActive_ != active
        || proxyPort_ != port || proxyPassword_ != password;

    internetAvailable_ = active;
    proxyActive_ = active;
    proxyAddress_ = address;
    hasConnectivityTuple_ = true;
    proxyHost_ = peerHost;
    proxyPort_ = port;
    proxyPassword_ = password;

    if (observableChanged)
        emit internetChanged();
    if (routeChanged)
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
