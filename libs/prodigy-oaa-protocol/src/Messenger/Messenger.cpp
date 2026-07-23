#include <oaa/Messenger/Messenger.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <QtEndian>
#include <QDebug>

#include <limits>

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
    if (started_)
        return;

    parser_.reset();
    assembler_.reset();
    sendQueue_.clear();
    sending_ = false;
    handshakeFailureEmitted_ = false;
    tlsFailureEmitted_ = false;
    protocolFailureEmitted_ = false;

    connect(transport_, &ITransport::dataReceived,
            &parser_, &FrameParser::onData);
    connect(&parser_, &FrameParser::frameParsed,
            this, &Messenger::onFrameParsed);
    connect(&assembler_, &FrameAssembler::messageAssembled,
            this, &Messenger::onMessageAssembled);
    connect(&assembler_, &FrameAssembler::assemblyFailed,
            this, &Messenger::failProtocol);
    connect(transport_, &ITransport::error,
            this, &Messenger::transportError);
    started_ = true;
}

void Messenger::stop()
{
    ++lifecycleGeneration_;
    if (started_) {
        disconnect(transport_, &ITransport::dataReceived,
                   &parser_, &FrameParser::onData);
        disconnect(&parser_, &FrameParser::frameParsed,
                   this, &Messenger::onFrameParsed);
        disconnect(&assembler_, &FrameAssembler::messageAssembled,
                   this, &Messenger::onMessageAssembled);
        disconnect(&assembler_, &FrameAssembler::assemblyFailed,
                   this, &Messenger::failProtocol);
        disconnect(transport_, &ITransport::error,
                   this, &Messenger::transportError);
        started_ = false;
    }

    parser_.reset();
    assembler_.reset();
    cryptor_.deinit();
    handshakeFailureEmitted_ = false;
    tlsFailureEmitted_ = false;
    protocolFailureEmitted_ = false;
    sendQueue_.clear();
    sending_ = false;
}

void Messenger::sendMessage(uint8_t channelId, uint16_t messageId,
                             const QByteArray& payload)
{
    if (!started_) {
        qWarning() << "Messenger: dropping send while stopped, ch" << channelId
                   << "msgId" << Qt::hex << messageId;
        return;
    }
    if (tlsFailureEmitted_ || protocolFailureEmitted_) {
        qWarning() << "Messenger: dropping send after terminal protocol failure, ch"
                   << channelId << "msgId" << Qt::hex << messageId;
        return;
    }

    const uint64_t generation = lifecycleGeneration_;
    emit messageSent(channelId, messageId, payload);
    if (!started_ || generation != lifecycleGeneration_)
        return;

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

            auto encrypted = cryptor_.encrypt(framePl);
            if (!encrypted.isComplete()) {
                failTls(encrypted.error);
                return;
            }

            // Rebuild frame with encrypted payload and updated size
            QByteArray newFrame;
            newFrame.reserve(headerLen + encrypted.data.size());
            newFrame.append(frameHeader.left(2)); // header bytes

            // Rewrite size field with encrypted size
            if (hdr.frameType == FrameType::First) {
                uint16_t frameSizeBE = qToBigEndian(
                    static_cast<uint16_t>(encrypted.data.size()));
                newFrame.append(reinterpret_cast<const char*>(&frameSizeBE), 2);
                // Total size in FIRST stays as-is (refers to plaintext total)
                newFrame.append(frameHeader.mid(4, 4));
            } else {
                uint16_t frameSizeBE = qToBigEndian(
                    static_cast<uint16_t>(encrypted.data.size()));
                newFrame.append(reinterpret_cast<const char*>(&frameSizeBE), 2);
            }
            newFrame.append(encrypted.data);
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
                         EncryptionType encType, uint32_t totalMessageSize)
{
    if (!started_) {
        qWarning() << "Messenger: dropping raw send while stopped, ch" << channelId;
        return;
    }
    if (data.size() > std::numeric_limits<uint16_t>::max()) {
        qWarning() << "Messenger: rejecting raw frame whose payload exceeds 16-bit size";
        return;
    }
    if (frameType == FrameType::First) {
        if (totalMessageSize < 2
            || totalMessageSize <= static_cast<uint32_t>(data.size())
            || totalMessageSize > MAX_ASSEMBLED_MESSAGE_SIZE) {
            qWarning() << "Messenger: rejecting FIRST raw frame with invalid total size"
                       << totalMessageSize;
            return;
        }
    } else if (totalMessageSize != 0) {
        qWarning() << "Messenger: raw total size is valid only for FIRST frames";
        return;
    }
    if (tlsFailureEmitted_ || protocolFailureEmitted_) {
        qWarning() << "Messenger: dropping raw send after terminal protocol failure, ch"
                   << channelId;
        return;
    }

    FrameHeader header{channelId, frameType, encType, msgType};
    QByteArray frame;
    int sizeLen = FrameHeader::sizeFieldLength(frameType);
    frame.reserve(2 + sizeLen + data.size());

    frame.append(header.serialize());
    uint16_t sizeBE = qToBigEndian(static_cast<uint16_t>(data.size()));
    frame.append(reinterpret_cast<const char*>(&sizeBE), 2);
    if (frameType == FrameType::First) {
        // Extended size field: 4 additional bytes for total size
        uint32_t totalBE = qToBigEndian(totalMessageSize);
        frame.append(reinterpret_cast<const char*>(&totalBE), 4);
    }
    frame.append(data);

    sendQueue_.enqueue(SendItem{{frame}});
    processSendQueue();
}

