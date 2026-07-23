#include <QtTest/QtTest>
#include <QSignalSpy>
#include <oaa/Messenger/Messenger.hpp>
#include <oaa/Messenger/FrameHeader.hpp>
#include <oaa/Transport/ReplayTransport.hpp>
#include <QtEndian>
#include <functional>

class ReentrantReplayTransport : public oaa::ReplayTransport {
public:
    using ReplayTransport::ReplayTransport;

    void write(const QByteArray& data) override {
        ReplayTransport::write(data);
        ++writeCount;
        if (onWrite)
            onWrite(writeCount);
    }

    int writeCount = 0;
    std::function<void(int)> onWrite;
};

class TestMessenger : public QObject {
    Q_OBJECT

private:
    // Helper to extract header from a raw frame
    oaa::FrameHeader parseHeader(const QByteArray& frame) {
        return oaa::FrameHeader::parse(frame.left(2));
    }

    // Helper to extract frame payload size from the size field
    uint16_t parseFrameSize(const QByteArray& frame) {
        uint16_t size;
        memcpy(&size, frame.constData() + 2, 2);
        return qFromBigEndian(size);
    }

    // Helper to extract payload from a frame
    QByteArray extractPayload(const QByteArray& frame, oaa::FrameType type) {
        int headerLen = 2 + oaa::FrameHeader::sizeFieldLength(type);
        return frame.mid(headerLen);
    }

    // Helper to build a hand-crafted frame for receive tests
    QByteArray buildFrame(uint8_t channelId, oaa::FrameType ft,
                          oaa::MessageType mt, oaa::EncryptionType et,
                          const QByteArray& payload, qint32 totalSize = -1)
    {
        oaa::FrameHeader hdr{channelId, ft, et, mt};
        QByteArray frame;
        int sizeLen = oaa::FrameHeader::sizeFieldLength(ft);
        frame.reserve(2 + sizeLen + payload.size());

        frame.append(hdr.serialize());
        uint16_t sizeBE = qToBigEndian(static_cast<uint16_t>(payload.size()));
        frame.append(reinterpret_cast<const char*>(&sizeBE), 2);
        if (ft == oaa::FrameType::First) {
            uint32_t totalBE = qToBigEndian(static_cast<uint32_t>(totalSize));
            frame.append(reinterpret_cast<const char*>(&totalBE), 4);
        }
        frame.append(payload);
        return frame;
    }

    bool driveHandshake(oaa::Messenger& messenger,
                        oaa::ReplayTransport& transport,
                        oaa::Cryptor& server)
    {
        if (!server.init(oaa::Cryptor::Role::Server))
            return false;

        int writeCursor = 0;
        messenger.startHandshake();
        for (int round = 0; round < 20; ++round) {
            const auto written = transport.writtenData();
            while (writeCursor < written.size()) {
                const QByteArray& frame = written[writeCursor++];
                const auto header = parseHeader(frame);
                const QByteArray payload = extractPayload(frame, header.frameType);
                if (payload.size() < 2)
                    return false;
                const uint16_t messageId = qFromBigEndian<uint16_t>(
                    reinterpret_cast<const uchar*>(payload.constData()));
                if (messageId != 0x0003
                    || !server.writeHandshakeBuffer(payload.mid(2)))
                    return false;
            }

            server.doHandshake();
            auto serverOut = server.readHandshakeBuffer();
            if (!serverOut.isComplete())
                return false;
            if (!serverOut.data.isEmpty()) {
                QByteArray payload;
                const uint16_t handshakeId = qToBigEndian(uint16_t(0x0003));
                payload.append(reinterpret_cast<const char*>(&handshakeId), 2);
                payload.append(serverOut.data);
                transport.feedData(buildFrame(
                    0, oaa::FrameType::Bulk, oaa::MessageType::Specific,
                    oaa::EncryptionType::Plain, payload));
            }

            if (messenger.isEncrypted() && server.isActive())
                return true;
        }
        return false;
    }

private slots:
    void testSendPlainControlMessage() {
        oaa::ReplayTransport transport;
        oaa::Messenger messenger(&transport);

        transport.simulateConnect();
        messenger.start();

        // Send VERSION_REQUEST (ch0, msgId 0x0001) with 4-byte version payload
        QByteArray versionPayload(4, '\x01');
        messenger.sendMessage(0, 0x0001, versionPayload);

        auto written = transport.writtenData();
        QCOMPARE(written.size(), 1);

        QByteArray frame = written[0];

        // Header byte 0 = 0x00 (channel 0)
        QCOMPARE(static_cast<uint8_t>(frame[0]), uint8_t(0x00));

        // Header byte 1 = Bulk(0x03) | Specific(0x00) | Plain(0x00) = 0x03
        // AA wire protocol uses MessageType::Specific for all messages,
        // including those on channel 0 (control channel).
        QCOMPARE(static_cast<uint8_t>(frame[1]), uint8_t(0x03));

        // Size field = 6 (2-byte msgId + 4-byte payload)
        QCOMPARE(parseFrameSize(frame), uint16_t(6));

        // Payload starts with messageId 0x0001 in big-endian
        QByteArray framePayload = extractPayload(frame, oaa::FrameType::Bulk);
        QCOMPARE(framePayload.size(), 6);
        QCOMPARE(static_cast<uint8_t>(framePayload[0]), uint8_t(0x00));
        QCOMPARE(static_cast<uint8_t>(framePayload[1]), uint8_t(0x01));
        QCOMPARE(framePayload.mid(2), versionPayload);
    }

