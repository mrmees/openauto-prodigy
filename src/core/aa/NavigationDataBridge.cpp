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
    connect(handler, &oaa::hu::NavigationChannelHandler::navigationStepChanged,
            this, &NavigationDataBridge::onNavigationStepChanged);
    connect(handler, &oaa::hu::NavigationChannelHandler::navigationDistanceChanged,
            this, &NavigationDataBridge::onNavigationDistanceChanged);
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
        instruction_.clear();
        phoneDistanceText_.clear();
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

void NavigationDataBridge::onNavigationStepChanged(const QString& instruction,
                                                     const QString& /*destination*/,
                                                     int maneuverType)
{
    // NavigationNotification (0x8006) — modern phones send this instead of
    // the deprecated NavigationTurnEvent (0x8004). Use instruction as road name
    // and update maneuver type. Only update if we haven't gotten a TurnEvent
    // with richer data (road name + turn icon) in this nav session.
    if (roadName_.isEmpty() || currentIcon_.isEmpty()) {
        // No TurnEvent data — use step instruction as road name
        roadName_ = instruction;
    }
    instruction_ = instruction;
    maneuverType_ = maneuverType;
    emit turnDataChanged();
}

void NavigationDataBridge::onNavigationDistanceChanged(const QString& displayText, int unit)
{
    // NavigationNextTurnDistanceEvent (0x8007) — phone sends numeric display_text
    // (e.g. "0", "0.3") plus distance_unit enum. We combine them.
    phoneDistanceText_ = displayText;
    if (unit != 0)
        distanceUnit_ = unit;
    emit distanceChanged();
}

QString NavigationDataBridge::formattedDistance() const
{
    // If we have phone-provided display text, combine with unit suffix
    if (!phoneDistanceText_.isEmpty()) {
        QString suffix = unitSuffix(distanceUnit_);
        if (!suffix.isEmpty())
            return phoneDistanceText_ + " " + suffix;
        return phoneDistanceText_;
    }

    // Fallback: compute from NavigationTurnEvent data (legacy phones)
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

QString NavigationDataBridge::unitSuffix(int distanceUnit)
{
    switch (distanceUnit) {
    case 1: return QStringLiteral("m");
    case 2: return QStringLiteral("km");
    case 3: return QStringLiteral("mi");
    case 4: return QStringLiteral("ft");
    case 5: return QStringLiteral("yd");
    case 6: return QStringLiteral("ft");  // UNKNOWN_6 observed as feet on US-locale phones
    default: return QString();
    }
}

} // namespace aa
} // namespace oap
