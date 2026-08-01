#include "oaa/HU/Handlers/NavigationChannelHandler.hpp"

#include <QDebug>

#include "oaa/navigation/NavigationStateMessage.pb.h"
#include "oaa/navigation/NavigationNotificationMessage.pb.h"
#include "oaa/navigation/NavigationTurnEventMessage.pb.h"
#include "oaa/navigation/VehicleEnergyForecastMessage.pb.h"
namespace oaa {
namespace hu {

namespace {

bool isNavigationActive(NavigationState state)
{
    return state == NavigationState::Active || state == NavigationState::Rerouting;
}

NavigationDistanceData copyDistance(
    const oaa::proto::messages::NavigationTurnDistance& distance)
{
    NavigationDistanceData result;
    result.hasValue = distance.has_distance_value();
    if (result.hasValue)
        result.value = distance.distance_value();
    result.hasDisplayText = distance.has_display_text();
    if (result.hasDisplayText)
        result.displayText = QString::fromStdString(distance.display_text());
    result.hasUnit = distance.has_distance_unit();
    if (result.hasUnit)
        result.unit = static_cast<int>(distance.distance_unit());
    return result;
}

} // namespace

NavigationChannelHandler::NavigationChannelHandler(QObject* parent)
    : oaa::IChannelHandler(parent)
{
    qRegisterMetaType<oaa::hu::NavigationLaneGuidance>();
    qRegisterMetaType<oaa::hu::NavigationState>();
    qRegisterMetaType<oaa::hu::NavigationNotificationSnapshot>();
    qRegisterMetaType<oaa::hu::NavigationPositionSnapshot>();
}

void NavigationChannelHandler::onChannelOpened()
{
    navigationState_ = NavigationState::Unavailable;
    qInfo() << "[NavChannel] opened";
}

void NavigationChannelHandler::onChannelClosed()
{
    if (navigationState_ != NavigationState::Unavailable) {
        const bool wasActive = isNavigationActive(navigationState_);
        navigationState_ = NavigationState::Unavailable;
        emit navigationStateSnapshotChanged(navigationState_);
        if (wasActive)
            emit navigationStateChanged(false);
    }
    qInfo() << "[NavChannel] closed";
}

void NavigationChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset)
{
    // Zero-copy view of data past the message ID header
    const QByteArray data = QByteArray::fromRawData(
        payload.constData() + dataOffset, payload.size() - dataOffset);

    switch (messageId) {
    case oaa::NavigationMessageId::NAV_STATE:
        handleNavState(data);
        break;
    case oaa::NavigationMessageId::NAV_TURN_EVENT:
        handleTurnEvent(data);
        break;
    case oaa::NavigationMessageId::NAV_STEP:
        handleNavStep(data);
        break;
    case oaa::NavigationMessageId::NAV_DISTANCE:
        handleNavDistance(data);
        break;
    case oaa::NavigationMessageId::VEHICLE_ENERGY_FORECAST:
        handleVehicleEnergyForecast(data);
        break;
    default:
        qInfo() << "[NavChannel] unknown msgId:" << QString("0x%1").arg(messageId, 4, 16, QChar('0'))
                << "len:" << data.size()
                << "hex:" << data.left(64).toHex(' ');
        break;
    }
}

void NavigationChannelHandler::handleVehicleEnergyForecast(
    const QByteArray& payload)
{
    oaa::proto::messages::VehicleEnergyForecastMessage outer;
    if (!outer.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[NavChannel] failed to parse VehicleEnergyForecast outer, size:"
                   << payload.size();
        return;
    }

    bool innerParsed = false;
    QString summary = QString::fromStdString(
        outer.ShortDebugString()).left(512);
    if (outer.has_vehicle_energy_forecast()) {
        oaa::proto::messages::VehicleEnergyForecast inner;
        innerParsed = inner.ParseFromString(outer.vehicle_energy_forecast());
        if (innerParsed) {
            summary = QString::fromStdString(
                inner.ShortDebugString()).left(512);
        } else {
            qWarning() << "[NavChannel] failed to parse VehicleEnergyForecast inner, size:"
                       << outer.vehicle_energy_forecast().size();
        }
    }

    qDebug() << "[NavChannel] vehicle energy forecast, size:"
             << payload.size() << "inner parsed:" << innerParsed
             << "summary:" << summary;
    emit vehicleEnergyForecastReceived(innerParsed, summary);
}

void NavigationChannelHandler::handleNavState(const QByteArray& payload)
{
    oaa::proto::messages::NavigationState msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[NavChannel] failed to parse NavigationState";
        return;
    }

