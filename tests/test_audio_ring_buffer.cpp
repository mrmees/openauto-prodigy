#include <QTest>
#include "core/audio/AudioRingBuffer.hpp"

#include <atomic>
#include <chrono>
#include <thread>

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

    // Drain's flush contract is now FULLY quiescent (both reader and writer are
    // stopped — the BT tap gates its capture writer off before draining), so a
    // quiescent drain is a total flush: after it, no pre-drain byte is readable.
    // Fill the ring to overflow with pattern A, drain, write pattern B, read it
    // all back, and assert every byte is B — not one A survives the drain.
    void drainFullyFlushesPreDrainBytes()
    {
        constexpr uint32_t kCap = 1024;
        oap::AudioRingBuffer rb(kCap);

        // Pattern A: write past capacity so the ring overflow-drops and ends up
        // exactly full of 0xAA.
        uint8_t a[kCap];
        memset(a, 0xAA, sizeof(a));
        rb.write(a, kCap);
        rb.write(a, kCap);            // force at least one overflow-drop
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

    // Bounded-fill robustness under a CONTINUOUSLY-OVERFLOWING live writer. This
    // is NOT the drain-flush-safety guarantee (that now requires a quiesced
    // writer — see drainFullyFlushesPreDrainBytes above). It instead documents
    // that write()'s own drop-oldest keeps the ring bounded under an unrelenting
    // writer, and that a concurrent drain() never yields a wrapped/near-2^32
    // garbage available() reading even though it is racing write()'s read-index
    // advance (a torn index would surface as exactly that garbage).
    //
    // The writer is deliberately much FASTER than the real threat model (a
    // PipeWire RT capture stream, 48 kHz stereo S16 ≈ 192 KB/s): ~25 MB/s keeps
    // the ring overflowing continuously so write()'s own read-index advance
    // races drain() for the whole test. It is bounded (not a GB/s hammer) only
    // to keep available()'s sampling skew within one chunk.
    // (An unbounded writer only produces measurement skew in available(), whose
    // get_read_index() loads the read then the write index non-atomically: with
    // GB/s writes the write index races ahead between those two loads, inflating
    // the *reported* fill without any actual ring defect. Pacing keeps that skew
    // negligible so the capacity invariant is a meaningful signal.)
    void boundedFillUnderLiveWriter()
    {
        constexpr uint32_t kCap = 4096;
        constexpr uint32_t kChunk = 512;
        oap::AudioRingBuffer rb(kCap);

        std::atomic<bool> stop{false};
        std::atomic<uint32_t> maxSeen{0};

        std::thread writer([&]() {
            uint8_t chunk[kChunk];
            for (uint32_t i = 0; i < kChunk; ++i)
                chunk[i] = static_cast<uint8_t>(i);
            // ~kChunk every 20 us ≈ 25 MB/s — well above RT capture rate (so the
            // ring genuinely fills and overflows, exercising write()'s own read-
            // index advance against drain()) yet bounded enough that available()
            // sampling skew stays within one chunk.
            while (!stop.load(std::memory_order_relaxed)) {
                rb.write(chunk, kChunk);
                std::this_thread::sleep_for(std::chrono::microseconds(20));
            }
        });

        // Drain in a tight loop for ~1s, tracking the peak fill the ring ever
        // reports. Bound: capacity + one write chunk (the sampling-skew ceiling
        // for a paced writer). A torn reset() would instead report a wrapped,
        // near-2^32 garbage value — this bound cleanly rejects that.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (std::chrono::steady_clock::now() < deadline) {
            uint32_t a = rb.available();
            if (a > maxSeen.load(std::memory_order_relaxed))
                maxSeen.store(a, std::memory_order_relaxed);
            rb.drain();
        }

        stop.store(true, std::memory_order_relaxed);
        writer.join();

        // No runaway / torn-ring garbage while racing the writer.
        QVERIFY2(maxSeen.load() <= kCap + kChunk,
                 qPrintable(QStringLiteral("maxSeen=%1 exceeded cap+chunk=%2")
                                .arg(maxSeen.load()).arg(kCap + kChunk)));

        // Writer is quiesced: the ring must be bounded and a final drain empties
        // it completely.
        QVERIFY(rb.available() <= kCap);
        rb.drain();
        QCOMPARE(rb.available(), 0u);
    }
};

QTEST_MAIN(TestAudioRingBuffer)
#include "test_audio_ring_buffer.moc"
