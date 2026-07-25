#include <QTest>
#include <QtEndian>

#include "core/aa/AVInputCaptureBridge.hpp"

#include <array>
#include <limits>

namespace {

QByteArray pcm(std::initializer_list<qint16> samples)
{
    QByteArray bytes(static_cast<qsizetype>(samples.size() * 2), '\0');
    qsizetype offset = 0;
    for (const qint16 sample : samples) {
        qToLittleEndian<qint16>(sample,
            reinterpret_cast<uchar*>(bytes.data()) + offset);
        offset += 2;
    }
    return bytes;
}

qint16 sampleAt(const QByteArray& bytes, qsizetype sample)
{
    return qFromLittleEndian<qint16>(
        reinterpret_cast<const uchar*>(bytes.constData()) + sample * 2);
}

QByteArray fullFrame(qint16 sample)
{
    QByteArray bytes(oap::aa::AVInputCaptureBridge::FrameBytes, '\0');
    for (qsizetype offset = 0; offset < bytes.size(); offset += 2) {
        qToLittleEndian<qint16>(sample,
            reinterpret_cast<uchar*>(bytes.data()) + offset);
    }
    return bytes;
}

} // namespace

class TestAVInputCaptureBridge : public QObject {
    Q_OBJECT
private slots:
    void testGainNormalization() {
        QCOMPARE(oap::aa::AVInputCaptureBridge::normalizedGain(0.1), 0.5);
        QCOMPARE(oap::aa::AVInputCaptureBridge::normalizedGain(2.25), 2.25);
        QCOMPARE(oap::aa::AVInputCaptureBridge::normalizedGain(8.0), 4.0);
        QCOMPARE(oap::aa::AVInputCaptureBridge::normalizedGain(
                     std::numeric_limits<double>::infinity()), 1.0);
        QCOMPARE(oap::aa::AVInputCaptureBridge::normalizedGain(
                     std::numeric_limits<double>::quiet_NaN()), 1.0);
    }

    void testGainIsSignedAndSaturating() {
        QByteArray bytes = pcm({1000, -1000, 20000, -20000, 0});
        oap::aa::AVInputCaptureBridge::applyGainS16Le(bytes, 2.0);
        QCOMPARE(sampleAt(bytes, 0), qint16(2000));
        QCOMPARE(sampleAt(bytes, 1), qint16(-2000));
        QCOMPARE(sampleAt(bytes, 2), std::numeric_limits<qint16>::max());
        QCOMPARE(sampleAt(bytes, 3), std::numeric_limits<qint16>::min());
        QCOMPARE(sampleAt(bytes, 4), qint16(0));

        bytes = pcm({1001, -1001});
        oap::aa::AVInputCaptureBridge::applyGainS16Le(bytes, 0.5);
        QCOMPARE(sampleAt(bytes, 0), qint16(501));
        QCOMPARE(sampleAt(bytes, 1), qint16(-501));
    }

    void testCompleteFramesDrainOnQtThread() {
        oap::aa::AVInputCaptureBridge bridge;
        QList<QByteArray> frames;
        QList<uint64_t> timestamps;
        const uint64_t generation = bridge.start(2.0,
            [&frames, &timestamps](const QByteArray& frame, uint64_t timestamp) {
                frames.append(frame);
                timestamps.append(timestamp);
                return true;
            });
        QVERIFY(generation != 0);

        const QByteArray source = fullFrame(1000);
        bridge.pushPcm(generation,
                       reinterpret_cast<const uint8_t*>(source.constData()),
                       source.size() - 2);
        QTest::qWait(oap::aa::AVInputCaptureBridge::DrainIntervalMs * 2);
        QCOMPARE(frames.size(), 0);

        bridge.pushPcm(generation,
                       reinterpret_cast<const uint8_t*>(source.constData()), 2);
        QTRY_COMPARE(frames.size(), 1);
        QCOMPARE(frames.front().size(),
                 static_cast<qsizetype>(oap::aa::AVInputCaptureBridge::FrameBytes));
        QCOMPARE(sampleAt(frames.front(), 0), qint16(2000));
        QVERIFY(timestamps.front() > 0);
    }

    void testNormalBatchedFramesDrainInOrder() {
        oap::aa::AVInputCaptureBridge bridge;
        QList<qint16> sentSamples;
        const uint64_t generation = bridge.start(1.0,
            [&sentSamples](const QByteArray& frame, uint64_t) {
                sentSamples.append(sampleAt(frame, 0));
                return true;
            });

        QByteArray batch = fullFrame(100);
        batch.append(fullFrame(200));
        bridge.pushPcm(generation,
                       reinterpret_cast<const uint8_t*>(batch.constData()),
                       batch.size());

        QTRY_COMPARE(sentSamples.size(), 2);
        QCOMPARE(sentSamples, QList<qint16>({100, 200}));
        QCOMPARE(bridge.droppedFrames(), uint64_t(0));
    }

