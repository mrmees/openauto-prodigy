#include <QSignalSpy>
#include <QMutex>
#include <QStringList>
#include <QTest>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QVideoSink>

#include "core/aa/ProjectedDisplaySession.hpp"
#include "oaa/av/AVChannelStartIndicationMessage.pb.h"

namespace oap::aa {

class VideoDecoderTestAccess {
public:
    static void failNextCodecInitialization()
    {
        VideoDecoder::failCodecInitForTest_.store(true);
    }

    static void publishFrame(
        VideoDecoder& decoder,
        quint64 generation,
        QSize size = QSize(kClusterViewportGeometry.encodedWidth,
                           kClusterViewportGeometry.encodedHeight))
    {
        const QVideoFrameFormat format(size,
                                       QVideoFrameFormat::Format_YUV420P);
        {
            std::scoped_lock lock(decoder.latestFrameMutex_);
            decoder.latestFrame_ = QVideoFrame(format);
            decoder.hasLatestFrame_.store(true);
        }
        emit decoder.frameReadyForGeneration(generation);
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

QByteArray startIndicationBytes(int session = 1, uint32_t config = 0)
{
    oaa::proto::messages::AVChannelStartIndication message;
    message.set_session(session);
    message.set_config(config);
    QByteArray payload(message.ByteSizeLong(), '\0');
    message.SerializeToArray(payload.data(), payload.size());
    return payload;
}

oap::aa::ProjectedDisplaySession enabledClusterDisplay()
{
    return oap::aa::ProjectedDisplaySession(
        oap::aa::ProjectedDisplayRole::Cluster,
        oap::aa::kClusterDisplayId,
        oaa::ChannelId::ClusterVideo,
        oaa::ChannelId::ClusterInput,
        true,
        oap::aa::ProjectedSetupFocus::ProjectedNoInput,
        nullptr);
}

oap::aa::ProjectedDisplaySession enabledMainDisplay()
{
    return oap::aa::ProjectedDisplaySession(
        oap::aa::ProjectedDisplayRole::Main,
        oap::aa::kMainDisplayId,
        oaa::ChannelId::Video,
        oaa::ChannelId::Input,
        true,
        oap::aa::ProjectedSetupFocus::Projected,
        nullptr);
}

void openAndStart(oap::aa::ProjectedDisplaySession& display,
                  QSignalSpy& resetSpy)
{
    display.videoHandler()->onChannelOpened();
    display.noteChannelOpened(oaa::ChannelId::ClusterVideo);
    display.videoHandler()->onMessage(
        oaa::AVMessageId::START_INDICATION, startIndicationBytes());
    QTRY_COMPARE(resetSpy.count(), 1);
}

} // namespace

class TestProjectedDisplaySession : public QObject {
    Q_OBJECT

private slots:
    void decoderConstructionFailureEntersErrorAtProtocolStart()
    {
        oap::aa::VideoDecoderTestAccess::failNextCodecInitialization();
        auto display = enabledClusterDisplay();

        QVERIFY(!display.decoder()->isOperational());
        display.beginProtocolSession();

        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Error));
        QVERIFY(display.statusText().contains(
            QStringLiteral("decoder"), Qt::CaseInsensitive));
    }

    void transientDecoderResetFailureCanRecoverNextProtocolSession()
    {
        auto display = enabledClusterDisplay();
        QSignalSpy resetSpy(display.decoder(),
                            &oap::aa::VideoDecoder::streamResetCompleted);

        display.beginProtocolSession();
        display.videoHandler()->onChannelOpened();
        display.noteChannelOpened(oaa::ChannelId::ClusterVideo);
        oap::aa::VideoDecoderTestAccess::failNextCodecInitialization();
        display.videoHandler()->onMessage(
            oaa::AVMessageId::START_INDICATION, startIndicationBytes());
        QTRY_COMPARE(resetSpy.count(), 1);
        QTRY_COMPARE(display.state(),
                     static_cast<int>(oap::aa::ProjectedDisplaySession::Error));
        QVERIFY(!display.decoder()->isOperational());

        display.endProtocolSession();
        display.beginProtocolSession();
        QCOMPARE(display.state(),
                 static_cast<int>(
                     oap::aa::ProjectedDisplaySession::WaitingForChannel));
        display.videoHandler()->onChannelOpened();
        display.noteChannelOpened(oaa::ChannelId::ClusterVideo);
        display.videoHandler()->onMessage(
            oaa::AVMessageId::START_INDICATION, startIndicationBytes(2));
        QTRY_COMPARE(resetSpy.count(), 2);
        QTRY_VERIFY(display.decoder()->isOperational());
        QCOMPARE(display.state(),
                 static_cast<int>(
                     oap::aa::ProjectedDisplaySession::WaitingForFrames));
    }

