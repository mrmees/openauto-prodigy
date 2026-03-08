#pragma once

#include <oaa/Messenger/FrameType.hpp>
#include <QByteArray>

namespace oaa {

struct FrameHeader {
    uint8_t channelId;
    FrameType frameType;
    EncryptionType encryptionType;
    MessageType messageType;

    static FrameHeader parse(const QByteArray& data);
    QByteArray serialize() const;
    static int sizeFieldLength(FrameType type);
};

} // namespace oaa
