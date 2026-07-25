#include <QTest>
#include "core/audio/AudioRingBuffer.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

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

    void overrunDropsNewestAndReturnsShortWrite()
    {
        oap::AudioRingBuffer rb(256);
        uint8_t data[256];
        memset(data, 0xAA, 256);
        QCOMPARE(rb.write(data, 256), 256u);

        uint8_t moreData[64];
        memset(moreData, 0xBB, 64);
        QCOMPARE(rb.write(moreData, 64), 0u);
        QCOMPARE(rb.dropCount(), 1u);

        uint8_t out[256] = {};
        QCOMPARE(rb.read(out, sizeof(out)), 256u);
        for (uint8_t byte : out)
            QCOMPARE(byte, static_cast<uint8_t>(0xAA));
    }

    void partialOverflowWritesOnlyAvailableSpace()
    {
        oap::AudioRingBuffer rb(8);
        const uint8_t first[] = {1, 2, 3, 4, 5, 6};
        const uint8_t second[] = {7, 8, 9, 10};
        QCOMPARE(rb.write(first, sizeof(first)), 6u);
        QCOMPARE(rb.write(second, sizeof(second)), 2u);
        QCOMPARE(rb.available(), 8u);

        uint8_t out[8] = {};
        QCOMPARE(rb.read(out, sizeof(out)), 8u);
        const uint8_t expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
        QCOMPARE(std::memcmp(out, expected, sizeof(out)), 0);
    }

    void atomicOverflowDropsWholePacket()
    {
        oap::AudioRingBuffer rb(8);
        const uint8_t first[] = {1, 2, 3, 4, 5, 6};
        const uint8_t second[] = {7, 8, 9, 10};
        QCOMPARE(rb.writeAllOrDrop(first, sizeof(first)), 6u);
        QCOMPARE(rb.writeAllOrDrop(second, sizeof(second)), 0u);
        QCOMPARE(rb.dropCount(), 1u);
        QCOMPARE(rb.available(), 6u);

        uint8_t out[6] = {};
        QCOMPARE(rb.read(out, sizeof(out)), 6u);
        QCOMPARE(std::memcmp(out, first, sizeof(out)), 0);
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

    // Drain's flush contract is now FULLY quiescent (both reader and writer are
    // stopped — the BT tap gates its capture writer off before draining), so a
    // quiescent drain is a total flush: after it, no pre-drain byte is readable.
    // Fill the ring to overflow with pattern A, drain, write pattern B, read it
    // all back, and assert every byte is B — not one A survives the drain.
    void drainFullyFlushesPreDrainBytes()
    {
        constexpr uint32_t kCap = 1024;
        oap::AudioRingBuffer rb(kCap);

        // Pattern A: fill the ring and attempt one drop-newest overflow so the
        // ring remains exactly full of 0xAA.
        uint8_t a[kCap];
        memset(a, 0xAA, sizeof(a));
        rb.write(a, kCap);
        rb.write(a, kCap);            // force at least one overflow event
        QVERIFY(rb.available() > 0);

        // Quiescent drain — total flush, ring is empty afterwards.
        rb.drain();
        QCOMPARE(rb.available(), 0u);

        // Pattern B into the drained ring.
        constexpr uint32_t kB = 512;
        uint8_t b[kB];
        memset(b, 0xBB, sizeof(b));
        QCOMPARE(rb.write(b, kB), kB);
        QCOMPARE(rb.available(), kB);

        // Read all — every byte must be B; no 0xAA leaks past the drain.
        uint8_t out[kB] = {};
        QCOMPARE(rb.read(out, kB), kB);
        for (uint32_t i = 0; i < kB; ++i)
            QCOMPARE(out[i], static_cast<uint8_t>(0xBB));
    }

    // Real SPSC stress: the producer publishes monotonically increasing words
    // and accepts drop-newest short writes. The consumer may observe gaps, but
    // every accepted word must remain intact and strictly increasing — never
    // replayed, regressed, or torn — and reported fill stays bounded.
    void concurrentProducerConsumerPreservesAcceptedOrder()
    {
        constexpr uint32_t kCap = 4096;
        constexpr uint32_t kValues = 200000;
        oap::AudioRingBuffer rb(kCap);

        std::atomic<bool> producerDone{false};
        std::atomic<uint32_t> maxSeen{0};
        std::atomic<bool> invalid{false};
        std::vector<uint32_t> acceptedValues;
        std::vector<uint32_t> consumedValues;
        acceptedValues.reserve(kValues);
        consumedValues.reserve(kValues);

        std::thread producer([&]() {
            for (uint32_t value = 1; value <= kValues; ++value) {
                if (rb.write(reinterpret_cast<const uint8_t*>(&value),
                             sizeof(value)) == sizeof(value)) {
                    acceptedValues.push_back(value);
                }
            }
            producerDone.store(true, std::memory_order_release);
        });

        std::thread consumer([&]() {
            uint32_t previous = 0;
            while (!producerDone.load(std::memory_order_acquire)
                   || rb.available() != 0) {
                const uint32_t avail = rb.available();
                uint32_t oldMax = maxSeen.load(std::memory_order_relaxed);
                while (avail > oldMax && !maxSeen.compare_exchange_weak(
                           oldMax, avail, std::memory_order_relaxed)) {}

                uint32_t value = 0;
                if (rb.read(reinterpret_cast<uint8_t*>(&value), sizeof(value))
                    != sizeof(value)) {
                    std::this_thread::yield();
                    continue;
                }
                if (value <= previous || value == 0 || value > kValues)
                    invalid.store(true, std::memory_order_relaxed);
                previous = value;
                consumedValues.push_back(value);
            }
        });

        producer.join();
        consumer.join();

        QVERIFY(!invalid.load());
        QVERIFY(consumedValues == acceptedValues);
        QVERIFY(maxSeen.load() <= kCap);
        QCOMPARE(rb.available(), 0u);
    }
};

QTEST_MAIN(TestAudioRingBuffer)
#include "test_audio_ring_buffer.moc"
