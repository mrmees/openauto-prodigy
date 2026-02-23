#include <oaa/Messenger/FrameParser.hpp>
#include <QtEndian>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcFrameParser, "oaa.messenger.frameparser")

namespace oaa {

FrameParser::FrameParser(QObject* parent)
    : QObject(parent)
{
}

void FrameParser::onData(const QByteArray& data)
{
    m_buffer.append(data);
    process();
}

void FrameParser::process()
{
    while (true) {
        switch (m_state) {
        case State::ReadHeader:
            if (m_buffer.size() < 2)
                return;
            m_currentHeader = FrameHeader::parse(m_buffer.left(2));
            m_sizeFieldLength = FrameHeader::sizeFieldLength(m_currentHeader.frameType);
            m_buffer.remove(0, 2);
            m_state = State::ReadSize;
            break;

        case State::ReadSize:
            if (m_buffer.size() < m_sizeFieldLength)
                return;
            // Frame payload size is always the first 2 bytes (big-endian uint16)
            m_framePayloadSize = qFromBigEndian<uint16_t>(
                reinterpret_cast<const uchar*>(m_buffer.constData()));
            // For FIRST frames, bytes 2-5 are total message size — we don't need it here
            m_buffer.remove(0, m_sizeFieldLength);
            m_state = State::ReadPayload;
            break;

        case State::ReadPayload:
            if (m_buffer.size() < m_framePayloadSize)
                return;
            {
                QByteArray payload = m_buffer.left(m_framePayloadSize);
                m_buffer.remove(0, m_framePayloadSize);
                m_state = State::ReadHeader;
                emit frameParsed(m_currentHeader, payload);
            }
            break;
        }
    }
}

} // namespace oaa
