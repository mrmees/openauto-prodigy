#pragma once

#include <oaa/Channel/IChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>
#include <QSet>

namespace oaa {
namespace hu {

class SensorChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    explicit SensorChannelHandler(QObject* parent = nullptr);

    uint8_t channelId() const override { return oaa::ChannelId::Sensor; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset = 0) override;

    void pushNightMode(bool isNight);
    void pushDrivingStatus(int status);
    void pushParkingBrake(bool engaged);

private:
    void handleSensorStartRequest(const QByteArray& payload);

    bool channelOpen_ = false;
    QSet<int> activeSensors_;
};

} // namespace hu
} // namespace oaa
