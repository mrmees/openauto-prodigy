#include "core/api/ApiInboundState.hpp"

namespace oap::api {

ApiInboundState::ApiInboundState(QObject* parent) : QObject(parent) {}

void ApiInboundState::setGps(double lat, double lon, double speedMps) {
    gpsValid_ = true;
    gpsLat_ = lat;
    gpsLon_ = lon;
    gpsSpeedMps_ = speedMps;
    emit gpsChanged();
}

void ApiInboundState::setBattery(int percent, bool charging) {
    phoneBattery_ = percent;
    phoneCharging_ = charging;
    emit batteryChanged();
}

void ApiInboundState::setConnectivity(const QString& peerHost, bool active,
                                      quint16 port, const QString& password) {
    internetAvailable_ = active;
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

} // namespace oap::api
