#pragma once

#include <oaa/Messenger/FrameType.hpp>
#include <oaa/Messenger/FrameHeader.hpp>
#include <QObject>
#include <QByteArray>

namespace oaa {

class FrameParser : public QObject {
    Q_OBJECT

public:
    explicit FrameParser(QObject* parent = nullptr);

public slots:
    void onData(const QByteArray& data);

signals:
    void frameParsed(const oaa::FrameHeader& header, const QByteArray& payload);

private:
    enum class State {
        ReadHeader,
        ReadSize,
        ReadPayload
    };

    void process();

    State m_state = State::ReadHeader;
    QByteArray m_buffer;
    FrameHeader m_currentHeader{};
    int m_sizeFieldLength = 0;
    uint16_t m_framePayloadSize = 0;
};

} // namespace oaa