void Messenger::startHandshake()
{
    handshakeFailureEmitted_ = false;
    if (!cryptor_.init(Cryptor::Role::Client)) {
        failHandshake(cryptor_.lastError());
        return;
    }
    driveHandshake();
}

bool Messenger::isEncrypted() const
{
    return cryptor_.isActive();
}

void Messenger::onFrameParsed(const FrameHeader& header,
                               const QByteArray& framePayload)
{
    if (tlsFailureEmitted_ || protocolFailureEmitted_)
        return;

    QByteArray payload = framePayload;

    // Decrypt if frame says it's encrypted
    if (header.encryptionType == EncryptionType::Encrypted) {
        auto decrypted = cryptor_.decrypt(framePayload, framePayload.size());
        if (!decrypted.isComplete()) {
            failTls(decrypted.error);
            return;
        }
        payload = std::move(decrypted.data);
    }

    assembler_.onFrame(header, payload);
}

void Messenger::onMessageAssembled(uint8_t channelId, MessageType messageType,
                                    const QByteArray& payload)
{
    if (payload.size() < 2) {
        failProtocol(QStringLiteral("assembled message cannot contain a message ID on channel %1")
                         .arg(channelId));
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
    if (!cryptor_.writeHandshakeBuffer(data)) {
        failHandshake(cryptor_.lastError());
        return;
    }
    driveHandshake();
}

void Messenger::driveHandshake()
{
    const auto result = cryptor_.doHandshake();
    const QString handshakeError = result == Cryptor::HandshakeResult::Failed
        ? cryptor_.lastHandshakeError()
        : QString{};

    // Send any outgoing handshake bytes as SSL_HANDSHAKE messages (msgId 0x0003)
    auto outgoing = cryptor_.readHandshakeBuffer();
    if (!outgoing.isComplete()) {
        failHandshake(outgoing.error);
        return;
    }
    if (!outgoing.data.isEmpty()) {
        sendMessage(0, 0x0003, outgoing.data);
    }

    if (result == Cryptor::HandshakeResult::Complete) {
        emit handshakeComplete();
    } else if (result == Cryptor::HandshakeResult::Failed) {
        failHandshake(handshakeError);
    }
}

void Messenger::failHandshake(const QString& message)
{
    if (handshakeFailureEmitted_)
        return;
    handshakeFailureEmitted_ = true;
    emit handshakeFailed(message.left(1024));
}

void Messenger::failTls(const QString& message)
{
    if (tlsFailureEmitted_)
        return;
    tlsFailureEmitted_ = true;
    sendQueue_.clear();
    emit tlsFailed(message.left(1024));
}

void Messenger::failProtocol(const QString& message)
{
    if (protocolFailureEmitted_)
        return;
    protocolFailureEmitted_ = true;
    sendQueue_.clear();
    assembler_.reset();
    emit protocolFailed(message.left(1024));
}

void Messenger::processSendQueue()
{
    if (sending_) return;
    const uint64_t generation = lifecycleGeneration_;
    sending_ = true;

    while (started_ && generation == lifecycleGeneration_
           && !sendQueue_.isEmpty()) {
        SendItem item = sendQueue_.dequeue();
        for (const auto& frame : item.frames) {
            if (!started_ || generation != lifecycleGeneration_)
                return;
            transport_->write(frame);
            if (!started_ || generation != lifecycleGeneration_)
                return;
        }
    }

    if (generation == lifecycleGeneration_)
        sending_ = false;
}

} // namespace oaa
