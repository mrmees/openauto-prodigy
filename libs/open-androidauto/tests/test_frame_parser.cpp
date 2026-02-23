#include <QtTest/QtTest>
#include <QSignalSpy>
#include <oaa/Messenger/FrameParser.hpp>
#include <oaa/Messenger/FrameSerializer.hpp>

class TestFrameParser : public QObject {
    Q_OBJECT

private:
    // Helper: build a single BULK wire frame
    QByteArray makeBulkFrame(uint8_t ch, const QByteArray& payload) {
        auto frames = oaa::FrameSerializer::serialize(
            ch, oaa::MessageType::Specific, oaa::EncryptionType::Plain, payload);
        Q_ASSERT(frames.size() == 1);
        return frames[0];
    }

private slots:
    void testCompleteFrame() {
        oaa::FrameParser parser;
        QSignalSpy spy(&parser, &oaa::FrameParser::frameParsed);

        QByteArray payload("hello");
        QByteArray frame = makeBulkFrame(5, payload);

        parser.onData(frame);

        QCOMPARE(spy.count(), 1);
        auto header = spy[0][0].value<oaa::FrameHeader>();
        QCOMPARE(header.channelId, uint8_t(5));
        QCOMPARE(header.frameType, oaa::FrameType::Bulk);
        QCOMPARE(header.encryptionType, oaa::EncryptionType::Plain);
        QCOMPARE(header.messageType, oaa::MessageType::Specific);
        QCOMPARE(spy[0][1].toByteArray(), payload);
    }

    void testByteByByte() {
        oaa::FrameParser parser;
        QSignalSpy spy(&parser, &oaa::FrameParser::frameParsed);

        QByteArray payload("test");
        QByteArray frame = makeBulkFrame(2, payload);

        // Feed one byte at a time
        for (int i = 0; i < frame.size(); ++i) {
            parser.onData(frame.mid(i, 1));
        }

        QCOMPARE(spy.count(), 1);
        auto header = spy[0][0].value<oaa::FrameHeader>();
        QCOMPARE(header.channelId, uint8_t(2));
        QCOMPARE(header.frameType, oaa::FrameType::Bulk);
        QCOMPARE(spy[0][1].toByteArray(), payload);
    }

    void testTwoFramesOneChunk() {
        oaa::FrameParser parser;
        QSignalSpy spy(&parser, &oaa::FrameParser::frameParsed);

        QByteArray payload1("AAAA");
        QByteArray payload2("BBBBBB");
        QByteArray frame1 = makeBulkFrame(1, payload1);
        QByteArray frame2 = makeBulkFrame(3, payload2);

        // Feed both frames concatenated
        parser.onData(frame1 + frame2);

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy[0][0].value<oaa::FrameHeader>().channelId, uint8_t(1));
        QCOMPARE(spy[0][1].toByteArray(), payload1);
        QCOMPARE(spy[1][0].value<oaa::FrameHeader>().channelId, uint8_t(3));
        QCOMPARE(spy[1][1].toByteArray(), payload2);
    }

    void testSplitAcrossChunks() {
        oaa::FrameParser parser;
        QSignalSpy spy(&parser, &oaa::FrameParser::frameParsed);

        QByteArray payload("split-me");
        QByteArray frame = makeBulkFrame(7, payload);

        // Split at byte 3 (in the middle of the size field)
        parser.onData(frame.left(3));
        QCOMPARE(spy.count(), 0);

        parser.onData(frame.mid(3));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].value<oaa::FrameHeader>().channelId, uint8_t(7));
        QCOMPARE(spy[0][1].toByteArray(), payload);
    }

    void testFirstFrameExtendedSize() {
        oaa::FrameParser parser;
        QSignalSpy spy(&parser, &oaa::FrameParser::frameParsed);

        // Create a multi-frame message (just over max payload)
        QByteArray bigPayload(16385, 'X');
        auto frames = oaa::FrameSerializer::serialize(
            10, oaa::MessageType::Control, oaa::EncryptionType::Encrypted, bigPayload);
        QCOMPARE(frames.size(), 2); // FIRST + LAST

        // Feed the FIRST frame
        parser.onData(frames[0]);

        QCOMPARE(spy.count(), 1);
        auto header = spy[0][0].value<oaa::FrameHeader>();
        QCOMPARE(header.channelId, uint8_t(10));
        QCOMPARE(header.frameType, oaa::FrameType::First);
        QCOMPARE(header.encryptionType, oaa::EncryptionType::Encrypted);
        QCOMPARE(header.messageType, oaa::MessageType::Control);
        // FIRST frame payload should be exactly FRAME_MAX_PAYLOAD bytes
        QCOMPARE(spy[0][1].toByteArray().size(), oaa::FrameSerializer::FRAME_MAX_PAYLOAD);

        // Feed the LAST frame
        parser.onData(frames[1]);

        QCOMPARE(spy.count(), 2);
        auto lastHeader = spy[1][0].value<oaa::FrameHeader>();
        QCOMPARE(lastHeader.frameType, oaa::FrameType::Last);
        QCOMPARE(spy[1][1].toByteArray().size(), 1); // 16385 - 16384 = 1
    }
};

QTEST_MAIN(TestFrameParser)
#include "test_frame_parser.moc"