    NavigationState nextState = NavigationState::Unavailable;
    switch (msg.state()) {
    case oaa::proto::messages::NAV_STATE_ACTIVE:
        nextState = NavigationState::Active;
        break;
    case oaa::proto::messages::NAV_STATE_INACTIVE:
        nextState = NavigationState::Inactive;
        break;
    case oaa::proto::messages::NAV_STATE_REROUTING:
        nextState = NavigationState::Rerouting;
        break;
    case oaa::proto::messages::NAV_STATE_UNAVAILABLE:
    default:
        break;
    }

    qInfo() << "[NavChannel] state:" << msg.state();
    if (navigationState_ != nextState) {
        const bool wasActive = isNavigationActive(navigationState_);
        const bool active = isNavigationActive(nextState);
        navigationState_ = nextState;
        emit navigationStateSnapshotChanged(navigationState_);
        if (wasActive != active)
            emit navigationStateChanged(active);
    }
}

void NavigationChannelHandler::handleTurnEvent(const QByteArray& payload)
{
    oaa::proto::messages::NavigationTurnEvent msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[NavChannel] failed to parse NavigationTurnEvent";
        return;
    }

    QString roadName = msg.has_road_name()
        ? QString::fromStdString(msg.road_name()) : QString();
    int maneuver = msg.has_maneuver_type() ? static_cast<int>(msg.maneuver_type()) : 0;
    int direction = msg.has_turn_direction() ? static_cast<int>(msg.turn_direction()) : 0;
    QByteArray icon = msg.has_turn_icon()
        ? QByteArray(msg.turn_icon().data(), msg.turn_icon().size())
        : QByteArray();
    int distance = msg.has_distance_meters() ? msg.distance_meters() : -1;
    int unit = msg.has_distance_unit() ? msg.distance_unit() : 0;

    qInfo() << "[NavChannel] turn:" << roadName
            << "maneuver:" << maneuver << "dir:" << direction
            << "dist:" << distance << "unit:" << unit
            << "icon:" << icon.size() << "bytes";

    emit navigationTurnEvent(roadName, maneuver, direction, icon, distance, unit);
}

