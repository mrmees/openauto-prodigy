#include "NavigationDataBridge.hpp"

#include "ManeuverIconProvider.hpp"

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
    connect(handler, &oaa::hu::NavigationChannelHandler::navigationStateSnapshotChanged,
            this, &NavigationDataBridge::onNavigationStateChanged);
    connect(handler, &oaa::hu::NavigationChannelHandler::navigationNotificationChanged,
            this, &NavigationDataBridge::onNavigationNotificationChanged);
    connect(handler, &oaa::hu::NavigationChannelHandler::navigationPositionChanged,
            this, &NavigationDataBridge::onNavigationPositionChanged);
    connect(handler, &oaa::hu::NavigationChannelHandler::navigationTurnEvent,
            this, &NavigationDataBridge::onNavigationTurnEvent);
}

void NavigationDataBridge::setManeuverIconProvider(ManeuverIconProvider* provider)
{
    iconProvider_ = provider;
}

bool NavigationDataBridge::navActive() const
{
    return navigationState_ == oaa::hu::NavigationState::Active
        || navigationState_ == oaa::hu::NavigationState::Rerouting;
}

bool NavigationDataBridge::guidanceVisible() const
{
    return navActive() && notificationFresh_ && !awaitingPostReroute_
        && navigationState_ != oaa::hu::NavigationState::Rerouting;
}

bool NavigationDataBridge::tripVisible() const
{
    return guidanceVisible() && positionFresh_;
}

bool NavigationDataBridge::guidanceFresh() const
{
    return guidanceVisible();
}

bool NavigationDataBridge::rerouting() const
{
    return navigationState_ == oaa::hu::NavigationState::Rerouting
        || awaitingPostReroute_;
}

QString NavigationDataBridge::roadName() const
{
    return guidanceVisible() ? roadName_ : QString();
}

QString NavigationDataBridge::destination() const
{
    return tripVisible() ? destinations_.value(0) : QString();
}

int NavigationDataBridge::maneuverType() const
{
    return guidanceVisible() ? maneuverType_ : 0;
}

int NavigationDataBridge::turnDirection() const
{
    return guidanceVisible() ? turnDirection_ : 0;
}

int NavigationDataBridge::distanceMeters() const
{
    if (!tripVisible())
        return 0;
    return legacyDistance_ ? legacyDistanceMeters_ : 0;
}

bool NavigationDataBridge::hasManeuverIcon() const
{
    return guidanceVisible() && !currentIcon_.isEmpty();
}

bool NavigationDataBridge::hasLaneGuidance() const
{
    return guidanceVisible() && laneModel_->rowCount() > 0;
}

bool NavigationDataBridge::hasDistance() const
{
    return tripVisible()
        && (legacyDistance_ || (hasStepDistance_ && stepDistance_.hasDisplayText
            && !stepDistance_.displayText.isEmpty()));
}

bool NavigationDataBridge::hasActionCue() const
{
    return guidanceVisible() && !actionCue_.isEmpty();
}

QString NavigationDataBridge::actionCue() const
{
    return hasActionCue() ? actionCue_ : QString();
}

bool NavigationDataBridge::hasTimeToStep() const
{
    return tripVisible() && hasTimeToStep_ && timeToStepSeconds_ > 0;
}

QString NavigationDataBridge::formattedTimeToStep() const
{
    return hasTimeToStep() ? formatDuration(timeToStepSeconds_) : QString();
}

int NavigationDataBridge::destinationCount() const
{
    return tripVisible() ? destinations_.size() : 0;
}

const oaa::hu::NavigationDestinationDistanceData*
NavigationDataBridge::firstDestinationDistance() const
{
    return destinationDistances_.isEmpty() ? nullptr : &destinationDistances_.first();
}

bool NavigationDataBridge::hasDestinationDistance() const
{
    const auto* entry = firstDestinationDistance();
    return tripVisible() && !destinations_.isEmpty() && entry && entry->hasDistance
        && entry->distance.hasDisplayText && !entry->distance.displayText.isEmpty();
}

QString NavigationDataBridge::formattedDestinationDistance() const
{
    const auto* entry = firstDestinationDistance();
    return hasDestinationDistance() && entry ? formatDistance(entry->distance) : QString();
}

bool NavigationDataBridge::hasDestinationEta() const
{
    const auto* entry = firstDestinationDistance();
    return tripVisible() && !destinations_.isEmpty() && entry
        && entry->hasEstimatedTimeOfArrival && !entry->estimatedTimeOfArrival.isEmpty();
}

