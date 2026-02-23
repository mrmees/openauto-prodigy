#pragma once

#include <oaa/Messenger/FrameType.hpp>
#include <oaa/Messenger/FrameParser.hpp>
#include <oaa/Messenger/FrameAssembler.hpp>
#include <oaa/Messenger/FrameSerializer.hpp>
#include <oaa/Messenger/Cryptor.hpp>
#include <oaa/Messenger/EncryptionPolicy.hpp>
#include <oaa/Transport/ITransport.hpp>

#include <QObject>
#include <QByteArray>
#include <QQueue>

namespace oaa {

class Messenger : public QObject {
    Q_OBJECT

public:
    explicit Messenger(ITransport* transport, QObject* parent = nullptr);

    void start();
    void stop();

    void sendMessage(uint8_t channelId, uint16_t messageId,
                     const QByteArray& payload);

    void sendRaw(uint8_t channelId, const QByteArray& data,
                 FrameType frameType, MessageType msgType,
                 EncryptionType encType);

    void startHandshake();
    bool isEncrypted() const;

signals:
    void messageReceived(uint8_t channelId, uint16_t messageId,
                         const QByteArray& payload);
    void messageSent(uint8_t channelId, uint16_t messageId,
                     const QByteArray& payload);
    void handshakeComplete();
    void transportError(const QString& message);

private:
    struct SendItem {
        QList<QByteArray> frames;
    };

    void onFrameParsed(const FrameHeader& header, const QByteArray& framePayload);
    void onMessageAssembled(uint8_t channelId, MessageType messageType,
                            const QByteArray& payload);
    void handleHandshakeData(const QByteArray& data);
    void driveHandshake();
    void processSendQueue();

    ITransport* transport_;
    FrameParser parser_;
    FrameAssembler assembler_;
    Cryptor cryptor_;
    EncryptionPolicy encryptionPolicy_;

    QQueue<SendItem> sendQueue_;
    bool sending_ = false;
};

} // namespace oaa
