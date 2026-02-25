#include "oaa/HU/Handlers/NavigationChannelHandler.hpp"

#include <QDebug>

#include "NavigationStateMessage.pb.h"
#include "NavigationStepMessage.pb.h"
#include "NavigationDistanceMessage.pb.h"

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

void NavigationChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload)
{
    switch (messageId) {
    case oaa::NavigationMessageId::NAV_STATE:
        handleNavState(payload);
        break;
    case oaa::NavigationMessageId::NAV_STEP:
        handleNavStep(payload);
        break;
    case oaa::NavigationMessageId::NAV_DISTANCE:
        handleNavDistance(payload);
        break;
    default:
        qInfo() << "[NavChannel] unknown msgId:" << QString("0x%1").arg(messageId, 4, 16, QChar('0'))
                << "len:" << payload.size()
                << "hex:" << payload.left(64).toHex(' ');
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
    oaa::proto::messages::NavigationDistance msg;
    if (!msg.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[NavChannel] failed to parse NavigationDistance";
        return;
    }

    QString distance;
    int unit = 0;

    if (msg.has_info() && msg.info().has_distance()) {
        // value() is now fixed32 — convert to string for display
        distance = QString::number(msg.info().distance().value());
        unit = msg.info().distance().unit();
    }

    qDebug() << "[NavChannel] distance:" << distance << "unit:" << unit;

    emit navigationDistanceChanged(distance, unit);
}

} // namespace hu
} // namespace oaa
