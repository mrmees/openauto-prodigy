#include <oaa/Messenger/Messenger.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <QtEndian>
#include <QDebug>

namespace oaa {

Messenger::Messenger(ITransport* transport, QObject* parent)
    : QObject(parent)
    , transport_(transport)
    , parser_(this)
    , assembler_(this)
{
}

void Messenger::start()
{
    connect(transport_, &ITransport::dataReceived,
            &parser_, &FrameParser::onData);
    connect(&parser_, &FrameParser::frameParsed,
            this, &Messenger::onFrameParsed);
    connect(&assembler_, &FrameAssembler::messageAssembled,
            this, &Messenger::onMessageAssembled);
    connect(transport_, &ITransport::error,
            this, &Messenger::transportError);
}

void Messenger::stop()
{
    disconnect(transport_, &ITransport::dataReceived,
               &parser_, &FrameParser::onData);
    disconnect(&parser_, &FrameParser::frameParsed,
               this, &Messenger::onFrameParsed);
    disconnect(&assembler_, &FrameAssembler::messageAssembled,
               this, &Messenger::onMessageAssembled);
    disconnect(transport_, &ITransport::error,
               this, &Messenger::transportError);
}

void Messenger::sendMessage(uint8_t channelId, uint16_t messageId,
                             const QByteArray& payload)
{
    emit messageSent(channelId, messageId, payload);

    // Prepend 2-byte big-endian messageId
    QByteArray fullPayload;
    fullPayload.reserve(2 + payload.size());
    uint16_t msgIdBE = qToBigEndian(messageId);
    fullPayload.append(reinterpret_cast<const char*>(&msgIdBE), 2);
    fullPayload.append(payload);

    // Message type follows aasdk convention:
    // - Channel 0 (control): always Specific
    // - Non-zero channels, msg 0x0008 (ChannelOpenResponse): Control
    // - Non-zero channels, everything else: Specific
    MessageType msgType;
    if (channelId == 0) {
        msgType = MessageType::Specific;
    } else if (messageId == 0x0008) {
        msgType = MessageType::Control;  // ChannelOpenResponse
    } else {
        msgType = MessageType::Specific;
    }

    // Determine encryption
    EncryptionType encType = encryptionPolicy_.shouldEncrypt(
        channelId, messageId, cryptor_.isActive())
            ? EncryptionType::Encrypted
            : EncryptionType::Plain;

    // Serialize into frames
    auto frames = FrameSerializer::serialize(channelId, msgType, encType, fullPayload);

    // Encrypt frame payloads if needed
    if (encType == EncryptionType::Encrypted) {
        for (int i = 0; i < frames.size(); ++i) {
            const auto& frame = frames[i];
            auto hdr = FrameHeader::parse(frame.left(2));
            int sizeLen = FrameHeader::sizeFieldLength(hdr.frameType);
            int headerLen = 2 + sizeLen;
            QByteArray frameHeader = frame.left(headerLen);
            QByteArray framePl = frame.mid(headerLen);

            QByteArray encrypted = cryptor_.encrypt(framePl);

            // Rebuild frame with encrypted payload and updated size
            QByteArray newFrame;
            newFrame.reserve(headerLen + encrypted.size());
            newFrame.append(frameHeader.left(2)); // header bytes

            // Rewrite size field with encrypted size
            if (hdr.frameType == FrameType::First) {
                uint16_t frameSizeBE = qToBigEndian(static_cast<uint16_t>(encrypted.size()));
                newFrame.append(reinterpret_cast<const char*>(&frameSizeBE), 2);
                // Total size in FIRST stays as-is (refers to plaintext total)
                newFrame.append(frameHeader.mid(4, 4));
            } else {
                uint16_t frameSizeBE = qToBigEndian(static_cast<uint16_t>(encrypted.size()));
                newFrame.append(reinterpret_cast<const char*>(&frameSizeBE), 2);
            }
            newFrame.append(encrypted);
            frames[i] = newFrame;
        }
    }

    // Queue and send — input channel (touch) gets priority
    if (channelId == ChannelId::Input) {
        sendQueue_.prepend(SendItem{std::move(frames)});
    } else {
        sendQueue_.enqueue(SendItem{std::move(frames)});
    }
    processSendQueue();
}

void Messenger::sendRaw(uint8_t channelId, const QByteArray& data,
                         FrameType frameType, MessageType msgType,
                         EncryptionType encType)
{
    FrameHeader header{channelId, frameType, encType, msgType};
    QByteArray frame;
    int sizeLen = FrameHeader::sizeFieldLength(frameType);
    frame.reserve(2 + sizeLen + data.size());

    frame.append(header.serialize());
    uint16_t sizeBE = qToBigEndian(static_cast<uint16_t>(data.size()));
    frame.append(reinterpret_cast<const char*>(&sizeBE), 2);
    if (frameType == FrameType::First) {
        // Extended size field: 4 additional bytes for total size
        uint32_t totalBE = qToBigEndian(static_cast<uint32_t>(data.size()));
        frame.append(reinterpret_cast<const char*>(&totalBE), 4);
    }
    frame.append(data);

    sendQueue_.enqueue(SendItem{{frame}});
    processSendQueue();
}

void Messenger::startHandshake()
{
    cryptor_.init(Cryptor::Role::Client);
    driveHandshake();
}

bool Messenger::isEncrypted() const
{
    return cryptor_.isActive();
}

void Messenger::onFrameParsed(const FrameHeader& header,
                               const QByteArray& framePayload)
{
    QByteArray payload = framePayload;

    // Decrypt if frame says it's encrypted
    if (header.encryptionType == EncryptionType::Encrypted) {
        payload = cryptor_.decrypt(framePayload, framePayload.size());
    }

    assembler_.onFrame(header, payload);
}

void Messenger::onMessageAssembled(uint8_t channelId, MessageType messageType,
                                    const QByteArray& payload)
{
    if (payload.size() < 2) {
        qWarning() << "Messenger: assembled message too short, ch" << channelId;
        return;
    }

    // Extract 2-byte big-endian messageId
    uint16_t messageId;
    memcpy(&messageId, payload.constData(), 2);
    messageId = qFromBigEndian(messageId);

    constexpr int msgIdSize = 2;

    // SSL handshake messages on ch0 (msgId 0x0003) before encryption is active
    // are routed to the handshake handler
    if (channelId == 0 && messageId == 0x0003 && !cryptor_.isActive()) {
        // Handshake needs a clean buffer — small and infrequent, copy is fine
        handleHandshakeData(payload.mid(msgIdSize));
        return;
    }

    // Pass full payload with offset to avoid per-message QByteArray allocation
    emit messageReceived(channelId, messageId, payload, msgIdSize);
}

void Messenger::handleHandshakeData(const QByteArray& data)
{
    cryptor_.writeHandshakeBuffer(data);
    driveHandshake();
}

void Messenger::driveHandshake()
{
    bool complete = cryptor_.doHandshake();

    // Send any outgoing handshake bytes as SSL_HANDSHAKE messages (msgId 0x0003)
    QByteArray outgoing = cryptor_.readHandshakeBuffer();
    if (!outgoing.isEmpty()) {
        sendMessage(0, 0x0003, outgoing);
    }

    if (complete) {
        emit handshakeComplete();
    }
}

void Messenger::processSendQueue()
{
    if (sending_) return;
    sending_ = true;

    while (!sendQueue_.isEmpty()) {
        SendItem item = sendQueue_.dequeue();
        for (const auto& frame : item.frames) {
            transport_->write(frame);
        }
    }

    sending_ = false;
}

} // namespace oaa
