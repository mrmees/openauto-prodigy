#include <QSignalSpy>
#include <QTest>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QVideoSink>

#include "core/aa/ProjectedDisplaySession.hpp"
#include "oaa/av/AVChannelStartIndicationMessage.pb.h"

namespace oap::aa {

class VideoDecoderTestAccess {
public:
    static void publishFrame(VideoDecoder& decoder, quint64 generation)
    {
        const QVideoFrameFormat format(
            QSize(2, 2), QVideoFrameFormat::Format_YUV420P);
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
        display.decoder()->setVideoSink(&sink);
        oap::aa::VideoDecoderTestAccess::publishFrame(
            *display.decoder(), resetSpy[0][0].toULongLong());
        QTRY_COMPARE(frameSpy.count(), 1);
        QVERIFY(frameSpy[0][0].value<QVideoFrame>().isValid());
        QCOMPARE(display.state(),
                 static_cast<int>(oap::aa::ProjectedDisplaySession::Rendering));
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

        QVERIFY(display.attachVideoSink(&first));
        QCOMPARE(display.decoder()->videoSink(), &first);
        QVERIFY(!display.attachVideoSink(&second));
        QCOMPARE(display.decoder()->videoSink(), &first);

        display.detachVideoSink(&second);
        QCOMPARE(display.decoder()->videoSink(), &first);
        display.detachVideoSink(&first);
        QCOMPARE(display.decoder()->videoSink(), nullptr);

        QVERIFY(display.attachVideoSink(&second));
        QCOMPARE(display.decoder()->videoSink(), &second);
    }
};

QTEST_MAIN(TestProjectedDisplaySession)
#include "test_projected_display_session.moc"