    void earlyChannelTrafficIsRejectedWithDiagnostic()
    {
        auto display = enabledClusterDisplay();
        QSignalSpy resetSpy(display.decoder(),
                            &oap::aa::VideoDecoder::streamResetCompleted);
        ScopedMessageCapture messages;

        display.videoHandler()->onChannelOpened();
        display.noteChannelOpened(oaa::ChannelId::ClusterVideo);
        display.videoHandler()->onMessage(
            oaa::AVMessageId::START_INDICATION, startIndicationBytes());
        QCoreApplication::processEvents();

        QCOMPARE(display.state(),
                 static_cast<int>(
                     oap::aa::ProjectedDisplaySession::Disconnected));
        QCOMPARE(resetSpy.count(), 0);
        QVERIFY(messages.joined().contains(
            QStringLiteral("before protocol begin")));
        QVERIFY(messages.joined().contains(
            QStringLiteral("outside protocol session")));
    }

    void disabledDisplayStaysDisabled()
    {
        oap::aa::ProjectedDisplaySession display(
            oap::aa::ProjectedDisplayRole::Cluster, 1, 12, 13, false,
            oap::aa::ProjectedSetupFocus::ProjectedNoInput, nullptr);
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Disabled));
        QCOMPARE(display.decoder(), nullptr);
        display.beginProtocolSession();
        display.noteChannelOpened(12);
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Disabled));
    }

    void geometryPropertiesAreRoleSafe()
    {
        auto cluster = enabledClusterDisplay();
        QCOMPARE(cluster.viewportEncodedWidth(), 800);
        QCOMPARE(cluster.viewportEncodedHeight(), 480);
        QCOMPARE(cluster.viewportContentX(), 250);
        QCOMPARE(cluster.viewportContentY(), 90);
        QCOMPARE(cluster.viewportContentWidth(), 300);
        QCOMPARE(cluster.viewportContentHeight(), 300);

        auto main = enabledMainDisplay();
        QCOMPARE(main.viewportEncodedWidth(), 0);
        QCOMPARE(main.viewportEncodedHeight(), 0);
        QCOMPARE(main.viewportContentX(), 0);
        QCOMPARE(main.viewportContentY(), 0);
        QCOMPARE(main.viewportContentWidth(), 0);
        QCOMPARE(main.viewportContentHeight(), 0);
    }

    void clusterStateFollowsOnlyItsLifecycle()
    {
        auto display = enabledClusterDisplay();
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Disconnected));

        display.beginProtocolSession();
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::WaitingForChannel));
        display.noteChannelOpened(oaa::ChannelId::Video);
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::WaitingForChannel));

        QSignalSpy resetSpy(display.decoder(),
                            &oap::aa::VideoDecoder::streamResetCompleted);
        openAndStart(display, resetSpy);
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::WaitingForFrames));

        const quint64 generation = resetSpy[0][0].toULongLong();
        display.decoder()->frameReadyForGeneration(generation);
        QCoreApplication::processEvents();
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Rendering));

        display.endProtocolSession();
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Disconnected));
    }

    void rejectionAndDecoderErrorAreExplicit()
    {
        auto rejected = enabledClusterDisplay();
        rejected.beginProtocolSession();
        rejected.noteChannelRejected(oaa::ChannelId::ClusterInput);
        QCOMPARE(rejected.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Rejected));

        auto failed = enabledClusterDisplay();
        failed.beginProtocolSession();
        QSignalSpy resetSpy(failed.decoder(),
                            &oap::aa::VideoDecoder::streamResetCompleted);
        openAndStart(failed, resetSpy);
        const quint64 generation = resetSpy[0][0].toULongLong();
        failed.decoder()->streamError(generation,
                                      QStringLiteral("codec init failed"));
        QCoreApplication::processEvents();
        QCOMPARE(failed.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Error));
        QVERIFY(failed.statusText().contains(QStringLiteral("codec init failed")));
    }

    void terminalStatesStayLatchedUntilNextProtocolSession()
    {
        auto rejected = enabledClusterDisplay();
        rejected.beginProtocolSession();
        rejected.noteChannelRejected(oaa::ChannelId::ClusterVideo);
        rejected.noteChannelOpened(oaa::ChannelId::ClusterVideo);
        rejected.videoHandler()->onChannelOpened();
        rejected.videoHandler()->onMessage(
            oaa::AVMessageId::START_INDICATION, startIndicationBytes());
        QCoreApplication::processEvents();
        QCOMPARE(rejected.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Rejected));

        rejected.beginProtocolSession();
        QCOMPARE(rejected.state(),
                 static_cast<int>(
                     oap::aa::ProjectedDisplaySession::WaitingForChannel));

        auto failed = enabledClusterDisplay();
        failed.beginProtocolSession();
        QSignalSpy resetSpy(failed.decoder(),
                            &oap::aa::VideoDecoder::streamResetCompleted);
        openAndStart(failed, resetSpy);
        const quint64 generation = resetSpy[0][0].toULongLong();
        failed.decoder()->streamError(generation,
                                      QStringLiteral("codec init failed"));
        QCoreApplication::processEvents();
        failed.decoder()->frameReadyForGeneration(generation);
        failed.videoHandler()->onMessage(
            oaa::AVMessageId::START_INDICATION, startIndicationBytes(2));
        QCoreApplication::processEvents();
        QCOMPARE(failed.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Error));

        failed.beginProtocolSession();
        QCOMPARE(failed.state(),
                 static_cast<int>(
                     oap::aa::ProjectedDisplaySession::WaitingForChannel));
    }

    void legacyDecoderSinkReceivesSessionFrames()
    {
        auto display = enabledClusterDisplay();
        display.beginProtocolSession();
        QSignalSpy resetSpy(display.decoder(),
                            &oap::aa::VideoDecoder::streamResetCompleted);
        openAndStart(display, resetSpy);

        QVideoSink sink;
        QSignalSpy frameSpy(&sink, &QVideoSink::videoFrameChanged);
        QVERIFY(display.attachVideoSink(&sink));
        oap::aa::VideoDecoderTestAccess::publishFrame(
            *display.decoder(), resetSpy[0][0].toULongLong());
        QTRY_COMPARE(frameSpy.count(), 1);
        QVERIFY(frameSpy[0][0].value<QVideoFrame>().isValid());
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Rendering));
    }

    void clusterRejectsMismatchedFrameBeforeSinkDelivery()
    {
        auto display = enabledClusterDisplay();
        display.beginProtocolSession();
        QSignalSpy resetSpy(display.decoder(),
                            &oap::aa::VideoDecoder::streamResetCompleted);
        openAndStart(display, resetSpy);

        QVideoSink sink;
        QSignalSpy frameSpy(&sink, &QVideoSink::videoFrameChanged);
        QVERIFY(display.attachVideoSink(&sink));
        ScopedMessageCapture capture;
        const quint64 generation = resetSpy[0][0].toULongLong();
        oap::aa::VideoDecoderTestAccess::publishFrame(
            *display.decoder(), generation, QSize(640, 480));
        oap::aa::VideoDecoderTestAccess::publishFrame(
            *display.decoder(), generation, QSize(640, 480));

        QCoreApplication::processEvents();
        QCOMPARE(frameSpy.count(), 0);
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Error));
        QVERIFY(display.statusText().contains(
            QStringLiteral("geometry mismatch"), Qt::CaseInsensitive));
        QCOMPARE(capture.joined().count(
                     QStringLiteral("decoded frame geometry mismatch")), 1);
    }

    void malformedSideMessageDoesNotTerminateLegacyMainRendering()
    {
        auto display = enabledMainDisplay();
        display.beginProtocolSession();
        display.videoHandler()->onChannelOpened();
        display.noteChannelOpened(oaa::ChannelId::Video);
        QSignalSpy resetSpy(display.decoder(),
                            &oap::aa::VideoDecoder::streamResetCompleted);
        display.videoHandler()->onMessage(
            oaa::AVMessageId::START_INDICATION, startIndicationBytes());
        QTRY_COMPARE(resetSpy.count(), 1);

        display.videoHandler()->onMessage(
            oaa::AVMessageId::VIDEO_FOCUS_REQUEST,
            QByteArray(1, static_cast<char>(0xff)));
        QCOMPARE(display.state(),
                 static_cast<int>(
                     oap::aa::ProjectedDisplaySession::WaitingForFrames));

        QVideoSink sink;
        QSignalSpy frameSpy(&sink, &QVideoSink::videoFrameChanged);
        QSignalSpy sentSpy(display.videoHandler(),
                           &oaa::IChannelHandler::sendRequested);
        display.decoder()->setVideoSink(&sink);
        display.videoHandler()->onMediaData(QByteArray(64, '\0'), 123);
        QCOMPARE(display.videoHandler()->ackCount(), 1ULL);
        QCOMPARE(sentSpy.count(), 1);
        oap::aa::VideoDecoderTestAccess::publishFrame(
            *display.decoder(), resetSpy[0][0].toULongLong(), QSize(2, 2));
        QTRY_COMPARE(frameSpy.count(), 1);
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Rendering));
    }

    void videoCloseAndReopenInvalidateOldGeneration()
    {
        for (const auto role : {oap::aa::ProjectedDisplayRole::Main,
                                oap::aa::ProjectedDisplayRole::Cluster}) {
            const uint8_t videoChannel =
                role == oap::aa::ProjectedDisplayRole::Main
                ? oaa::ChannelId::Video : oaa::ChannelId::ClusterVideo;
            const uint8_t inputChannel =
                role == oap::aa::ProjectedDisplayRole::Main
                ? oaa::ChannelId::Input : oaa::ChannelId::ClusterInput;
            const uint8_t displayId =
                role == oap::aa::ProjectedDisplayRole::Main
                ? oap::aa::kMainDisplayId : oap::aa::kClusterDisplayId;
            oap::aa::ProjectedDisplaySession display(
                role, displayId, videoChannel, inputChannel, true,
                role == oap::aa::ProjectedDisplayRole::Main
                    ? oap::aa::ProjectedSetupFocus::Projected
                    : oap::aa::ProjectedSetupFocus::ProjectedNoInput,
                nullptr);
            display.beginProtocolSession();
            QSignalSpy resetSpy(display.decoder(),
                                &oap::aa::VideoDecoder::streamResetCompleted);
            display.videoHandler()->onChannelOpened();
            display.noteChannelOpened(videoChannel);
            display.videoHandler()->onMessage(
                oaa::AVMessageId::START_INDICATION, startIndicationBytes());
            QTRY_COMPARE(resetSpy.count(), 1);
            const quint64 oldGeneration = resetSpy[0][0].toULongLong();
            display.decoder()->frameReadyForGeneration(oldGeneration);
            QCoreApplication::processEvents();
            QCOMPARE(display.state(),
                     static_cast<int>(
                         oap::aa::ProjectedDisplaySession::Rendering));

            display.videoHandler()->onChannelClosed();
            display.noteChannelClosed(videoChannel);
            QCOMPARE(display.state(),
                     static_cast<int>(
                         oap::aa::ProjectedDisplaySession::WaitingForChannel));
            display.decoder()->frameReadyForGeneration(oldGeneration);
            QCoreApplication::processEvents();
            QCOMPARE(display.state(),
                     static_cast<int>(
                         oap::aa::ProjectedDisplaySession::WaitingForChannel));

            display.videoHandler()->onChannelOpened();
            display.noteChannelOpened(videoChannel);
            display.videoHandler()->onMessage(
                oaa::AVMessageId::START_INDICATION,
                startIndicationBytes(2));
            QTRY_COMPARE(resetSpy.count(), 2);
            QVERIFY(resetSpy[1][0].toULongLong() != oldGeneration);
            QCOMPARE(display.state(),
                     static_cast<int>(
                         oap::aa::ProjectedDisplaySession::WaitingForFrames));
        }
    }

    void staleDecoderCallbacksCannotChangeNewGeneration()
    {
        auto display = enabledClusterDisplay();
        display.beginProtocolSession();
        QSignalSpy resetSpy(display.decoder(),
                            &oap::aa::VideoDecoder::streamResetCompleted);
        openAndStart(display, resetSpy);
        const quint64 oldGeneration = resetSpy[0][0].toULongLong();

        display.endProtocolSession();
        display.beginProtocolSession();
        display.videoHandler()->onChannelOpened();
        display.noteChannelOpened(oaa::ChannelId::ClusterVideo);
        display.videoHandler()->onMessage(
            oaa::AVMessageId::START_INDICATION, startIndicationBytes(2));
        QTRY_COMPARE(resetSpy.count(), 2);
        const quint64 newGeneration = resetSpy[1][0].toULongLong();
        QVERIFY(newGeneration != oldGeneration);

        display.decoder()->frameReadyForGeneration(oldGeneration);
        display.decoder()->streamError(oldGeneration, QStringLiteral("stale"));
        QCoreApplication::processEvents();
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::WaitingForFrames));

        display.decoder()->frameReadyForGeneration(newGeneration);
        QCoreApplication::processEvents();
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Rendering));
    }

    void noSinkDoesNotBlockProtocolOrRenegotiate()
    {
        auto display = enabledClusterDisplay();
        display.beginProtocolSession();
        QSignalSpy resetSpy(display.decoder(),
                            &oap::aa::VideoDecoder::streamResetCompleted);
        openAndStart(display, resetSpy);
        QSignalSpy sentSpy(display.videoHandler(),
                           &oaa::IChannelHandler::sendRequested);

        display.videoHandler()->onMediaData(QByteArray(64, '\0'), 123);
        QCOMPARE(display.videoHandler()->receivedFrameCount(), 1ULL);
        QCOMPARE(display.videoHandler()->ackCount(), 1ULL);
        QCOMPARE(sentSpy.count(), 1);

        const quint64 generation = resetSpy[0][0].toULongLong();
        display.decoder()->frameReadyForGeneration(generation);
        QCoreApplication::processEvents();
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Rendering));

        QVideoSink sink;
        QVERIFY(display.attachVideoSink(&sink));
        QCOMPARE(resetSpy.count(), 1);
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Rendering));
    }

    void videoSinkOwnershipIsExclusiveAndPointerBound()
    {
        auto display = enabledClusterDisplay();
        QVideoSink first;
        QVideoSink second;
        ScopedMessageCapture messages;

        QVERIFY(display.attachVideoSink(&first));
        QCOMPARE(display.decoder()->videoSink(), &first);
        QVERIFY(!display.attachVideoSink(&second));
        QVERIFY(!display.attachVideoSink(&second));
        QCOMPARE(display.decoder()->videoSink(), &first);
        QCOMPARE(messages.joined().count(
                     QStringLiteral("sink claim rejected; already owned")),
                 1);

        display.detachVideoSink(&second);
        QCOMPARE(display.decoder()->videoSink(), &first);
        display.detachVideoSink(&first);
        QCOMPARE(display.decoder()->videoSink(), nullptr);

        QVERIFY(display.attachVideoSink(&second));
        QCOMPARE(display.decoder()->videoSink(), &second);
        QVERIFY(!display.attachVideoSink(&first));
        QCOMPARE(messages.joined().count(
                     QStringLiteral("sink claim rejected; already owned")),
                 2);
    }

    void videoSinkAvailabilityTracksClaimDetachAndDestruction()
    {
        auto display = enabledClusterDisplay();
        QSignalSpy availabilitySpy(
            &display,
            &oap::aa::ProjectedDisplaySession::videoSinkAvailabilityChanged);
        QCOMPARE(display.isVideoSinkAvailable(), true);

        QVideoSink first;
        QVERIFY(display.attachVideoSink(&first));
        QCOMPARE(display.isVideoSinkAvailable(), false);
        QCOMPARE(availabilitySpy.count(), 1);

        display.detachVideoSink(&first);
        QCOMPARE(display.isVideoSinkAvailable(), true);
        QCOMPARE(availabilitySpy.count(), 2);

        auto second = std::make_unique<QVideoSink>();
        QVERIFY(display.attachVideoSink(second.get()));
        QCOMPARE(display.isVideoSinkAvailable(), false);
        QCOMPARE(availabilitySpy.count(), 3);
        second.reset();
        QCOMPARE(display.isVideoSinkAvailable(), true);
        QCOMPARE(availabilitySpy.count(), 4);
    }

    void reentrantAvailabilityObserverCannotLeaveFalseOwnership()
    {
        auto display = enabledClusterDisplay();
        QVideoSink sink;
        connect(
            &display,
            &oap::aa::ProjectedDisplaySession::videoSinkAvailabilityChanged,
            &display,
            [&display, &sink]() {
                if (!display.isVideoSinkAvailable())
                    display.detachVideoSink(&sink);
            },
            Qt::DirectConnection);

        QVERIFY(!display.attachVideoSink(&sink));
        QVERIFY(display.isVideoSinkAvailable());
        QCOMPARE(display.decoder()->videoSink(), nullptr);
    }
};

QTEST_MAIN(TestProjectedDisplaySession)
#include "test_projected_display_session.moc"
