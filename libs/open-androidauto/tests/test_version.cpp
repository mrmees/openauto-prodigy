#include <QtTest/QtTest>
#include <oaa/Version.hpp>

class TestVersion : public QObject {
    Q_OBJECT
private slots:
    void testProtocolVersion() {
        QCOMPARE(oaa::PROTOCOL_MAJOR, uint16_t(1));
        QCOMPARE(oaa::PROTOCOL_MINOR, uint16_t(7));
    }
    void testFrameConstants() {
        QCOMPARE(oaa::FRAME_MAX_PAYLOAD, uint16_t(0x4000));
        QCOMPARE(oaa::FRAME_HEADER_SIZE, uint16_t(2));
        QCOMPARE(oaa::FRAME_SIZE_SHORT, uint16_t(2));
        QCOMPARE(oaa::FRAME_SIZE_EXTENDED, uint16_t(6));
    }
    void testTlsConstants() {
        QCOMPARE(oaa::TLS_OVERHEAD, 29);
        QCOMPARE(oaa::BIO_BUFFER_SIZE, 20480);
    }
};

QTEST_MAIN(TestVersion)
#include "test_version.moc"
