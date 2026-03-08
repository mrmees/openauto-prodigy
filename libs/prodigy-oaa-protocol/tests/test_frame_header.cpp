#include <QtTest/QtTest>
#include <oaa/Messenger/FrameHeader.hpp>

class TestFrameHeader : public QObject {
    Q_OBJECT
private slots:
    void testParseBulkPlainControl() {
        // byte[0]=0x00 (channelId=0), byte[1]=0x07 (Bulk=0x03 | Control=0x04)
        QByteArray data("\x00\x07", 2);
        auto hdr = oaa::FrameHeader::parse(data);
        QCOMPARE(hdr.channelId, uint8_t(0));
        QCOMPARE(hdr.frameType, oaa::FrameType::Bulk);
        QCOMPARE(hdr.encryptionType, oaa::EncryptionType::Plain);
        QCOMPARE(hdr.messageType, oaa::MessageType::Control);
    }

    void testParseFirstEncryptedSpecific() {
        // byte[0]=0x03 (channelId=3), byte[1]=0x09 (First=0x01 | Encrypted=0x08)
        QByteArray data("\x03\x09", 2);
        auto hdr = oaa::FrameHeader::parse(data);
        QCOMPARE(hdr.channelId, uint8_t(3));
        QCOMPARE(hdr.frameType, oaa::FrameType::First);
        QCOMPARE(hdr.encryptionType, oaa::EncryptionType::Encrypted);
        QCOMPARE(hdr.messageType, oaa::MessageType::Specific);
    }

    void testSerialize() {
        oaa::FrameHeader hdr{5, oaa::FrameType::Last, oaa::EncryptionType::Encrypted, oaa::MessageType::Control};
        QByteArray bytes = hdr.serialize();
        QCOMPARE(bytes.size(), 2);
        QCOMPARE(static_cast<uint8_t>(bytes[0]), uint8_t(5));
        // Last=0x02 | Encrypted=0x08 | Control=0x04 = 0x0E
        QCOMPARE(static_cast<uint8_t>(bytes[1]), uint8_t(0x0E));
    }

    void testRoundTrip() {
        oaa::FrameHeader original{7, oaa::FrameType::Middle, oaa::EncryptionType::Plain, oaa::MessageType::Specific};
        QByteArray bytes = original.serialize();
        auto parsed = oaa::FrameHeader::parse(bytes);
        QCOMPARE(parsed.channelId, original.channelId);
        QCOMPARE(parsed.frameType, original.frameType);
        QCOMPARE(parsed.encryptionType, original.encryptionType);
        QCOMPARE(parsed.messageType, original.messageType);
    }

    void testSizeFieldLength() {
        QCOMPARE(oaa::FrameHeader::sizeFieldLength(oaa::FrameType::First), 6);
        QCOMPARE(oaa::FrameHeader::sizeFieldLength(oaa::FrameType::Middle), 2);
        QCOMPARE(oaa::FrameHeader::sizeFieldLength(oaa::FrameType::Last), 2);
        QCOMPARE(oaa::FrameHeader::sizeFieldLength(oaa::FrameType::Bulk), 2);
    }
};

QTEST_MAIN(TestFrameHeader)
#include "test_frame_header.moc"
