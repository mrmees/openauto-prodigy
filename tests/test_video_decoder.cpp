#include <QSignalSpy>
#include <QTest>
#include <QMutex>
#include <QStringList>

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

namespace oap::aa {

class VideoDecoderTestAccess {
public:
    static void failNextCodecInitialization()
    {
        VideoDecoder::failCodecInitForTest_.store(true);
    }

};

} // namespace oap::aa

namespace {

QMutex capturedMessageMutex;
QStringList capturedMessages;

void captureMessage(QtMsgType, const QMessageLogContext&, const QString& message)
{
    QMutexLocker locker(&capturedMessageMutex);
    capturedMessages.append(message);
}

class ScopedMessageCapture {
public:
    ScopedMessageCapture()
        : previous_(qInstallMessageHandler(captureMessage))
    {
        QMutexLocker locker(&capturedMessageMutex);
        capturedMessages.clear();
    }

    ~ScopedMessageCapture()
    {
        qInstallMessageHandler(previous_);
    }

    QString joined() const
    {
        QMutexLocker locker(&capturedMessageMutex);
        return capturedMessages.join('\n');
    }

private:
    QtMessageHandler previous_ = nullptr;
};

} // namespace

class TestVideoDecoder : public QObject {
    Q_OBJECT

private slots:
    void constructionFailureDoesNotStartWorker()
    {
        oap::aa::VideoDecoderTestAccess::failNextCodecInitialization();
        oap::aa::VideoDecoder decoder;

        QVERIFY(!decoder.isOperational());
        QCOMPARE(decoder.beginStream(), 0ULL);
    }

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

    void endBoundaryPurgesAndRejectsFramesUntilNextBegin()
    {
        oap::aa::VideoDecoder decoder;
        QSignalSpy endedSpy(&decoder, &oap::aa::VideoDecoder::streamEnded);
        QSignalSpy codecSpy(&decoder, &oap::aa::VideoDecoder::streamCodecDetected);

        QCOMPARE(decoder.beginStream(), 1ULL);
        decoder.decodeFrame(std::make_shared<const QByteArray>(annexBNal('\x67')));
        QTRY_COMPARE(codecSpy.count(), 1);

        decoder.endStream();
        QTRY_COMPARE(endedSpy.count(), 1);
        QCOMPARE(endedSpy[0][0].toULongLong(), 1ULL);
        QVERIFY(!decoder.takeLatestFrame().isValid());

        decoder.decodeFrame(std::make_shared<const QByteArray>(annexBNal('\x42')));
        QTest::qWait(30);
        QCOMPARE(codecSpy.count(), 1);

        QCOMPARE(decoder.beginStream(), 2ULL);
        decoder.decodeFrame(std::make_shared<const QByteArray>(annexBNal('\x42')));
        QTRY_COMPARE(codecSpy.count(), 2);
        QCOMPARE(codecSpy.last()[0].toULongLong(), 2ULL);
    }

    void resetFailureIsObservableAndLabelled()
    {
        oap::aa::VideoDecoder decoder;
        QVERIFY(decoder.isOperational());
        decoder.setDiagnosticLabel(QStringLiteral("CLUSTER[id=1,ch=12]"));
        QSignalSpy errorSpy(&decoder, &oap::aa::VideoDecoder::streamError);
        ScopedMessageCapture messages;

        oap::aa::VideoDecoderTestAccess::failNextCodecInitialization();
        const quint64 generation = decoder.beginStream();

        QTRY_COMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy[0][0].toULongLong(), generation);
        QVERIFY(!errorSpy[0][1].toString().isEmpty());
        QVERIFY(!decoder.isOperational());
        QTRY_VERIFY(messages.joined().contains(
            QStringLiteral("CLUSTER[id=1,ch=12]")));

        QTest::qWait(30);
        QCOMPARE(errorSpy.count(), 1);
    }
};

QTEST_MAIN(TestVideoDecoder)
#include "test_video_decoder.moc"
