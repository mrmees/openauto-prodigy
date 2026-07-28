#include "NavigationDataBridge.hpp"
#include "ManeuverIconProvider.hpp"
#include <oaa/HU/Handlers/NavigationChannelHandler.hpp>
#include <QtMath>

namespace oap {
namespace aa {

NavigationDataBridge::NavigationDataBridge(QObject* parent)
    : INavigationProvider(parent)
    , laneModel_(std::make_unique<NavigationLaneModel>())
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
    connect(handler,
            &oaa::hu::NavigationChannelHandler::navigationLaneGuidanceChanged,
            this, &NavigationDataBridge::onNavigationLaneGuidanceChanged);
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
        hasDistance_ = false;
        currentIcon_.clear();
        laneModel_->clear();
        if (iconProvider_)
            iconProvider_->updateIcon(QByteArray());
        emit turnDataChanged();
        emit distanceChanged();
        emit laneGuidanceChanged();
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
    if (distanceMeters >= 0)
        hasDistance_ = true;

    if (!turnIcon.isEmpty()) {
        currentIcon_ = turnIcon;
        ++iconVersion_;
        if (iconProvider_)
            iconProvider_->updateIcon(turnIcon);
    }

    emit turnDataChanged();
    emit distanceChanged();
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
    if (!displayText.isEmpty())
        hasDistance_ = true;
    if (unit != 0)
        distanceUnit_ = unit;
    emit distanceChanged();
}

void NavigationDataBridge::onNavigationLaneGuidanceChanged(
    const oaa::hu::NavigationLaneGuidance& lanes)
{
    LanePresentationList presentation;
    presentation.reserve(lanes.size());
    for (const auto& lane : lanes) {
        LanePresentation directions;
        directions.reserve(lane.directions.size());
        for (const auto& direction : lane.directions) {
            directions.append({
                laneShapeToken(direction.shape),
                direction.recommended,
            });
        }
        presentation.append(directions);
    }

    laneModel_->replaceLanes(presentation);
    emit laneGuidanceChanged();
}

bool NavigationDataBridge::hasLaneGuidance() const
{
    return laneModel_->rowCount() > 0;
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
    // Uses same AA Distance.displayUnit enum as above
    // Audited AA 17.3 DistanceDisplayUnit enum:
    // 1=METERS, 2/3=KILOMETERS, 4/5=MILES, 6=FEET, 7=YARDS.
    switch (distanceUnit_) {
    case 1: // METERS
        if (distanceMeters_ >= 1000)
            return QString::number(distanceMeters_ / 1000.0, 'f', 1) + " km";
        return QString::number(distanceMeters_) + " m";
    case 2: // KILOMETERS
    case 3: // KILOMETERS_P1
        return QString::number(distanceMeters_ / 1000.0, 'f', 1) + " km";
    case 4: // MILES
    case 5: // MILES_P1
        return QString::number(distanceMeters_ / 1609.34, 'f', 1) + " mi";
    case 6: // FEET
        return QString::number(qRound(distanceMeters_ * 3.28084)) + " ft";
    case 7: // YARDS
        return QString::number(qRound(distanceMeters_ / 0.9144)) + " yd";
    default:
        return QString::number(distanceMeters_) + " m";
    }
}

QString NavigationDataBridge::unitSuffix(int distanceUnit)
{
    // Audited AA 17.3 DistanceDisplayUnit enum. The P1 variants select
    // one-decimal phone formatting but retain the same unit suffix.
    // display_text already has correct precision, so we just need the suffix.
    switch (distanceUnit) {
    case 1: return QStringLiteral("m");
    case 2: return QStringLiteral("km");
    case 3: return QStringLiteral("km");
    case 4: return QStringLiteral("mi");
    case 5: return QStringLiteral("mi");
    case 6: return QStringLiteral("ft");
    case 7: return QStringLiteral("yd");
    default: return QString();
    }
}

QString NavigationDataBridge::laneShapeToken(int shape)
{
    switch (shape) {
    case 0: return QStringLiteral("unknown");
    case 1: return QStringLiteral("straight");
    case 2: return QStringLiteral("slight_left");
    case 3: return QStringLiteral("slight_right");
    case 4: return QStringLiteral("normal_left");
    case 5: return QStringLiteral("normal_right");
    case 6: return QStringLiteral("sharp_left");
    case 7: return QStringLiteral("sharp_right");
    case 8: return QStringLiteral("u_turn_left");
    case 9: return QStringLiteral("u_turn_right");
    default: return QStringLiteral("unknown_future");
    }
}

} // namespace aa
} // namespace oap
