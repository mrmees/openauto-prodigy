#include <QSignalSpy>
#include <QTest>

#include "core/aa/VideoDecoder.hpp"

namespace {

QByteArray annexBNal(char first, char second = '\x01')
{
    QByteArray data;
    data.append('\x00');
    data.append('\x00');
    data.append('\x00');
    data.append('\x01');
    data.append(first);
    data.append(second);
    return data;
}

} // namespace

class TestVideoDecoder : public QObject {
    Q_OBJECT

private slots:
    void workerOrdersResetBeforeNewCodecDetection()
    {
        oap::aa::VideoDecoder decoder;
        QSignalSpy resetSpy(&decoder, &oap::aa::VideoDecoder::streamResetCompleted);
        QSignalSpy codecSpy(&decoder, &oap::aa::VideoDecoder::streamCodecDetected);

        decoder.beginStream();
        decoder.decodeFrame(std::make_shared<const QByteArray>(annexBNal('\x42')));

        QTRY_COMPARE(resetSpy.count(), 1);
        QTRY_COMPARE(codecSpy.count(), 1);
        QCOMPARE(resetSpy[0][0].toULongLong(), 1ULL);
        QCOMPARE(codecSpy[0][0].toULongLong(), 1ULL);
        QCOMPARE(codecSpy[0][1].toInt(), static_cast<int>(AV_CODEC_ID_H265));

        // Queue stale H.265 work, then place a reset barrier and a new H.264
        // SPS behind it. Stale work may finish before the barrier, but no old
        // codec detection may appear in the new generation.
        for (int i = 0; i < 64; ++i)
            decoder.decodeFrame(std::make_shared<const QByteArray>(annexBNal('\x42')));
        decoder.beginStream();
        decoder.decodeFrame(std::make_shared<const QByteArray>(annexBNal('\x67')));

        QTRY_COMPARE(resetSpy.count(), 2);
        QTRY_VERIFY(codecSpy.count() >= 2);
        QCOMPARE(resetSpy[1][0].toULongLong(), 2ULL);
        QCOMPARE(codecSpy.last()[0].toULongLong(), 2ULL);
        QCOMPARE(codecSpy.last()[1].toInt(), static_cast<int>(AV_CODEC_ID_H264));

        QTest::qWait(30);
        for (const auto& emission : codecSpy) {
            if (emission[0].toULongLong() == 2ULL)
                QCOMPARE(emission[1].toInt(), static_cast<int>(AV_CODEC_ID_H264));
        }
    }

    void repeatedBoundaryPublishesOneCompletionPerGeneration()
    {
        oap::aa::VideoDecoder decoder;
        QSignalSpy resetSpy(&decoder, &oap::aa::VideoDecoder::streamResetCompleted);

        decoder.beginStream();
        QTRY_COMPARE(resetSpy.count(), 1);
        decoder.beginStream();
        QTRY_COMPARE(resetSpy.count(), 2);
        decoder.beginStream();
        QTRY_COMPARE(resetSpy.count(), 3);

        QCOMPARE(resetSpy[0][0].toULongLong(), 1ULL);
        QCOMPARE(resetSpy[1][0].toULongLong(), 2ULL);
        QCOMPARE(resetSpy[2][0].toULongLong(), 3ULL);
    }
};

QTEST_MAIN(TestVideoDecoder)
#include "test_video_decoder.moc"
