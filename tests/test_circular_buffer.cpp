#include <oaa/Messenger/CircularBuffer.hpp>
#include <QtTest>

class TestCircularBuffer : public QObject {
    Q_OBJECT

private slots:
    void testBasicWriteRead()
    {
        oaa::CircularBuffer buf(64);
        const char data[] = "Hello, World!";
        buf.append(data, 13);

        QCOMPARE(buf.available(), size_t(13));
        QByteArray result = buf.peek(13);
        QCOMPARE(result, QByteArray("Hello, World!"));
    }

    void testConsumeAdvancesReadCursor()
    {
        oaa::CircularBuffer buf(64);
        buf.append("ABCDEFGH", 8);

        QCOMPARE(buf.available(), size_t(8));
        buf.consume(3);
        QCOMPARE(buf.available(), size_t(5));

        QByteArray result = buf.peek(5);
        QCOMPARE(result, QByteArray("DEFGH"));
    }

    void testPeekDoesNotConsume()
    {
        oaa::CircularBuffer buf(64);
        buf.append("TestData", 8);

        QByteArray first = buf.peek(8);
        QCOMPARE(buf.available(), size_t(8));

        QByteArray second = buf.peek(4);
        QCOMPARE(buf.available(), size_t(8));
        QCOMPARE(first, QByteArray("TestData"));
        QCOMPARE(second, QByteArray("Test"));
    }

    void testWrapAround()
    {
        // Small buffer to force wrapping
        oaa::CircularBuffer buf(16);

        // Fill most of the buffer
        buf.append("AAAAAAAAAAAA", 12); // 12 bytes
        QCOMPARE(buf.available(), size_t(12));

        // Consume 10 to move read cursor near the end
        buf.consume(10);
        QCOMPARE(buf.available(), size_t(2));

        // Now write 10 more bytes — this should wrap around
        buf.append("BBBBBBBBBB", 10);
        QCOMPARE(buf.available(), size_t(12));

        // Read should get the remaining AA then BBBBBBBBBB
        QByteArray result = buf.peek(12);
        QCOMPARE(result, QByteArray("AABBBBBBBBBB"));
    }

    void testAutoGrow()
    {
        oaa::CircularBuffer buf(8); // tiny initial capacity

        // Write more than capacity
        const char data[] = "0123456789ABCDEF"; // 16 bytes
        buf.append(data, 16);

        QCOMPARE(buf.available(), size_t(16));
        QByteArray result = buf.peek(16);
        QCOMPARE(result, QByteArray("0123456789ABCDEF"));
    }

    void testAutoGrowWithExistingWrappedData()
    {
        oaa::CircularBuffer buf(8);

        // Create a wrapped state: write 6, consume 4, write 6 more
        buf.append("AABBCC", 6);
        buf.consume(4);
        buf.append("DDEEFF", 6);
        // Now we have 8 bytes (CC DDEEFF) wrapped in an 8-byte buffer

        QCOMPARE(buf.available(), size_t(8));

        // Writing more should trigger grow and linearize
        buf.append("GGHH", 4);
        QCOMPARE(buf.available(), size_t(12));

        QByteArray result = buf.peek(12);
        QCOMPARE(result, QByteArray("CCDDEEFFGGHH"));
    }

    void testReadPtr()
    {
        oaa::CircularBuffer buf(64);
        buf.append("Hello", 5);

        size_t contigLen = 0;
        const char* ptr = buf.readPtr(contigLen);

        QVERIFY(ptr != nullptr);
        QCOMPARE(contigLen, size_t(5));
        QCOMPARE(QByteArray(ptr, static_cast<int>(contigLen)), QByteArray("Hello"));
    }

    void testReadPtrWrapped()
    {
        oaa::CircularBuffer buf(8);

        // Create wrapped state: write 6, consume 4 (not all — so positions don't reset)
        buf.append("XXXXXZ", 6);
        buf.consume(4); // readPos=4, writePos=6, size=2
        // Now write 6 more — wraps: positions 6,7,0,1,2,3
        buf.append("ABCDEF", 6); // total size=8, readPos=4, writePos=4

        size_t contigLen = 0;
        const char* ptr = buf.readPtr(contigLen);

        QVERIFY(ptr != nullptr);
        // Contiguous from readPos=4 to end of buffer = 4 bytes ("XZAB" at positions 4,5,6,7)
        QCOMPARE(contigLen, size_t(4));
        QCOMPARE(QByteArray(ptr, static_cast<int>(contigLen)), QByteArray("XZAB"));

        // Full peek should return all 8 bytes linearized
        QByteArray full = buf.peek(8);
        QCOMPARE(full, QByteArray("XZABCDEF"));
    }

    void testEmptyBuffer()
    {
        oaa::CircularBuffer buf(64);
        QCOMPARE(buf.available(), size_t(0));

        size_t contigLen = 0;
        const char* ptr = buf.readPtr(contigLen);
        QCOMPARE(ptr, nullptr);
        QCOMPARE(contigLen, size_t(0));
    }

    void testConsumeAll()
    {
        oaa::CircularBuffer buf(64);
        buf.append("Data", 4);
        buf.consume(4);
        QCOMPARE(buf.available(), size_t(0));

        // After consuming everything, write again should work from start
        buf.append("New", 3);
        QCOMPARE(buf.available(), size_t(3));
        QCOMPARE(buf.peek(3), QByteArray("New"));
    }

    void testMultipleAppends()
    {
        oaa::CircularBuffer buf(64);
        buf.append("Hello", 5);
        buf.append(", ", 2);
        buf.append("World!", 6);

        QCOMPARE(buf.available(), size_t(13));
        QCOMPARE(buf.peek(13), QByteArray("Hello, World!"));
    }
};

QTEST_MAIN(TestCircularBuffer)
#include "test_circular_buffer.moc"
