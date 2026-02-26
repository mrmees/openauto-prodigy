#pragma once

#include <oaa/Channel/IChannelHandler.hpp>
#include <QDebug>
#include <QString>

namespace oaa {
namespace hu {

class StubChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    StubChannelHandler(uint8_t channelId, const QString& name, QObject* parent = nullptr)
        : oaa::IChannelHandler(parent), channelId_(channelId), name_(name) {}

    uint8_t channelId() const override { return channelId_; }

    void onChannelOpened() override {
        qDebug() << "[" << name_ << "] channel opened";
    }

    void onChannelClosed() override {
        qDebug() << "[" << name_ << "] channel closed";
    }

    void onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset = 0) override {
        const int dataSize = payload.size() - dataOffset;
        // Use qInfo so it's not filtered by debug level
        qInfo() << "[" << name_ << "] msgId:" << QString("0x%1").arg(messageId, 4, 16, QChar('0'))
                << "len:" << dataSize
                << "hex:" << QByteArray::fromRawData(payload.constData() + dataOffset,
                                                      qMin(dataSize, 128)).toHex(' ');
    }

private:
    uint8_t channelId_;
    QString name_;
};

} // namespace hu
} // namespace oaa
