#include <QTest>
#include "core/audio/AudioRingBuffer.hpp"

class TestAudioRingBuffer : public QObject {
    Q_OBJECT

private slots:
    void constructionSetsCapacity()
    {
        oap::AudioRingBuffer rb(4096);
        QCOMPARE(rb.capacity(), 4096u);
        QCOMPARE(rb.available(), 0u);
    }

    void writeAndRead()
    {
        oap::AudioRingBuffer rb(1024);
        uint8_t writeData[256];
        for (int i = 0; i < 256; i++) writeData[i] = static_cast<uint8_t>(i);

        QCOMPARE(rb.write(writeData, 256), 256u);
        QCOMPARE(rb.available(), 256u);

        uint8_t readData[256] = {};
        QCOMPARE(rb.read(readData, 256), 256u);
        QCOMPARE(rb.available(), 0u);

        for (int i = 0; i < 256; i++)
            QCOMPARE(readData[i], static_cast<uint8_t>(i));
    }

    void readFromEmptyReturnsZero()
    {
        oap::AudioRingBuffer rb(1024);
        uint8_t buf[64];
        QCOMPARE(rb.read(buf, 64), 0u);
    }

    void overrunDropsOldest()
    {
        oap::AudioRingBuffer rb(256);
        uint8_t data[256];
        memset(data, 0xAA, 256);
        rb.write(data, 256);

        uint8_t moreData[64];
        memset(moreData, 0xBB, 64);
        rb.write(moreData, 64);

        QVERIFY(rb.available() <= 256u);
    }

    void resetClearsBuffer()
    {
        oap::AudioRingBuffer rb(1024);
        uint8_t data[128];
        rb.write(data, 128);
        rb.reset();
        QCOMPARE(rb.available(), 0u);
    }

    void partialReadLeavesRemainder()
    {
        oap::AudioRingBuffer rb(1024);
        uint8_t data[200];
        for (int i = 0; i < 200; i++) data[i] = static_cast<uint8_t>(i);
        rb.write(data, 200);

        uint8_t partial[100];
        QCOMPARE(rb.read(partial, 100), 100u);
        QCOMPARE(rb.available(), 100u);

        uint8_t rest[100];
        QCOMPARE(rb.read(rest, 100), 100u);
        QCOMPARE(rest[0], static_cast<uint8_t>(100));
    }
};

QTEST_MAIN(TestAudioRingBuffer)
#include "test_audio_ring_buffer.moc"
