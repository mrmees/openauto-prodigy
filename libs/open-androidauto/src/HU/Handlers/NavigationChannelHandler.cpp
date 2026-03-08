#include "oaa/HU/Handlers/NavigationChannelHandler.hpp"

#include <QDebug>

#include "oaa/navigation/NavigationStateMessage.pb.h"
#include "oaa/navigation/NavigationNotificationMessage.pb.h"
#include "oaa/navigation/NavigationTurnEventMessage.pb.h"
#include "oaa/navigation/NavigationFocusIndicationMessage.pb.h"

namespace oaa {
namespace hu {

NavigationChannelHandler::NavigationChannelHandler(QObject* parent)
    : oaa::IChannelHandler(parent)
{
}

void NavigationChannelHandler::onChannelOpened()
{
    navActive_ = false;
    qInfo() << "[NavChannel] opened";
}

void NavigationChannelHandler::onChannelClosed()
{
    if (navActive_) {
        navActive_ = false;
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
    case 0x8005: // was NAV_FOCUS_INDICATION (retracted — message doesn't exist)
        qInfo() << "[NavChannel] received 0x8005 (retracted NAV_FOCUS_INDICATION), len:" << data.size();
        break;
    case oaa::NavigationMessageId::NAV_STEP:
        handleNavStep(data);
        break;
    case oaa::NavigationMessageId::NAV_DISTANCE:
        handleNavDistance(data);
        break;
    default:
        qInfo() << "[NavChannel] unknown msgId:" << QString("0x%1").arg(messageId, 4, 16, QChar('0'))
                << "len:" << data.size()
                << "hex:" << data.left(64).toHex(' ');
        break;
    }
}

void NavigationChannelHandler::handleNavState(const QByteArray& payload)
{
    oaa::proto::messages::NavigationState msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[NavChannel] failed to parse NavigationState";
        return;
    }

    bool active = (msg.state() == 1);
    qInfo() << "[NavChannel] state:" << msg.state() << (active ? "(active)" : "(ended)");

    if (navActive_ != active) {
        navActive_ = active;
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

    int stepCount = msg.steps_size();
    int totalLanes = 0;
    QString instruction;
    int maneuverType = 0;
    QString destination;

    // Extract data from all steps (multi-step lookahead)
    for (int i = 0; i < msg.steps_size(); ++i) {
        const auto& step = msg.steps(i);

        // Use first step for primary instruction/maneuver (backward compat)
        if (i == 0) {
            if (step.has_instruction())
                instruction = QString::fromStdString(step.instruction().text());
            if (step.has_maneuver())
                maneuverType = step.maneuver().type();
        }

        // Count lanes across all steps
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

    // Extract destination
    if (msg.destinations_size() > 0)
        destination = QString::fromStdString(msg.destinations(0).address());

    qInfo() << "[NavChannel] notification:" << stepCount << "steps,"
            << totalLanes << "lanes, dest:" << destination
            << "instruction:" << instruction << "maneuver:" << maneuverType;

    // Emit original signal for backward compatibility
    emit navigationStepChanged(instruction, destination, maneuverType);

    // Emit enhanced notification signal
    emit navigationNotificationReceived(stepCount, totalLanes, destination, QString());
}

// handleFocusIndication removed — NavigationFocusIndication proto was retracted (2026-03-06).
// Nav focus is managed via Control channel (NavigationFocusRequest/Response, msgs 13/14).

void NavigationChannelHandler::handleNavDistance(const QByteArray& payload)
{
    oaa::proto::messages::NavigationNextTurnDistanceEvent msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[NavChannel] failed to parse NavigationNextTurnDistanceEvent";
        return;
    }

    QString distance;
    int unit = 0;

    if (msg.has_remaining_distance() && msg.remaining_distance().has_distance()) {
        const auto& d = msg.remaining_distance().distance();
        if (d.has_display_text())
            distance = QString::fromStdString(d.display_text());
        unit = d.distance_unit();
    }

    qDebug() << "[NavChannel] distance:" << distance << "unit:" << unit;

    emit navigationDistanceChanged(distance, unit);
}

} // namespace hu
} // namespace oaa
