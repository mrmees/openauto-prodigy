#include "NavigationDataBridge.hpp"
#include "ManeuverIconProvider.hpp"
#include <oaa/HU/Handlers/NavigationChannelHandler.hpp>
#include <QtMath>

namespace oap {
namespace aa {

NavigationDataBridge::NavigationDataBridge(QObject* parent)
    : QObject(parent)
{
}

void NavigationDataBridge::connectToHandler(oaa::hu::NavigationChannelHandler* handler)
{
    // Use Qt::DirectConnection for tests; in production the handler emits from
    // ASIO threads -- the orchestrator should bridge to the main thread before
    // these reach us, OR we use QueuedConnection. For safety, use AutoConnection
    // which becomes Queued when sender lives on a different thread.
    connect(handler, &oaa::hu::NavigationChannelHandler::navigationStateChanged,
            this, &NavigationDataBridge::onNavigationStateChanged);
    connect(handler, &oaa::hu::NavigationChannelHandler::navigationTurnEvent,
            this, &NavigationDataBridge::onNavigationTurnEvent);
}

void NavigationDataBridge::setManeuverIconProvider(ManeuverIconProvider* provider)
{
    iconProvider_ = provider;
}

void NavigationDataBridge::onNavigationStateChanged(bool active)
{
    if (navActive_ == active)
        return;

    navActive_ = active;

    if (!active) {
        // Clear cached turn data
        roadName_.clear();
        maneuverType_ = 0;
        turnDirection_ = 0;
        distanceMeters_ = 0;
        distanceUnit_ = 0;
        currentIcon_.clear();
        if (iconProvider_)
            iconProvider_->updateIcon(QByteArray());
        emit turnDataChanged();
    }

    emit navActiveChanged();
}

void NavigationDataBridge::onNavigationTurnEvent(const QString& roadName, int maneuverType,
                                                  int turnDirection, const QByteArray& turnIcon,
                                                  int distanceMeters, int distanceUnit)
{
    roadName_ = roadName;
    maneuverType_ = maneuverType;
    turnDirection_ = turnDirection;
    distanceMeters_ = distanceMeters;
    distanceUnit_ = distanceUnit;

    if (!turnIcon.isEmpty()) {
        currentIcon_ = turnIcon;
        ++iconVersion_;
        if (iconProvider_)
            iconProvider_->updateIcon(turnIcon);
    }

    emit turnDataChanged();
}

QString NavigationDataBridge::formattedDistance() const
{
    // distance_unit from phone: 1=meters, 2=km, 3=miles, 4=feet, 5=yards
    // distanceMeters_ is always in meters regardless of unit
    switch (distanceUnit_) {
    case 1: // meters -- show km above 1000m
        if (distanceMeters_ >= 1000)
            return QString::number(distanceMeters_ / 1000.0, 'f', 1) + " km";
        return QString::number(distanceMeters_) + " m";
    case 2: // km
        return QString::number(distanceMeters_ / 1000.0, 'f', 1) + " km";
    case 3: // miles
        return QString::number(distanceMeters_ / 1609.34, 'f', 1) + " mi";
    case 4: // feet
        return QString::number(qRound(distanceMeters_ * 3.28084)) + " ft";
    case 5: // yards
        return QString::number(qRound(distanceMeters_ / 0.9144)) + " yd";
    default:
        return QString::number(distanceMeters_) + " m";
    }
}

} // namespace aa
} // namespace oap
