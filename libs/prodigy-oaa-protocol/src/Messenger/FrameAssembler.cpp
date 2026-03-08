#include <oaa/Messenger/FrameAssembler.hpp>
#include <QDebug>

namespace oaa {

FrameAssembler::FrameAssembler(QObject* parent)
    : QObject(parent)
{
}

void FrameAssembler::onFrame(const FrameHeader& header, const QByteArray& payload)
{
    switch (header.frameType) {
    case FrameType::Bulk:
        emit messageAssembled(header.channelId, header.messageType, payload);
        break;

    case FrameType::First:
        if (m_buffers.contains(header.channelId)) {
            qWarning() << "FrameAssembler: duplicate FIRST on channel"
                        << header.channelId << "— discarding partial";
        }
        m_buffers[header.channelId] = payload;
        m_messageTypes[header.channelId] = header.messageType;
        break;

    case FrameType::Middle:
        if (!m_buffers.contains(header.channelId)) {
            qWarning() << "FrameAssembler: MIDDLE without FIRST on channel"
                        << header.channelId << "— discarding";
            return;
        }
        m_buffers[header.channelId].append(payload);
        break;

    case FrameType::Last:
        if (!m_buffers.contains(header.channelId)) {
            qWarning() << "FrameAssembler: LAST without FIRST on channel"
                        << header.channelId << "— discarding";
            return;
        }
        m_buffers[header.channelId].append(payload);
        emit messageAssembled(header.channelId, m_messageTypes[header.channelId],
                              m_buffers.take(header.channelId));
        m_messageTypes.remove(header.channelId);
        break;
    }
}

} // namespace oaa
