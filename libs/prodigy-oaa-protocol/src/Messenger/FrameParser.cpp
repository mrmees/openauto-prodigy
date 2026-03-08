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
    m_buffer.append(data.constData(), data.size());
    process();
}

void FrameParser::process()
{
    while (true) {
        switch (m_state) {
        case State::ReadHeader:
            if (m_buffer.available() < 2)
                return;
            {
                QByteArray hdr = m_buffer.peek(2);
                m_currentHeader = FrameHeader::parse(hdr);
                m_sizeFieldLength = FrameHeader::sizeFieldLength(m_currentHeader.frameType);
                m_buffer.consume(2);
            }
            m_state = State::ReadSize;
            break;

        case State::ReadSize:
            if (m_buffer.available() < static_cast<size_t>(m_sizeFieldLength))
                return;
            {
                // Peek the size field bytes to extract payload size
                QByteArray sizeData = m_buffer.peek(m_sizeFieldLength);
                // Frame payload size is always the first 2 bytes (big-endian uint16)
                m_framePayloadSize = qFromBigEndian<uint16_t>(
                    reinterpret_cast<const uchar*>(sizeData.constData()));
                // For FIRST frames, bytes 2-5 are total message size — we don't need it here
                m_buffer.consume(m_sizeFieldLength);
            }
            m_state = State::ReadPayload;
            break;

        case State::ReadPayload:
            if (m_buffer.available() < m_framePayloadSize)
                return;
            {
                QByteArray payload = m_buffer.peek(m_framePayloadSize);
                m_buffer.consume(m_framePayloadSize);
                m_state = State::ReadHeader;
                emit frameParsed(m_currentHeader, payload);
            }
            break;
        }
    }
}

} // namespace oaa