QString NavigationDataBridge::destinationEta() const
{
    const auto* entry = firstDestinationDistance();
    return hasDestinationEta() && entry ? entry->estimatedTimeOfArrival : QString();
}

bool NavigationDataBridge::hasTimeToArrival() const
{
    const auto* entry = firstDestinationDistance();
    return tripVisible() && destinations_.size() == 1 && entry
        && entry->hasTimeToArrival && entry->timeToArrivalSeconds > 0;
}

QString NavigationDataBridge::formattedTimeToArrival() const
{
    const auto* entry = firstDestinationDistance();
    return hasTimeToArrival() && entry ? formatDuration(entry->timeToArrivalSeconds)
                                        : QString();
}

void NavigationDataBridge::clearIcon()
{
    currentIcon_.clear();
    if (iconProvider_)
        iconProvider_->updateIcon({});
}

bool NavigationDataBridge::clearSnapshotData()
{
    roadName_.clear();
    actionCue_.clear();
    destinations_.clear();
    maneuverType_ = 0;
    turnDirection_ = 0;
    hasStepDistance_ = false;
    stepDistance_ = {};
    hasTimeToStep_ = false;
    timeToStepSeconds_ = 0;
    destinationDistances_.clear();
    legacyDistance_ = false;
    legacyDistanceMeters_ = 0;
    legacyDistanceUnit_ = 0;
    notificationFresh_ = false;
    positionFresh_ = false;
    clearIcon();
    return laneModel_->clear();
}

void NavigationDataBridge::onNavigationStateChanged(oaa::hu::NavigationState state)
{
    if (receivedState_ && navigationState_ == state)
        return;

    const bool wasActive = navActive();
    navigationState_ = state;
    receivedState_ = true;

    bool lanesChanged = false;
    if (state == oaa::hu::NavigationState::Rerouting) {
        awaitingPostReroute_ = true;
        lanesChanged = clearSnapshotData();
    } else if (state == oaa::hu::NavigationState::Inactive
               || state == oaa::hu::NavigationState::Unavailable) {
        awaitingPostReroute_ = false;
        lanesChanged = clearSnapshotData();
    }

    if (wasActive != navActive())
        emit navActiveChanged();
    emit turnDataChanged();
    emit distanceChanged();
    if (lanesChanged)
        emit laneGuidanceChanged();
    emit tripDataChanged();
    emit navigationPresentationChanged();
}

bool NavigationDataBridge::replaceLanes(const oaa::hu::NavigationLaneGuidance& lanes)
{
    LanePresentationList presentation;
    presentation.reserve(lanes.size());
    for (const auto& lane : lanes) {
        LanePresentation directions;
        directions.reserve(lane.directions.size());
        for (const auto& direction : lane.directions) {
            directions.append({laneShapeToken(direction.shape), direction.recommended});
        }
        presentation.append(directions);
    }
    return laneModel_->replaceLanes(presentation);
}

void NavigationDataBridge::onNavigationNotificationChanged(
    const oaa::hu::NavigationNotificationSnapshot& snapshot)
{
    roadName_ = snapshot.hasUpcomingRoad ? snapshot.upcomingRoad : QString();
    maneuverType_ = snapshot.hasManeuver ? snapshot.maneuverType : 0;
    turnDirection_ = 0;
    actionCue_.clear();
    const QString trimmedRoad = roadName_.trimmed();
    for (const QString& cue : snapshot.actionCues) {
        const QString trimmedCue = cue.trimmed();
        if (!trimmedCue.isEmpty() && trimmedCue != trimmedRoad) {
            actionCue_ = trimmedCue;
            break;
        }
    }
    destinations_ = snapshot.destinations;
    const bool lanesChanged = replaceLanes(snapshot.lanes);
    notificationFresh_ = true;
    awaitingPostReroute_ = false;

    emit turnDataChanged();
    if (lanesChanged)
        emit laneGuidanceChanged();
    emit tripDataChanged();
    emit navigationPresentationChanged();
}

void NavigationDataBridge::onNavigationPositionChanged(
    const oaa::hu::NavigationPositionSnapshot& snapshot)
{
    hasStepDistance_ = snapshot.hasStepDistance;
    stepDistance_ = snapshot.hasStepDistance ? snapshot.stepDistance
                                             : oaa::hu::NavigationDistanceData{};
    hasTimeToStep_ = snapshot.hasTimeToStep;
    timeToStepSeconds_ = snapshot.hasTimeToStep ? snapshot.timeToStepSeconds : 0;
    destinationDistances_ = snapshot.destinationDistances;
    legacyDistance_ = false;
    positionFresh_ = true;

    emit distanceChanged();
    emit tripDataChanged();
    emit navigationPresentationChanged();
}

