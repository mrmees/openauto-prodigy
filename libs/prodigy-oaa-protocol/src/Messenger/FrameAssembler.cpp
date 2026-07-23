#include <oaa/Messenger/FrameAssembler.hpp>

#include <QDebug>

namespace oaa {

FrameAssembler::FrameAssembler(QObject* parent)
    : FrameAssembler(MAX_ASSEMBLED_MESSAGE_SIZE,
                     MAX_IN_FLIGHT_ASSEMBLY_SIZE, parent)
{
}

FrameAssembler::FrameAssembler(uint32_t maxMessageSize,
                               uint64_t maxAggregateSize, QObject* parent)
    : QObject(parent)
    , m_maxMessageSize(maxMessageSize)
    , m_maxAggregateSize(maxAggregateSize)
{
}

void FrameAssembler::reset()
{
    m_partials.clear();
    m_reservedBytes = 0;
}

void FrameAssembler::release(uint8_t channelId)
{
    auto it = m_partials.find(channelId);
    if (it == m_partials.end())
        return;

    m_reservedBytes -= it->declaredSize;
    m_partials.erase(it);
}

void FrameAssembler::fail(const QString& message)
{
    qWarning() << "FrameAssembler:" << message;
    reset();
    emit assemblyFailed(message.left(1024));
}

bool FrameAssembler::flagsMatch(const PartialMessage& partial,
                                const FrameHeader& header) const
{
    return partial.messageType == header.messageType
        && partial.encryptionType == header.encryptionType;
}

void FrameAssembler::onFrame(const FrameHeader& header,
                             const QByteArray& payload)
{
    switch (header.frameType) {
    case FrameType::Bulk:
        if (m_partials.contains(header.channelId)) {
            fail(QStringLiteral("BULK interrupted fragmented message on channel %1")
                     .arg(header.channelId));
            return;
        }
        emit messageAssembled(header.channelId, header.messageType, payload);
        return;

    case FrameType::First: {
        if (m_partials.contains(header.channelId)) {
            qWarning() << "FrameAssembler: duplicate FIRST on channel"
                       << header.channelId << "— replacing partial";
            release(header.channelId);
        }

        const uint32_t payloadSize = static_cast<uint32_t>(payload.size());
        if (header.totalMessageSize == 0) {
            fail(QStringLiteral("FIRST declared zero total on channel %1")
                     .arg(header.channelId));
            return;
        }
        if (header.totalMessageSize <= payloadSize) {
            fail(QStringLiteral("FIRST total is not larger than its payload on channel %1")
                     .arg(header.channelId));
            return;
        }
        if (header.totalMessageSize > m_maxMessageSize) {
            fail(QStringLiteral("FIRST total exceeds message limit on channel %1")
                     .arg(header.channelId));
            return;
        }
        if (header.totalMessageSize > m_maxAggregateSize - m_reservedBytes) {
            fail(QStringLiteral("FIRST total exceeds aggregate assembly budget on channel %1")
                     .arg(header.channelId));
            return;
        }

        PartialMessage partial;
        partial.payload = payload;
        partial.declaredSize = header.totalMessageSize;
        partial.messageType = header.messageType;
        partial.encryptionType = header.encryptionType;
        m_reservedBytes += partial.declaredSize;
        m_partials.insert(header.channelId, std::move(partial));
        return;
    }

    case FrameType::Middle:
    case FrameType::Last: {
        auto it = m_partials.find(header.channelId);
        if (it == m_partials.end()) {
            fail(QStringLiteral("continuation without FIRST on channel %1")
                     .arg(header.channelId));
            return;
        }
        if (!flagsMatch(it.value(), header)) {
            fail(QStringLiteral("continuation flags differ from FIRST on channel %1")
                     .arg(header.channelId));
            return;
        }

        const uint64_t nextSize = static_cast<uint64_t>(it->payload.size())
            + static_cast<uint64_t>(payload.size());
        if (header.frameType == FrameType::Middle) {
            if (nextSize >= it->declaredSize) {
                fail(QStringLiteral("MIDDLE reaches or exceeds declared total on channel %1")
                         .arg(header.channelId));
                return;
            }
            it->payload.append(payload);
            return;
        }

        if (nextSize != it->declaredSize) {
            fail(QStringLiteral("LAST does not finish at declared total on channel %1")
                     .arg(header.channelId));
            return;
        }

        it->payload.append(payload);
        const MessageType messageType = it->messageType;
        QByteArray message = std::move(it->payload);
        release(header.channelId);
        emit messageAssembled(header.channelId, messageType, message);
        return;
    }
    }
}

} // namespace oaa
