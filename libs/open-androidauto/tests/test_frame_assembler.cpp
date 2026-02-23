#include <QtTest/QtTest>
#include <QSignalSpy>
#include <oaa/Messenger/FrameAssembler.hpp>

class TestFrameAssembler : public QObject {
    Q_OBJECT

private slots:
    void testBulkFrame() {
        oaa::FrameAssembler assembler;
        QSignalSpy spy(&assembler, &oaa::FrameAssembler::messageAssembled);

        QByteArray payload("hello world");
        oaa::FrameHeader header{5, oaa::FrameType::Bulk,
                                oaa::EncryptionType::Plain,
                                oaa::MessageType::Specific};
        assembler.onFrame(header, payload);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].value<uint8_t>(), uint8_t(5));
        QCOMPARE(spy[0][1].value<oaa::MessageType>(), oaa::MessageType::Specific);
        QCOMPARE(spy[0][2].toByteArray(), payload);
    }

    void testFirstLast() {
        oaa::FrameAssembler assembler;
        QSignalSpy spy(&assembler, &oaa::FrameAssembler::messageAssembled);

        QByteArray part1("AAAA");
        QByteArray part2("BBBB");

        oaa::FrameHeader first{1, oaa::FrameType::First,
                               oaa::EncryptionType::Encrypted,
                               oaa::MessageType::Control};
        oaa::FrameHeader last{1, oaa::FrameType::Last,
                              oaa::EncryptionType::Encrypted,
                              oaa::MessageType::Control};

        assembler.onFrame(first, part1);
        QCOMPARE(spy.count(), 0);

        assembler.onFrame(last, part2);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].value<uint8_t>(), uint8_t(1));
        QCOMPARE(spy[0][1].value<oaa::MessageType>(), oaa::MessageType::Control);
        QCOMPARE(spy[0][2].toByteArray(), QByteArray("AAAABBBB"));
    }

    void testFirstMiddleLast() {
        oaa::FrameAssembler assembler;
        QSignalSpy spy(&assembler, &oaa::FrameAssembler::messageAssembled);

        oaa::FrameHeader first{2, oaa::FrameType::First,
                               oaa::EncryptionType::Plain,
                               oaa::MessageType::Specific};
        oaa::FrameHeader middle{2, oaa::FrameType::Middle,
                                oaa::EncryptionType::Plain,
                                oaa::MessageType::Specific};
        oaa::FrameHeader last{2, oaa::FrameType::Last,
                              oaa::EncryptionType::Plain,
                              oaa::MessageType::Specific};

        assembler.onFrame(first, QByteArray("AA"));
        assembler.onFrame(middle, QByteArray("BB"));
        assembler.onFrame(last, QByteArray("CC"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].value<uint8_t>(), uint8_t(2));
        QCOMPARE(spy[0][2].toByteArray(), QByteArray("AABBCC"));
    }

    void testInterleavedChannels() {
        oaa::FrameAssembler assembler;
        QSignalSpy spy(&assembler, &oaa::FrameAssembler::messageAssembled);

        // ch3 FIRST
        oaa::FrameHeader ch3first{3, oaa::FrameType::First,
                                  oaa::EncryptionType::Plain,
                                  oaa::MessageType::Specific};
        assembler.onFrame(ch3first, QByteArray("3A"));

        // ch4 BULK — should emit immediately
        oaa::FrameHeader ch4bulk{4, oaa::FrameType::Bulk,
                                 oaa::EncryptionType::Plain,
                                 oaa::MessageType::Control};
        assembler.onFrame(ch4bulk, QByteArray("4X"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].value<uint8_t>(), uint8_t(4));
        QCOMPARE(spy[0][2].toByteArray(), QByteArray("4X"));

        // ch3 LAST
        oaa::FrameHeader ch3last{3, oaa::FrameType::Last,
                                 oaa::EncryptionType::Plain,
                                 oaa::MessageType::Specific};
        assembler.onFrame(ch3last, QByteArray("3B"));

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy[1][0].value<uint8_t>(), uint8_t(3));
        QCOMPARE(spy[1][2].toByteArray(), QByteArray("3A3B"));
    }

    void testMiddleWithoutFirst() {
        oaa::FrameAssembler assembler;
        QSignalSpy spy(&assembler, &oaa::FrameAssembler::messageAssembled);

        oaa::FrameHeader middle{7, oaa::FrameType::Middle,
                                oaa::EncryptionType::Plain,
                                oaa::MessageType::Specific};
        assembler.onFrame(middle, QByteArray("orphan"));

        QCOMPARE(spy.count(), 0);
    }

    void testDuplicateFirst() {
        oaa::FrameAssembler assembler;
        QSignalSpy spy(&assembler, &oaa::FrameAssembler::messageAssembled);

        oaa::FrameHeader first1{8, oaa::FrameType::First,
                                oaa::EncryptionType::Plain,
                                oaa::MessageType::Specific};
        assembler.onFrame(first1, QByteArray("old"));

        // Second FIRST on same channel — should discard old partial
        oaa::FrameHeader first2{8, oaa::FrameType::First,
                                oaa::EncryptionType::Plain,
                                oaa::MessageType::Control};
        assembler.onFrame(first2, QByteArray("new"));

        QCOMPARE(spy.count(), 0); // Nothing emitted yet

        // Complete the second message
        oaa::FrameHeader last{8, oaa::FrameType::Last,
                              oaa::EncryptionType::Plain,
                              oaa::MessageType::Control};
        assembler.onFrame(last, QByteArray("end"));

        QCOMPARE(spy.count(), 1);
        // Should contain the second FIRST's data, not the first
        QCOMPARE(spy[0][1].value<oaa::MessageType>(), oaa::MessageType::Control);
        QCOMPARE(spy[0][2].toByteArray(), QByteArray("newend"));
    }
};

QTEST_MAIN(TestFrameAssembler)
#include "test_frame_assembler.moc"