    void testSendServiceChannelUsesControlBit() {
        oaa::ReplayTransport transport;
        oaa::Messenger messenger(&transport);

        transport.simulateConnect();
        messenger.start();

        // Send CHANNEL_OPEN_RESPONSE (ch3, msgId 0x0008) — service channel
        QByteArray payload(2, '\x00'); // Status::OK protobuf
        messenger.sendMessage(3, 0x0008, payload);

        auto written = transport.writtenData();
        QCOMPARE(written.size(), 1);

        QByteArray frame = written[0];

        // Header byte 0 = 0x03 (channel 3)
        QCOMPARE(static_cast<uint8_t>(frame[0]), uint8_t(0x03));

        // Header byte 1 = Bulk(0x03) | Control(0x04) | Plain(0x00) = 0x07
        // Non-zero channels use MessageType::Control in aasdk.
        QCOMPARE(static_cast<uint8_t>(frame[1]), uint8_t(0x07));
    }

    void testReceivePlainControlMessage() {
        oaa::ReplayTransport transport;
        oaa::Messenger messenger(&transport);

        qRegisterMetaType<uint8_t>("uint8_t");
        qRegisterMetaType<uint16_t>("uint16_t");
        QSignalSpy spy(&messenger, &oaa::Messenger::messageReceived);

        transport.simulateConnect();
        messenger.start();

        // Build a BULK frame: ch0, Control, Plain
        // Payload: msgId 0x0002 (BE) + 6-byte version response
        QByteArray versionResponse(6, '\x02');
        QByteArray msgPayload;
        uint16_t msgIdBE = qToBigEndian(uint16_t(0x0002));
        msgPayload.append(reinterpret_cast<const char*>(&msgIdBE), 2);
        msgPayload.append(versionResponse);

        QByteArray frame = buildFrame(0, oaa::FrameType::Bulk,
                                       oaa::MessageType::Control,
                                       oaa::EncryptionType::Plain,
                                       msgPayload);

        // Feed into transport
        transport.feedData(frame);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].value<uint8_t>(), uint8_t(0));
        QCOMPARE(spy[0][1].value<uint16_t>(), uint16_t(0x0002));
        // Signal now carries full payload + dataOffset instead of stripped payload
        int dataOffset = spy[0][3].toInt();
        QByteArray fullPayload = spy[0][2].toByteArray();
        QCOMPARE(fullPayload.mid(dataOffset), versionResponse);
    }

    void testSendLargeMessageFragmented() {
        oaa::ReplayTransport transport;
        oaa::Messenger messenger(&transport);

        transport.simulateConnect();
        messenger.start();

        // 20000-byte payload → fullPayload = 20002 bytes (with msgId prefix)
        // 20002 > 16384 → FIRST(16384) + LAST(3618) = 2 frames
        QByteArray payload(20000, 'X');
        messenger.sendMessage(1, 0x0100, payload);

        auto written = transport.writtenData();
        QVERIFY(written.size() >= 2);

        // First frame should be FIRST type
        auto hdr0 = parseHeader(written[0]);
        QCOMPARE(hdr0.frameType, oaa::FrameType::First);
        QCOMPARE(hdr0.channelId, uint8_t(1));

        // Last frame should be LAST type
        auto hdrLast = parseHeader(written[written.size() - 1]);
        QCOMPARE(hdrLast.frameType, oaa::FrameType::Last);

        // Verify total payload across all frames = 20002 (msgId + payload)
        int totalPayload = 0;
        for (const auto& f : written) {
            auto h = parseHeader(f);
            totalPayload += extractPayload(f, h.frameType).size();
        }
        QCOMPARE(totalPayload, 20002);
    }

    void testReceiveMultiFrameMessage() {
        oaa::ReplayTransport transport;
        oaa::Messenger messenger(&transport);

        qRegisterMetaType<uint8_t>("uint8_t");
        qRegisterMetaType<uint16_t>("uint16_t");
        QSignalSpy spy(&messenger, &oaa::Messenger::messageReceived);

        transport.simulateConnect();
        messenger.start();

        // Build a message split across FIRST + LAST
        // Full message: msgId 0x0005 (BE) + 10 bytes data = 12 bytes
        QByteArray fullMessage;
        uint16_t msgIdBE = qToBigEndian(uint16_t(0x0005));
        fullMessage.append(reinterpret_cast<const char*>(&msgIdBE), 2);
        fullMessage.append(QByteArray(10, 'Z'));

        QByteArray part1 = fullMessage.left(6);
        QByteArray part2 = fullMessage.mid(6);

        QByteArray firstFrame = buildFrame(3, oaa::FrameType::First,
                                            oaa::MessageType::Specific,
                                            oaa::EncryptionType::Plain,
                                            part1, fullMessage.size());
        QByteArray lastFrame = buildFrame(3, oaa::FrameType::Last,
                                           oaa::MessageType::Specific,
                                           oaa::EncryptionType::Plain,
                                           part2);

        // Feed FIRST — should not emit yet
        transport.feedData(firstFrame);
        QCOMPARE(spy.count(), 0);

        // Feed LAST — should emit complete message
        transport.feedData(lastFrame);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].value<uint8_t>(), uint8_t(3));
        QCOMPARE(spy[0][1].value<uint16_t>(), uint16_t(0x0005));
        // Signal now carries full payload + dataOffset instead of stripped payload
        int dataOffset = spy[0][3].toInt();
        QByteArray fullPayload = spy[0][2].toByteArray();
        QCOMPARE(fullPayload.mid(dataOffset), QByteArray(10, 'Z'));
    }

    void testRepeatedStartDoesNotDuplicateDelivery() {
        oaa::ReplayTransport transport;
        oaa::Messenger messenger(&transport);
        QSignalSpy messageSpy(&messenger, &oaa::Messenger::messageReceived);
        QSignalSpy errorSpy(&messenger, &oaa::Messenger::transportError);

        messenger.start();
        messenger.start();

        QByteArray payload;
        const uint16_t messageId = qToBigEndian(uint16_t(0x1234));
        payload.append(reinterpret_cast<const char*>(&messageId), 2);
        payload.append("once");
        transport.feedData(buildFrame(1, oaa::FrameType::Bulk,
                                      oaa::MessageType::Specific,
                                      oaa::EncryptionType::Plain, payload));
        QMetaObject::invokeMethod(&transport, "error", Qt::DirectConnection,
                                  Q_ARG(QString, QStringLiteral("once")));

        QCOMPARE(messageSpy.count(), 1);
        QCOMPARE(errorSpy.count(), 1);
    }

    void testStopStartDiscardsPartialParserState() {
        oaa::ReplayTransport transport;
        oaa::Messenger messenger(&transport);
        QSignalSpy messageSpy(&messenger, &oaa::Messenger::messageReceived);

        QByteArray payload;
        const uint16_t messageId = qToBigEndian(uint16_t(0x1234));
        payload.append(reinterpret_cast<const char*>(&messageId), 2);
        payload.append("fresh");
        const QByteArray frame = buildFrame(1, oaa::FrameType::Bulk,
                                            oaa::MessageType::Specific,
                                            oaa::EncryptionType::Plain, payload);

        messenger.start();
        transport.feedData(frame.left(3));
        messenger.stop();
        messenger.start();
        transport.feedData(frame);

        QCOMPARE(messageSpy.count(), 1);
        QCOMPARE(messageSpy[0][1].value<uint16_t>(), uint16_t(0x1234));
        QCOMPARE(messageSpy[0][2].toByteArray().mid(messageSpy[0][3].toInt()),
                 QByteArray("fresh"));
    }

    void testStopStartDiscardsPartialAssembledMessage() {
        oaa::ReplayTransport transport;
        oaa::Messenger messenger(&transport);
        QSignalSpy messageSpy(&messenger, &oaa::Messenger::messageReceived);
        QSignalSpy failureSpy(&messenger, &oaa::Messenger::protocolFailed);

        QByteArray oldPayload;
        const uint16_t oldId = qToBigEndian(uint16_t(0x1111));
        oldPayload.append(reinterpret_cast<const char*>(&oldId), 2);
        oldPayload.append("old");

        messenger.start();
        transport.feedData(buildFrame(3, oaa::FrameType::First,
                                      oaa::MessageType::Specific,
                                      oaa::EncryptionType::Plain,
                                      oldPayload, oldPayload.size() + 4));
        messenger.stop();
        messenger.start();
        transport.feedData(buildFrame(3, oaa::FrameType::Last,
                                      oaa::MessageType::Specific,
                                      oaa::EncryptionType::Plain,
                                      QByteArray("tail")));
        QCOMPARE(messageSpy.count(), 0);
        QCOMPARE(failureSpy.count(), 1);

        // A new lifecycle clears the terminal protocol-failure latch.
        messenger.stop();
        messenger.start();

        QByteArray freshPayload;
        const uint16_t freshId = qToBigEndian(uint16_t(0x2222));
        freshPayload.append(reinterpret_cast<const char*>(&freshId), 2);
        freshPayload.append("fresh");
        transport.feedData(buildFrame(3, oaa::FrameType::Bulk,
                                      oaa::MessageType::Specific,
                                      oaa::EncryptionType::Plain,
                                      freshPayload));
        QCOMPARE(messageSpy.count(), 1);
        QCOMPARE(messageSpy[0][1].value<uint16_t>(), uint16_t(0x2222));
    }

    void testStopFromMessageSentCancelsPendingWrite() {
        oaa::ReplayTransport transport;
        oaa::Messenger messenger(&transport);
        messenger.start();
        connect(&messenger, &oaa::Messenger::messageSent,
                &messenger, &oaa::Messenger::stop, Qt::DirectConnection);

        messenger.sendMessage(3, 0x1234, QByteArray("cancel"));

        QCOMPARE(transport.writtenData().size(), 0);
    }

    void testStopDuringFirstWriteCancelsRemainingFrames() {
        ReentrantReplayTransport transport;
        oaa::Messenger messenger(&transport);
        messenger.start();
        transport.onWrite = [&messenger](int writeCount) {
            if (writeCount == 1)
                messenger.stop();
        };

        messenger.sendMessage(3, 0x1234, QByteArray(20000, 'X'));

        QCOMPARE(transport.writtenData().size(), 1);
    }

    void testFatalHandshakeEmitsOnceWithDiagnostic() {
        oaa::ReplayTransport transport;
        oaa::Messenger messenger(&transport);
        QSignalSpy failureSpy(&messenger, &oaa::Messenger::handshakeFailed);

        messenger.start();
        messenger.startHandshake();

        QByteArray handshakePayload;
        const uint16_t handshakeId = qToBigEndian(uint16_t(0x0003));
        handshakePayload.append(reinterpret_cast<const char*>(&handshakeId), 2);
        handshakePayload.append(QByteArray(64, 'X'));
        const QByteArray malformedFrame = buildFrame(
            0, oaa::FrameType::Bulk, oaa::MessageType::Specific,
            oaa::EncryptionType::Plain, handshakePayload);

        transport.feedData(malformedFrame);
        transport.feedData(malformedFrame);

        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(!failureSpy[0][0].toString().isEmpty());
        QVERIFY(!messenger.isEncrypted());
    }

    void testFatalEncryptedRecordEmitsOnceAndForwardsNothing() {
        oaa::ReplayTransport transport;
        oaa::Messenger messenger(&transport);
        oaa::Cryptor server;
        QSignalSpy failureSpy(&messenger, &oaa::Messenger::tlsFailed);
        QSignalSpy messageSpy(&messenger, &oaa::Messenger::messageReceived);

        messenger.start();
        QVERIFY(driveHandshake(messenger, transport, server));
        transport.clearWritten();
        messageSpy.clear();

        auto encrypted = server.encrypt(QByteArrayLiteral("encrypted payload"));
        QVERIFY(encrypted.isComplete());
        encrypted.data[encrypted.data.size() - 1] ^= char(0x01);
        const QByteArray malformedFrame = buildFrame(
            3, oaa::FrameType::Bulk, oaa::MessageType::Specific,
            oaa::EncryptionType::Encrypted, encrypted.data);

        transport.feedData(malformedFrame);
        transport.feedData(malformedFrame);

        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(!failureSpy[0][0].toString().isEmpty());
        QCOMPARE(messageSpy.count(), 0);

        messenger.sendMessage(3, 0x1234, QByteArrayLiteral("must not send"));
        QCOMPARE(transport.writtenData().size(), 0);
        QCOMPARE(failureSpy.count(), 1);
    }

    void testAssemblyViolationEmitsOnceAndForwardsNothing() {
        oaa::ReplayTransport transport;
        oaa::Messenger messenger(&transport);
        QSignalSpy failureSpy(&messenger, &oaa::Messenger::protocolFailed);
        QSignalSpy messageSpy(&messenger, &oaa::Messenger::messageReceived);

        messenger.start();
        const QByteArray invalidFirst = buildFrame(
            3, oaa::FrameType::First, oaa::MessageType::Specific,
            oaa::EncryptionType::Plain, QByteArrayLiteral("oversized"), 4);
        transport.feedData(invalidFirst);
        transport.feedData(invalidFirst);

        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(!failureSpy[0][0].toString().isEmpty());
        QCOMPARE(messageSpy.count(), 0);

        messenger.sendMessage(3, 0x1234, QByteArrayLiteral("must not send"));
        QCOMPARE(transport.writtenData().size(), 0);
    }
};

QTEST_MAIN(TestMessenger)
#include "test_messenger.moc"