    void testWindowRefusalPurgesUntilPermitReturns() {
        oap::aa::AVInputCaptureBridge bridge;
        QList<qint16> sentSamples;
        bool permit = false;
        const uint64_t generation = bridge.start(1.0,
            [&sentSamples, &permit](const QByteArray& frame, uint64_t) {
                sentSamples.append(sampleAt(frame, 0));
                return permit;
            });

        QByteArray frame = fullFrame(100);
        bridge.pushPcm(generation,
                       reinterpret_cast<const uint8_t*>(frame.constData()), frame.size());
        QTRY_COMPARE(sentSamples.size(), 1);

        frame = fullFrame(200);
        bridge.pushPcm(generation,
                       reinterpret_cast<const uint8_t*>(frame.constData()), frame.size());
        QTest::qWait(oap::aa::AVInputCaptureBridge::DrainIntervalMs * 2);
        QCOMPARE(sentSamples.size(), 1);

        permit = true;
        bridge.notifyWindowAvailable();
        frame = fullFrame(300);
        bridge.pushPcm(generation,
                       reinterpret_cast<const uint8_t*>(frame.constData()), frame.size());
        QTRY_COMPARE(sentSamples.size(), 2);
        QCOMPARE(sentSamples.back(), qint16(300));
    }

    void testAckStallOverflowRemainsObservable() {
        oap::aa::AVInputCaptureBridge bridge;
        const uint64_t generation = bridge.start(1.0,
            [](const QByteArray&, uint64_t) { return false; });

        QByteArray frame = fullFrame(100);
        bridge.pushPcm(generation,
                       reinterpret_cast<const uint8_t*>(frame.constData()), frame.size());
        QTRY_VERIFY(bridge.droppedFrames() > 0);
        const uint64_t beforeOverflow = bridge.droppedFrames();

        const QByteArray stale(oap::aa::AVInputCaptureBridge::RingCapacity, '\x11');
        bridge.pushPcm(generation,
                       reinterpret_cast<const uint8_t*>(stale.constData()), stale.size());
        const std::array<uint8_t, 2> overflow{{0x22, 0x22}};
        bridge.pushPcm(generation, overflow.data(), overflow.size());
        bridge.notifyWindowAvailable();

        QVERIFY(bridge.droppedFrames() > beforeOverflow);
    }

    void testOverflowPurgesStaleAudio() {
        oap::aa::AVInputCaptureBridge bridge;
        QList<qint16> sentSamples;
        const uint64_t generation = bridge.start(1.0,
            [&sentSamples](const QByteArray& frame, uint64_t) {
                sentSamples.append(sampleAt(frame, 0));
                return true;
            });

        const QByteArray stale(oap::aa::AVInputCaptureBridge::RingCapacity, '\x11');
        bridge.pushPcm(generation,
                       reinterpret_cast<const uint8_t*>(stale.constData()), stale.size());
        const std::array<uint8_t, 2> overflow{{0x22, 0x22}};
        bridge.pushPcm(generation, overflow.data(), overflow.size());
        QTest::qWait(oap::aa::AVInputCaptureBridge::DrainIntervalMs * 2);
        QCOMPARE(sentSamples.size(), 0);

        const QByteArray fresh = fullFrame(400);
        bridge.pushPcm(generation,
                       reinterpret_cast<const uint8_t*>(fresh.constData()), fresh.size());
        QTRY_COMPARE(sentSamples.size(), 1);
        QCOMPARE(sentSamples.front(), qint16(400));
        QVERIFY(bridge.droppedFrames() > 0);
    }

    void testStopAndGenerationRejectStaleProducer() {
        oap::aa::AVInputCaptureBridge bridge;
        QList<qint16> sentSamples;
        const auto sender = [&sentSamples](const QByteArray& frame, uint64_t) {
            sentSamples.append(sampleAt(frame, 0));
            return true;
        };
        const uint64_t first = bridge.start(1.0, sender);
        bridge.stop();

        QByteArray frame = fullFrame(100);
        bridge.pushPcm(first,
                       reinterpret_cast<const uint8_t*>(frame.constData()), frame.size());
        QTest::qWait(oap::aa::AVInputCaptureBridge::DrainIntervalMs * 2);
        QCOMPARE(sentSamples.size(), 0);

        const uint64_t second = bridge.start(1.0, sender);
        QVERIFY(second != first);
        bridge.pushPcm(first,
                       reinterpret_cast<const uint8_t*>(frame.constData()), frame.size());
        frame = fullFrame(200);
        bridge.pushPcm(second,
                       reinterpret_cast<const uint8_t*>(frame.constData()), frame.size());
        QTRY_COMPARE(sentSamples.size(), 1);
        QCOMPARE(sentSamples.front(), qint16(200));
    }

    void testEmptySenderFailsClosed() {
        oap::aa::AVInputCaptureBridge bridge;
        QList<QByteArray> frames;
        const uint64_t generation = bridge.start(1.0,
            [&frames](const QByteArray& frame, uint64_t) {
                frames.append(frame);
                return true;
            });
        QVERIFY(generation != 0);
        QVERIFY(bridge.isActive());

        QCOMPARE(bridge.start(1.0, {}), uint64_t(0));
        QVERIFY(!bridge.isActive());

        const QByteArray frame = fullFrame(100);
        bridge.pushPcm(generation,
                       reinterpret_cast<const uint8_t*>(frame.constData()),
                       frame.size());
        QTest::qWait(oap::aa::AVInputCaptureBridge::DrainIntervalMs * 2);
        QCOMPARE(frames.size(), 0);
    }
};

QTEST_MAIN(TestAVInputCaptureBridge)
#include "test_avinput_capture_bridge.moc"
