#include <QtTest/QtTest>
#include <QSignalSpy>
#include <oaa/Messenger/FrameAssembler.hpp>

class TestFrameAssembler : public QObject {
    Q_OBJECT

private:
    static oaa::FrameHeader header(
        uint8_t channelId, oaa::FrameType frameType,
        oaa::EncryptionType encryption = oaa::EncryptionType::Plain,
        oaa::MessageType message = oaa::MessageType::Specific,
        uint32_t total = 0)
    {
        return {channelId, frameType, encryption, message, total};
    }

private slots:
    void testBulkFrame() {
        oaa::FrameAssembler assembler;
        QSignalSpy messageSpy(&assembler, &oaa::FrameAssembler::messageAssembled);
        QSignalSpy failureSpy(&assembler, &oaa::FrameAssembler::assemblyFailed);

        assembler.onFrame(header(5, oaa::FrameType::Bulk),
                          QByteArrayLiteral("hello world"));

        QCOMPARE(messageSpy.count(), 1);
        QCOMPARE(failureSpy.count(), 0);
        QCOMPARE(messageSpy[0][0].value<uint8_t>(), uint8_t(5));
        QCOMPARE(messageSpy[0][2].toByteArray(), QByteArray("hello world"));
    }

    void testExactFirstMiddleLast() {
        oaa::FrameAssembler assembler;
        QSignalSpy messageSpy(&assembler, &oaa::FrameAssembler::messageAssembled);
        QSignalSpy failureSpy(&assembler, &oaa::FrameAssembler::assemblyFailed);

        assembler.onFrame(header(2, oaa::FrameType::First,
                                 oaa::EncryptionType::Encrypted,
                                 oaa::MessageType::Control, 6),
                          QByteArrayLiteral("AA"));
        assembler.onFrame(header(2, oaa::FrameType::Middle,
                                 oaa::EncryptionType::Encrypted,
                                 oaa::MessageType::Control),
                          QByteArrayLiteral("BB"));
        assembler.onFrame(header(2, oaa::FrameType::Last,
                                 oaa::EncryptionType::Encrypted,
                                 oaa::MessageType::Control),
                          QByteArrayLiteral("CC"));

        QCOMPARE(failureSpy.count(), 0);
        QCOMPARE(messageSpy.count(), 1);
        QCOMPARE(messageSpy[0][0].value<uint8_t>(), uint8_t(2));
        QCOMPARE(messageSpy[0][1].value<oaa::MessageType>(),
                 oaa::MessageType::Control);
        QCOMPARE(messageSpy[0][2].toByteArray(), QByteArray("AABBCC"));
    }

    void testInterleavedChannelsAndBulk() {
        oaa::FrameAssembler assembler(16, 24);
        QSignalSpy messageSpy(&assembler, &oaa::FrameAssembler::messageAssembled);
        QSignalSpy failureSpy(&assembler, &oaa::FrameAssembler::assemblyFailed);

        assembler.onFrame(header(3, oaa::FrameType::First,
                                 oaa::EncryptionType::Plain,
                                 oaa::MessageType::Specific, 8),
                          QByteArrayLiteral("3A"));
        assembler.onFrame(header(4, oaa::FrameType::First,
                                 oaa::EncryptionType::Plain,
                                 oaa::MessageType::Control, 8),
                          QByteArrayLiteral("4A"));
        assembler.onFrame(header(5, oaa::FrameType::Bulk),
                          QByteArrayLiteral("5X"));
        assembler.onFrame(header(4, oaa::FrameType::Last,
                                 oaa::EncryptionType::Plain,
                                 oaa::MessageType::Control),
                          QByteArrayLiteral("456789"));
        assembler.onFrame(header(3, oaa::FrameType::Last),
                          QByteArrayLiteral("345678"));

        QCOMPARE(failureSpy.count(), 0);
        QCOMPARE(messageSpy.count(), 3);
        QCOMPARE(messageSpy[0][0].value<uint8_t>(), uint8_t(5));
        QCOMPARE(messageSpy[1][0].value<uint8_t>(), uint8_t(4));
        QCOMPARE(messageSpy[1][1].value<oaa::MessageType>(),
                 oaa::MessageType::Control);
        QCOMPARE(messageSpy[2][0].value<uint8_t>(), uint8_t(3));
    }

