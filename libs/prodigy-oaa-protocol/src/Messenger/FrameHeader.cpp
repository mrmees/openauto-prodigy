#include <oaa/Messenger/FrameHeader.hpp>

namespace oaa {

FrameHeader FrameHeader::parse(const QByteArray& data)
{
    uint8_t byte0 = static_cast<uint8_t>(data[0]);
    uint8_t byte1 = static_cast<uint8_t>(data[1]);

    return FrameHeader{
        byte0,
        static_cast<FrameType>(byte1 & 0x03),
        static_cast<EncryptionType>(byte1 & 0x08),
        static_cast<MessageType>(byte1 & 0x04)
    };
}

QByteArray FrameHeader::serialize() const
{
    QByteArray result(2, '\0');
    result[0] = static_cast<char>(channelId);
    result[1] = static_cast<char>(
        static_cast<uint8_t>(frameType) |
        static_cast<uint8_t>(encryptionType) |
        static_cast<uint8_t>(messageType)
    );
    return result;
}

int FrameHeader::sizeFieldLength(FrameType type)
{
    return (type == FrameType::First) ? 6 : 2;
}

} // namespace oaa
