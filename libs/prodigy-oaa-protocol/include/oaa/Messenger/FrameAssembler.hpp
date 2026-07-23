#pragma once

#include <oaa/Messenger/FrameType.hpp>
#include <oaa/Messenger/FrameHeader.hpp>
#include <oaa/Version.hpp>
#include <QObject>
#include <QByteArray>
#include <QHash>
#include <QString>

namespace oaa {

class FrameAssembler : public QObject {
    Q_OBJECT

public:
    explicit FrameAssembler(QObject* parent = nullptr);
    FrameAssembler(uint32_t maxMessageSize, uint64_t maxAggregateSize,
                   QObject* parent = nullptr);
    void reset();

public slots:
    void onFrame(const oaa::FrameHeader& header, const QByteArray& payload);

signals:
    void messageAssembled(uint8_t channelId, oaa::MessageType messageType,
                          const QByteArray& payload);
    void assemblyFailed(const QString& message);

private:
    struct PartialMessage {
        QByteArray payload;
        uint32_t declaredSize = 0;
        MessageType messageType = MessageType::Specific;
        EncryptionType encryptionType = EncryptionType::Plain;
    };

    void release(uint8_t channelId);
    void fail(const QString& message);
    bool flagsMatch(const PartialMessage& partial,
                    const FrameHeader& header) const;

    QHash<uint8_t, PartialMessage> m_partials;
    uint64_t m_reservedBytes = 0;
    uint32_t m_maxMessageSize = MAX_ASSEMBLED_MESSAGE_SIZE;
    uint64_t m_maxAggregateSize = MAX_IN_FLIGHT_ASSEMBLY_SIZE;
};

} // namespace oaa
