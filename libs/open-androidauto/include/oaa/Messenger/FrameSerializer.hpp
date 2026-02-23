#pragma once

#include <oaa/Messenger/FrameType.hpp>
#include <oaa/Messenger/FrameHeader.hpp>
#include <QByteArray>
#include <QList>

namespace oaa {

class FrameSerializer {
public:
    static constexpr int FRAME_MAX_PAYLOAD = 16384;

    static QList<QByteArray> serialize(uint8_t channelId,
                                       MessageType msgType,
                                       EncryptionType encType,
                                       const QByteArray& payload);

private:
    static QByteArray buildFrame(const FrameHeader& header,
                                 const QByteArray& payload,
                                 qint32 totalSize = -1);
};

} // namespace oaa
