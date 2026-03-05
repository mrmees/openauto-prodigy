#include "oaa/HU/Handlers/NavigationChannelHandler.hpp"

#include <QDebug>

#include "oaa/navigation/NavigationStateMessage.pb.h"
#include "oaa/navigation/NavigationNotificationMessage.pb.h"
#include "oaa/navigation/NavigationTurnEventMessage.pb.h"

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

void NavigationChannelHandler::handleNavStep(const QByteArray& payload)
{
    oaa::proto::messages::NavigationNotification msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[NavChannel] failed to parse NavigationNotification";
        return;
    }

    QString instruction;
    int maneuverType = 0;
    QString destination;

    if (msg.steps_size() > 0) {
        const auto& step = msg.steps(0);
        if (step.has_instruction())
            instruction = QString::fromStdString(step.instruction().text());
        if (step.has_maneuver())
            maneuverType = step.maneuver().type();
    }
    if (msg.destinations_size() > 0)
        destination = QString::fromStdString(msg.destinations(0).address());

    qInfo() << "[NavChannel] step:" << instruction << "→" << destination
            << "maneuver:" << maneuverType;

    emit navigationStepChanged(instruction, destination, maneuverType);
}

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