void NavigationChannelHandler::handleNavStep(const QByteArray& payload)
{
    oaa::proto::messages::NavigationNotification msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[NavChannel] failed to parse NavigationNotification";
        return;
    }

    NavigationNotificationSnapshot snapshot;
    snapshot.stepCount = msg.steps_size();
    int totalLanes = 0;

    if (msg.steps_size() > 0) {
        const auto& currentStep = msg.steps(0);
        snapshot.hasManeuver = currentStep.has_maneuver();
        if (snapshot.hasManeuver)
            snapshot.maneuverType = currentStep.maneuver().type();
        snapshot.hasUpcomingRoad = currentStep.has_instruction();
        if (snapshot.hasUpcomingRoad) {
            snapshot.upcomingRoad = QString::fromStdString(
                currentStep.instruction().text());
        }
        if (currentStep.has_road_info()) {
            const auto& roadInfo = currentStep.road_info();
            for (int r = 0; r < roadInfo.road_names_size(); ++r) {
                snapshot.actionCues.append(QString::fromStdString(
                    roadInfo.road_names(r)));
            }
        }
        for (int l = 0; l < currentStep.lanes_size(); ++l) {
            NavigationLaneData laneData;
            const auto& lane = currentStep.lanes(l);
            for (int d = 0; d < lane.directions_size(); ++d) {
                const auto& direction = lane.directions(d);
                laneData.directions.append({
                    static_cast<int>(direction.shape()),
                    direction.is_recommended(),
                });
            }
            snapshot.lanes.append(laneData);
        }
    }

    // Extract data from all steps (multi-step lookahead)
    for (int i = 0; i < msg.steps_size(); ++i) {
        const auto& step = msg.steps(i);

        totalLanes += step.lanes_size();

        // Log lane guidance details
        for (int l = 0; l < step.lanes_size(); ++l) {
            const auto& lane = step.lanes(l);
            for (int d = 0; d < lane.directions_size(); ++d) {
                const auto& dir = lane.directions(d);
                qDebug() << "[NavChannel] step" << i << "lane" << l
                         << "shape:" << static_cast<int>(dir.shape())
                         << "recommended:" << dir.is_recommended();
            }
        }

        // Log road info
        if (step.has_road_info()) {
            for (int r = 0; r < step.road_info().road_names_size(); ++r) {
                qDebug() << "[NavChannel] step" << i
                         << "road:" << QString::fromStdString(step.road_info().road_names(r));
            }
        }
    }

    for (int i = 0; i < msg.destinations_size(); ++i) {
        snapshot.destinations.append(QString::fromStdString(
            msg.destinations(i).address()));
    }

    qInfo() << "[NavChannel] notification:" << snapshot.stepCount << "steps,"
            << totalLanes << "lanes, dest:" << snapshot.destinations.value(0)
            << "instruction:" << snapshot.upcomingRoad
            << "maneuver:" << snapshot.maneuverType;

    emit navigationNotificationChanged(snapshot);

    emit navigationStepChanged(snapshot.upcomingRoad, snapshot.destinations.value(0),
                               snapshot.maneuverType);

    emit navigationLaneGuidanceChanged(snapshot.lanes);

    emit navigationNotificationReceived(snapshot.stepCount, totalLanes,
                                        snapshot.destinations.value(0), QString());
}

void NavigationChannelHandler::handleNavDistance(const QByteArray& payload)
{
    oaa::proto::messages::NavigationNextTurnDistanceEvent msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[NavChannel] failed to parse NavigationNextTurnDistanceEvent";
        return;
    }

    NavigationPositionSnapshot snapshot;
    snapshot.hasStepDistance = msg.has_step_distance();
    if (snapshot.hasStepDistance) {
        const auto& stepDistance = msg.step_distance();
        if (stepDistance.has_distance())
            snapshot.stepDistance = copyDistance(stepDistance.distance());
        snapshot.hasTimeToStep = stepDistance.has_time_to_step_seconds();
        if (snapshot.hasTimeToStep)
            snapshot.timeToStepSeconds = stepDistance.time_to_step_seconds();
    }

    for (int i = 0; i < msg.destination_distances_size(); ++i) {
        const auto& source = msg.destination_distances(i);
        NavigationDestinationDistanceData destination;
        destination.hasDistance = source.has_distance();
        if (destination.hasDistance)
            destination.distance = copyDistance(source.distance());
        destination.hasEstimatedTimeOfArrival = source.has_estimated_time_of_arrival();
        if (destination.hasEstimatedTimeOfArrival) {
            destination.estimatedTimeOfArrival = QString::fromStdString(
                source.estimated_time_of_arrival());
        }
        destination.hasTimeToArrival = source.has_time_to_arrival_seconds();
        if (destination.hasTimeToArrival)
            destination.timeToArrivalSeconds = source.time_to_arrival_seconds();
        snapshot.destinationDistances.append(destination);
    }

    snapshot.hasCurrentRoad = msg.has_current_road();
    if (snapshot.hasCurrentRoad)
        snapshot.currentRoad = QString::fromStdString(msg.current_road().text());

    qDebug() << "[NavChannel] distance:" << snapshot.stepDistance.displayText
             << "unit:" << snapshot.stepDistance.unit;

    emit navigationPositionChanged(snapshot);

    emit navigationDistanceChanged(snapshot.stepDistance.displayText,
                                   snapshot.stepDistance.unit);
}

} // namespace hu
} // namespace oaa