    void testInvalidFirstDeclarations_data() {
        QTest::addColumn<uint32_t>("declared");
        QTest::addColumn<QByteArray>("payload");

        QTest::newRow("zero") << uint32_t(0) << QByteArray("AA");
        QTest::newRow("one-byte-message") << uint32_t(1) << QByteArray();
        QTest::newRow("equal-to-first") << uint32_t(2) << QByteArray("AA");
        QTest::newRow("smaller-than-first") << uint32_t(1) << QByteArray("AA");
        QTest::newRow("over-per-message-limit") << uint32_t(17) << QByteArray("AA");
    }

    void testInvalidFirstDeclarations() {
        QFETCH(uint32_t, declared);
        QFETCH(QByteArray, payload);
        oaa::FrameAssembler assembler(16, 24);
        QSignalSpy messageSpy(&assembler, &oaa::FrameAssembler::messageAssembled);
        QSignalSpy failureSpy(&assembler, &oaa::FrameAssembler::assemblyFailed);

        assembler.onFrame(header(1, oaa::FrameType::First,
                                 oaa::EncryptionType::Plain,
                                 oaa::MessageType::Specific, declared), payload);

        QCOMPARE(failureSpy.count(), 1);
        QVERIFY(!failureSpy[0][0].toString().isEmpty());
        QCOMPARE(messageSpy.count(), 0);
    }

    void testAggregateBudgetFailureReleasesAllReservations() {
        oaa::FrameAssembler assembler(16, 20);
        QSignalSpy messageSpy(&assembler, &oaa::FrameAssembler::messageAssembled);
        QSignalSpy failureSpy(&assembler, &oaa::FrameAssembler::assemblyFailed);

        assembler.onFrame(header(1, oaa::FrameType::First,
                                 oaa::EncryptionType::Plain,
                                 oaa::MessageType::Specific, 12),
                          QByteArrayLiteral("AAAA"));
        assembler.onFrame(header(2, oaa::FrameType::First,
                                 oaa::EncryptionType::Plain,
                                 oaa::MessageType::Specific, 9),
                          QByteArrayLiteral("BBBB"));
        QCOMPARE(failureSpy.count(), 1);
        QCOMPARE(messageSpy.count(), 0);

        // The failed reservation clears channel 1 as well, so a fresh message
        // can consume the full budget and complete normally.
        assembler.onFrame(header(3, oaa::FrameType::First,
                                 oaa::EncryptionType::Plain,
                                 oaa::MessageType::Specific, 12),
                          QByteArrayLiteral("CCCC"));
        assembler.onFrame(header(3, oaa::FrameType::Last),
                          QByteArrayLiteral("DDDDDDDD"));
        QCOMPARE(failureSpy.count(), 1);
        QCOMPARE(messageSpy.count(), 1);
        QCOMPARE(messageSpy[0][2].toByteArray(), QByteArray("CCCCDDDDDDDD"));
    }

    void testContinuationFailures_data() {
        QTest::addColumn<oaa::FrameHeader>("first");
        QTest::addColumn<QByteArray>("firstPayload");
        QTest::addColumn<oaa::FrameHeader>("next");
        QTest::addColumn<QByteArray>("nextPayload");

        QTest::newRow("middle-without-first")
            << header(7, oaa::FrameType::Bulk) << QByteArray()
            << header(7, oaa::FrameType::Middle) << QByteArray("orphan");
        QTest::newRow("last-without-first")
            << header(7, oaa::FrameType::Bulk) << QByteArray()
            << header(7, oaa::FrameType::Last) << QByteArray("orphan");
        QTest::newRow("encryption-mismatch")
            << header(7, oaa::FrameType::First, oaa::EncryptionType::Plain,
                      oaa::MessageType::Specific, 8) << QByteArray("AAAA")
            << header(7, oaa::FrameType::Middle, oaa::EncryptionType::Encrypted)
            << QByteArray("B");
        QTest::newRow("message-type-mismatch")
            << header(7, oaa::FrameType::First, oaa::EncryptionType::Plain,
                      oaa::MessageType::Specific, 8) << QByteArray("AAAA")
            << header(7, oaa::FrameType::Middle, oaa::EncryptionType::Plain,
                      oaa::MessageType::Control) << QByteArray("B");
        QTest::newRow("middle-reaches-total")
            << header(7, oaa::FrameType::First, oaa::EncryptionType::Plain,
                      oaa::MessageType::Specific, 6) << QByteArray("AAAA")
            << header(7, oaa::FrameType::Middle) << QByteArray("BB");
        QTest::newRow("last-underruns")
            << header(7, oaa::FrameType::First, oaa::EncryptionType::Plain,
                      oaa::MessageType::Specific, 8) << QByteArray("AAAA")
            << header(7, oaa::FrameType::Last) << QByteArray("BB");
        QTest::newRow("last-overruns")
            << header(7, oaa::FrameType::First, oaa::EncryptionType::Plain,
                      oaa::MessageType::Specific, 5) << QByteArray("AAAA")
            << header(7, oaa::FrameType::Last) << QByteArray("BB");
    }

