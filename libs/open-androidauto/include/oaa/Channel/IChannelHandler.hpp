#pragma once

#include <QObject>
#include <QByteArray>
#include <cstdint>

namespace oaa {

class IChannelHandler : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~IChannelHandler() override;

    virtual uint8_t channelId() const = 0;
    virtual void onChannelOpened() = 0;
    virtual void onChannelClosed() = 0;
    virtual void onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset = 0) = 0;

signals:
    void sendRequested(uint8_t channelId, uint16_t messageId,
                       const QByteArray& payload);
    void unknownMessage(uint16_t messageId, const QByteArray& payload);
};

} // namespace oaa
