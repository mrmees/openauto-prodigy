#include "SensorChannelHandler.hpp"

#include <QDebug>

#include "SensorStartRequestMessage.pb.h"
#include "SensorStartResponseMessage.pb.h"
#include "SensorEventIndicationMessage.pb.h"
#include "NightModeData.pb.h"
#include "DrivingStatusData.pb.h"
#include "StatusEnum.pb.h"
#include "SensorTypeEnum.pb.h"

namespace oap {
namespace aa {

SensorChannelHandler::SensorChannelHandler(QObject* parent)
    : oaa::IChannelHandler(parent)
{
}

void SensorChannelHandler::onChannelOpened()
{
    channelOpen_ = true;
    activeSensors_.clear();
    qDebug() << "[SensorChannel] opened";
}

void SensorChannelHandler::onChannelClosed()
{
    channelOpen_ = false;
    activeSensors_.clear();
    qDebug() << "[SensorChannel] closed";
}

void SensorChannelHandler::onMessage(uint16_t messageId, const QByteArray& payload)
{
    switch (messageId) {
    case oaa::SensorMessageId::SENSOR_START_REQUEST:
        handleSensorStartRequest(payload);
        break;
    default:
        qWarning() << "[SensorChannel] unknown message id:" << Qt::hex << messageId;
        emit unknownMessage(messageId, payload);
        break;
    }
}

void SensorChannelHandler::handleSensorStartRequest(const QByteArray& payload)
{
    oaa::proto::messages::SensorStartRequestMessage req;
    if (!req.ParseFromArray(payload.constData(), payload.size())) {
        qWarning() << "[SensorChannel] failed to parse SensorStartRequest";
        return;
    }

    auto sensorType = req.sensor_type();
    qDebug() << "[SensorChannel] start request for sensor type:" << sensorType;

    activeSensors_.insert(static_cast<int>(sensorType));

    // Send start response (OK)
    oaa::proto::messages::SensorStartResponseMessage resp;
    resp.set_status(oaa::proto::enums::Status::OK);

    QByteArray respData(resp.ByteSizeLong(), '\0');
    resp.SerializeToArray(respData.data(), respData.size());
    emit sendRequested(channelId(), oaa::SensorMessageId::SENSOR_START_RESPONSE, respData);

    // Send initial data for the requested sensor
    if (sensorType == oaa::proto::enums::SensorType::NIGHT_DATA) {
        pushNightMode(false); // Default: day mode
    } else if (sensorType == oaa::proto::enums::SensorType::DRIVING_STATUS) {
        pushDrivingStatus(0); // UNRESTRICTED
    }
}

void SensorChannelHandler::pushNightMode(bool isNight)
{
    if (!channelOpen_)
        return;

    oaa::proto::messages::SensorEventIndication indication;
    auto* nightMode = indication.add_night_mode();
    nightMode->set_is_night(isNight);

    QByteArray data(indication.ByteSizeLong(), '\0');
    indication.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::SensorMessageId::SENSOR_EVENT_INDICATION, data);
}

void SensorChannelHandler::pushDrivingStatus(int status)
{
    if (!channelOpen_)
        return;

    oaa::proto::messages::SensorEventIndication indication;
    auto* drivingStatus = indication.add_driving_status();
    drivingStatus->set_status(status);

    QByteArray data(indication.ByteSizeLong(), '\0');
    indication.SerializeToArray(data.data(), data.size());
    emit sendRequested(channelId(), oaa::SensorMessageId::SENSOR_EVENT_INDICATION, data);
}

void SensorChannelHandler::sendSensorEvent(uint16_t messageId, const QByteArray& serialized)
{
    emit sendRequested(channelId(), messageId, serialized);
}

} // namespace aa
} // namespace oap
