#pragma once

#include <oaa/Messenger/FrameType.hpp>
#include <oaa/Messenger/FrameHeader.hpp>
#include <QObject>
#include <QByteArray>
#include <QHash>

namespace oaa {

class FrameAssembler : public QObject {
    Q_OBJECT

public:
    explicit FrameAssembler(QObject* parent = nullptr);

public slots:
    void onFrame(const oaa::FrameHeader& header, const QByteArray& payload);

signals:
    void messageAssembled(uint8_t channelId, oaa::MessageType messageType,
                          const QByteArray& payload);

private:
    QHash<uint8_t, QByteArray> m_buffers;
    QHash<uint8_t, MessageType> m_messageTypes;
};

} // namespace oaa
