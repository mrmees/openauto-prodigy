#pragma once

#include <oaa/Messenger/FrameType.hpp>
#include <QByteArray>

namespace oaa {

struct FrameHeader {
    uint8_t channelId;
    FrameType frameType;
    EncryptionType encryptionType;
    MessageType messageType;
    // Parsed from FIRST's extended size field. It is metadata for assembly
    // validation and is not part of the two-byte serialized header.
    uint32_t totalMessageSize = 0;

    static FrameHeader parse(const QByteArray& data);
    QByteArray serialize() const;
    static int sizeFieldLength(FrameType type);
};

} // namespace oaa
