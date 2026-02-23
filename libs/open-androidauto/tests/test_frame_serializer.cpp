#include <QtTest/QtTest>
#include <oaa/Messenger/FrameSerializer.hpp>
#include <QtEndian>

class TestFrameSerializer : public QObject {
    Q_OBJECT

private:
    // Helper to extract header from a frame
    oaa::FrameHeader parseHeader(const QByteArray& frame) {
        return oaa::FrameHeader::parse(frame.left(2));
    }

    // Helper to extract frame payload size from size field
    uint16_t parseFrameSize(const QByteArray& frame, oaa::FrameType type) {
        int offset = 2; // after header
        uint16_t size;
        memcpy(&size, frame.constData() + offset, 2);
        return qFromBigEndian(size);
    }

    // Helper to extract total size from FIRST frame's extended size field
    uint32_t parseTotalSize(const QByteArray& frame) {
        uint32_t size;
        memcpy(&size, frame.constData() + 4, 4); // offset 2 (header) + 2 (frame size)
        return qFromBigEndian(size);
    }

    // Helper to extract payload from a frame
    QByteArray extractPayload(const QByteArray& frame, oaa::FrameType type) {
        int headerLen = 2 + oaa::FrameHeader::sizeFieldLength(type);
        return frame.mid(headerLen);
    }

private slots:
    void testSmallMessage() {
        QByteArray payload(10, 'A');
        auto frames = oaa::FrameSerializer::serialize(
            1, oaa::MessageType::Specific, oaa::EncryptionType::Plain, payload);

        QCOMPARE(frames.size(), 1);

        auto hdr = parseHeader(frames[0]);
        QCOMPARE(hdr.channelId, uint8_t(1));
        QCOMPARE(hdr.frameType, oaa::FrameType::Bulk);
        QCOMPARE(hdr.encryptionType, oaa::EncryptionType::Plain);
        QCOMPARE(hdr.messageType, oaa::MessageType::Specific);

        QCOMPARE(parseFrameSize(frames[0], oaa::FrameType::Bulk), uint16_t(10));
        QCOMPARE(extractPayload(frames[0], oaa::FrameType::Bulk), payload);
    }

    void testExactMaxPayload() {
        QByteArray payload(16384, 'B');
        auto frames = oaa::FrameSerializer::serialize(
            2, oaa::MessageType::Control, oaa::EncryptionType::Encrypted, payload);

        QCOMPARE(frames.size(), 1);

        auto hdr = parseHeader(frames[0]);
        QCOMPARE(hdr.frameType, oaa::FrameType::Bulk);
        QCOMPARE(parseFrameSize(frames[0], oaa::FrameType::Bulk), uint16_t(16384));
        QCOMPARE(extractPayload(frames[0], oaa::FrameType::Bulk), payload);
    }

    void testOneByteOver() {
        QByteArray payload(16385, 'C');
        auto frames = oaa::FrameSerializer::serialize(
            3, oaa::MessageType::Specific, oaa::EncryptionType::Plain, payload);

        QCOMPARE(frames.size(), 2);

        // FIRST frame
        auto hdr0 = parseHeader(frames[0]);
        QCOMPARE(hdr0.frameType, oaa::FrameType::First);
        QCOMPARE(hdr0.channelId, uint8_t(3));
        QCOMPARE(parseFrameSize(frames[0], oaa::FrameType::First), uint16_t(16384));
        QCOMPARE(parseTotalSize(frames[0]), uint32_t(16385));
        QCOMPARE(extractPayload(frames[0], oaa::FrameType::First).size(), 16384);

        // LAST frame
        auto hdr1 = parseHeader(frames[1]);
        QCOMPARE(hdr1.frameType, oaa::FrameType::Last);
        QCOMPARE(parseFrameSize(frames[1], oaa::FrameType::Last), uint16_t(1));
        QCOMPARE(extractPayload(frames[1], oaa::FrameType::Last).size(), 1);

        // Verify reassembled payload matches
        QByteArray reassembled;
        reassembled.append(extractPayload(frames[0], oaa::FrameType::First));
        reassembled.append(extractPayload(frames[1], oaa::FrameType::Last));
        QCOMPARE(reassembled, payload);
    }

    void testLargeMessage() {
        // 40000 bytes → FIRST(16384) + MIDDLE(16384) + LAST(7232)
        QByteArray payload(40000, 'D');
        auto frames = oaa::FrameSerializer::serialize(
            4, oaa::MessageType::Control, oaa::EncryptionType::Encrypted, payload);

        QCOMPARE(frames.size(), 3);

        // FIRST
        auto hdr0 = parseHeader(frames[0]);
        QCOMPARE(hdr0.frameType, oaa::FrameType::First);
        QCOMPARE(hdr0.encryptionType, oaa::EncryptionType::Encrypted);
        QCOMPARE(hdr0.messageType, oaa::MessageType::Control);
        QCOMPARE(parseTotalSize(frames[0]), uint32_t(40000));
        QCOMPARE(extractPayload(frames[0], oaa::FrameType::First).size(), 16384);

        // MIDDLE
        auto hdr1 = parseHeader(frames[1]);
        QCOMPARE(hdr1.frameType, oaa::FrameType::Middle);
        QCOMPARE(extractPayload(frames[1], oaa::FrameType::Middle).size(), 16384);

        // LAST
        auto hdr2 = parseHeader(frames[2]);
        QCOMPARE(hdr2.frameType, oaa::FrameType::Last);
        QCOMPARE(extractPayload(frames[2], oaa::FrameType::Last).size(), 7232);

        // Verify reassembled payload matches
        QByteArray reassembled;
        reassembled.append(extractPayload(frames[0], oaa::FrameType::First));
        reassembled.append(extractPayload(frames[1], oaa::FrameType::Middle));
        reassembled.append(extractPayload(frames[2], oaa::FrameType::Last));
        QCOMPARE(reassembled, payload);
    }

    void testEmptyPayload() {
        QByteArray payload;
        auto frames = oaa::FrameSerializer::serialize(
            0, oaa::MessageType::Specific, oaa::EncryptionType::Plain, payload);

        QCOMPARE(frames.size(), 1);

        auto hdr = parseHeader(frames[0]);
        QCOMPARE(hdr.frameType, oaa::FrameType::Bulk);
        QCOMPARE(parseFrameSize(frames[0], oaa::FrameType::Bulk), uint16_t(0));
        QCOMPARE(extractPayload(frames[0], oaa::FrameType::Bulk).size(), 0);

        // Total frame size should be header(2) + size(2) = 4
        QCOMPARE(frames[0].size(), 4);
    }
};

QTEST_MAIN(TestFrameSerializer)
#include "test_frame_serializer.moc"