    void testContinuationFailures() {
        QFETCH(oaa::FrameHeader, first);
        QFETCH(QByteArray, firstPayload);
        QFETCH(oaa::FrameHeader, next);
        QFETCH(QByteArray, nextPayload);
        oaa::FrameAssembler assembler(16, 24);
        QSignalSpy messageSpy(&assembler, &oaa::FrameAssembler::messageAssembled);
        QSignalSpy failureSpy(&assembler, &oaa::FrameAssembler::assemblyFailed);

        if (first.frameType == oaa::FrameType::First)
            assembler.onFrame(first, firstPayload);
        assembler.onFrame(next, nextPayload);

        QCOMPARE(failureSpy.count(), 1);
        QCOMPARE(messageSpy.count(), 0);
    }

    void testDuplicateFirstReleasesOldReservation() {
        oaa::FrameAssembler assembler(16, 12);
        QSignalSpy messageSpy(&assembler, &oaa::FrameAssembler::messageAssembled);
        QSignalSpy failureSpy(&assembler, &oaa::FrameAssembler::assemblyFailed);

        assembler.onFrame(header(8, oaa::FrameType::First,
                                 oaa::EncryptionType::Plain,
                                 oaa::MessageType::Specific, 12),
                          QByteArrayLiteral("old"));
        assembler.onFrame(header(8, oaa::FrameType::First,
                                 oaa::EncryptionType::Plain,
                                 oaa::MessageType::Control, 6),
                          QByteArrayLiteral("new"));
        assembler.onFrame(header(8, oaa::FrameType::Last,
                                 oaa::EncryptionType::Plain,
                                 oaa::MessageType::Control),
                          QByteArrayLiteral("end"));

        QCOMPARE(failureSpy.count(), 0);
        QCOMPARE(messageSpy.count(), 1);
        QCOMPARE(messageSpy[0][1].value<oaa::MessageType>(),
                 oaa::MessageType::Control);
        QCOMPARE(messageSpy[0][2].toByteArray(), QByteArray("newend"));
    }

    void testResetReleasesReservationsAndPayload() {
        oaa::FrameAssembler assembler(16, 12);
        QSignalSpy messageSpy(&assembler, &oaa::FrameAssembler::messageAssembled);
        QSignalSpy failureSpy(&assembler, &oaa::FrameAssembler::assemblyFailed);

        assembler.onFrame(header(1, oaa::FrameType::First,
                                 oaa::EncryptionType::Plain,
                                 oaa::MessageType::Specific, 12),
                          QByteArrayLiteral("old"));
        assembler.reset();
        assembler.onFrame(header(2, oaa::FrameType::First,
                                 oaa::EncryptionType::Plain,
                                 oaa::MessageType::Specific, 12),
                          QByteArrayLiteral("new"));
        assembler.onFrame(header(2, oaa::FrameType::Last),
                          QByteArrayLiteral("123456789"));

        QCOMPARE(failureSpy.count(), 0);
        QCOMPARE(messageSpy.count(), 1);
        QCOMPARE(messageSpy[0][2].toByteArray(), QByteArray("new123456789"));
    }
};

QTEST_MAIN(TestFrameAssembler)
#include "test_frame_assembler.moc"
