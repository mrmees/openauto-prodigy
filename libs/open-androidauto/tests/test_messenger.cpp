#include <QtTest/QtTest>
#include <QSignalSpy>
#include <oaa/Messenger/Messenger.hpp>
#include <oaa/Messenger/FrameHeader.hpp>
#include <oaa/Transport/ReplayTransport.hpp>
#include <QtEndian>

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
        QCOMPARE(spy[0][2].toByteArray(), versionResponse);
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
        QCOMPARE(spy[0][2].toByteArray(), QByteArray(10, 'Z'));
    }
};

QTEST_MAIN(TestMessenger)
#include "test_messenger.moc"