void NavigationDataBridge::onNavigationTurnEvent(const QString& roadName, int maneuverType,
                                                  int turnDirection, const QByteArray& turnIcon,
                                                  int distanceMeters, int distanceUnit)
{
    roadName_ = roadName;
    maneuverType_ = maneuverType;
    turnDirection_ = turnDirection;
    actionCue_.clear();
    destinations_.clear();
    hasStepDistance_ = false;
    stepDistance_ = {};
    hasTimeToStep_ = false;
    timeToStepSeconds_ = 0;
    destinationDistances_.clear();
    legacyDistance_ = distanceMeters >= 0;
    legacyDistanceMeters_ = distanceMeters;
    legacyDistanceUnit_ = distanceUnit;
    notificationFresh_ = true;
    positionFresh_ = true;
    awaitingPostReroute_ = false;
    const bool lanesChanged = replaceLanes({});

    if (!turnIcon.isEmpty()) {
        currentIcon_ = turnIcon;
        ++iconVersion_;
        if (iconProvider_)
            iconProvider_->updateIcon(turnIcon);
    }

    emit turnDataChanged();
    emit distanceChanged();
    if (lanesChanged)
        emit laneGuidanceChanged();
    emit tripDataChanged();
    emit navigationPresentationChanged();
}

QString NavigationDataBridge::formattedDistance() const
{
    if (!hasDistance())
        return QStringLiteral("0 m");
    return legacyDistance_ ? formatLegacyDistance(legacyDistanceMeters_, legacyDistanceUnit_)
                           : formatDistance(stepDistance_);
}

QString NavigationDataBridge::formatDistance(const oaa::hu::NavigationDistanceData& distance)
{
    if (distance.hasDisplayText && !distance.displayText.isEmpty()) {
        QString text = distance.displayText;
        if (distance.unit == 4 || distance.unit == 5) {
            bool parsed = false;
            const double miles = text.toDouble(&parsed);
            if (parsed && miles > 9.9)
                text = formatMiles(miles);
        }
        const QString suffix = unitSuffix(distance.unit);
        return suffix.isEmpty() ? text : text + QStringLiteral(" ") + suffix;
    }
    return {};
}

QString NavigationDataBridge::formatLegacyDistance(int distanceMeters, int distanceUnit)
{
    switch (distanceUnit) {
    case 1:
        return distanceMeters >= 1000
            ? QString::number(distanceMeters / 1000.0, 'f', 1) + QStringLiteral(" km")
            : QString::number(distanceMeters) + QStringLiteral(" m");
    case 2:
    case 3:
        return QString::number(distanceMeters / 1000.0, 'f', 1) + QStringLiteral(" km");
    case 4:
    case 5:
        return formatMiles(distanceMeters / 1609.34) + QStringLiteral(" mi");
    case 6:
        return QString::number(qRound(distanceMeters * 3.28084)) + QStringLiteral(" ft");
    case 7:
        return QString::number(qRound(distanceMeters / 0.9144)) + QStringLiteral(" yd");
    default:
        return QString::number(distanceMeters) + QStringLiteral(" m");
    }
}

QString NavigationDataBridge::formatDuration(qint64 seconds)
{
    if (seconds <= 0)
        return {};
    if (seconds < 60)
        return QStringLiteral("<1 min");
    const qint64 minutes = seconds / 60;
    if (minutes < 60)
        return QStringLiteral("%1 min").arg(minutes);
    const qint64 hours = minutes / 60;
    const qint64 remainder = minutes % 60;
    return remainder == 0 ? QStringLiteral("%1 h").arg(hours)
                          : QStringLiteral("%1 h %2 min").arg(hours).arg(remainder);
}

QString NavigationDataBridge::formatMiles(double miles)
{
    return miles > 9.9 ? QString::number(qRound(miles))
                       : QString::number(miles, 'f', 1);
}

QString NavigationDataBridge::unitSuffix(int distanceUnit)
{
    switch (distanceUnit) {
    case 1: return QStringLiteral("m");
    case 2:
    case 3: return QStringLiteral("km");
    case 4:
    case 5: return QStringLiteral("mi");
    case 6: return QStringLiteral("ft");
    case 7: return QStringLiteral("yd");
    default: return {};
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
