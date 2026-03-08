#include <oaa/Messenger/FrameSerializer.hpp>
#include <QtEndian>

namespace oaa {

QByteArray FrameSerializer::buildFrame(const FrameHeader& header,
                                       const QByteArray& payload,
                                       qint32 totalSize)
{
    int sizeLen = FrameHeader::sizeFieldLength(header.frameType);
    QByteArray frame;
    frame.reserve(2 + sizeLen + payload.size());

    // Header (2 bytes)
    frame.append(header.serialize());

    // Size field
    if (header.frameType == FrameType::First) {
        // FIRST: 2B frame payload size (BE) + 4B total message size (BE)
        uint16_t frameSizeBE = qToBigEndian(static_cast<uint16_t>(payload.size()));
        frame.append(reinterpret_cast<const char*>(&frameSizeBE), 2);
        uint32_t totalSizeBE = qToBigEndian(static_cast<uint32_t>(totalSize));
        frame.append(reinterpret_cast<const char*>(&totalSizeBE), 4);
    } else {
        // BULK/MIDDLE/LAST: 2B frame payload size (BE)
        uint16_t frameSizeBE = qToBigEndian(static_cast<uint16_t>(payload.size()));
        frame.append(reinterpret_cast<const char*>(&frameSizeBE), 2);
    }

    // Payload
    frame.append(payload);

    return frame;
}

QList<QByteArray> FrameSerializer::serialize(uint8_t channelId,
                                              MessageType msgType,
                                              EncryptionType encType,
                                              const QByteArray& payload)
{
    QList<QByteArray> frames;

    if (payload.size() <= FRAME_MAX_PAYLOAD) {
        // Single BULK frame
        FrameHeader header{channelId, FrameType::Bulk, encType, msgType};
        frames.append(buildFrame(header, payload));
        return frames;
    }

    // Multi-frame: FIRST + MIDDLE(s) + LAST
    int offset = 0;
    int totalSize = payload.size();

    // FIRST frame
    {
        FrameHeader header{channelId, FrameType::First, encType, msgType};
        QByteArray chunk = payload.mid(offset, FRAME_MAX_PAYLOAD);
        frames.append(buildFrame(header, chunk, totalSize));
        offset += FRAME_MAX_PAYLOAD;
    }

    // MIDDLE frames
    while (totalSize - offset > FRAME_MAX_PAYLOAD) {
        FrameHeader header{channelId, FrameType::Middle, encType, msgType};
        QByteArray chunk = payload.mid(offset, FRAME_MAX_PAYLOAD);
        frames.append(buildFrame(header, chunk));
        offset += FRAME_MAX_PAYLOAD;
    }

    // LAST frame
    {
        FrameHeader header{channelId, FrameType::Last, encType, msgType};
        QByteArray chunk = payload.mid(offset);
        frames.append(buildFrame(header, chunk));
    }

    return frames;
}

} // namespace oaa
