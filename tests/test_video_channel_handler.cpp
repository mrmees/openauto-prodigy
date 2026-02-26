#include <QTest>
#include <QSignalSpy>
#include <memory>
#include <oaa/HU/Handlers/VideoChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include "AVChannelSetupRequestMessage.pb.h"
#include "AVChannelStartIndicationMessage.pb.h"
#include "VideoFocusIndicationMessage.pb.h"
#include "VideoFocusModeEnum.pb.h"

class TestVideoChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testChannelId() {
        oaa::hu::VideoChannelHandler handler;
        QCOMPARE(handler.channelId(), oaa::ChannelId::Video);
    }

    void testSetupRequestResponds() {
        oaa::hu::VideoChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();

        oaa::proto::messages::AVChannelSetupRequest req;
        req.set_config_index(3); // Phone sends its own internal index
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::SETUP_REQUEST, payload);

        // Expect 2 sends: SETUP_RESPONSE + VIDEO_FOCUS_INDICATION (FOCUSED)
        QCOMPARE(sendSpy.count(), 2);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::SETUP_RESPONSE));
        QCOMPARE(sendSpy[1][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::VIDEO_FOCUS_INDICATION));
    }

    void testStartIndicationEmitsSignal() {
        oaa::hu::VideoChannelHandler handler;
        QSignalSpy startSpy(&handler, &oaa::hu::VideoChannelHandler::streamStarted);

        handler.onChannelOpened();

        oaa::proto::messages::AVChannelStartIndication start;
        start.set_session(7);
        start.set_config(0);
        QByteArray payload(start.ByteSizeLong(), '\0');
        start.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::START_INDICATION, payload);

        QCOMPARE(startSpy.count(), 1);
        QCOMPARE(startSpy[0][0].value<int32_t>(), 7);
        QVERIFY(handler.canAcceptMedia());
    }

    void testMediaDataEmitsFrameAndAck() {
        qRegisterMetaType<std::shared_ptr<const QByteArray>>();

        oaa::hu::VideoChannelHandler handler;
        handler.onChannelOpened();

        // Start stream
        oaa::proto::messages::AVChannelStartIndication start;
        start.set_session(1);
        start.set_config(0);
        QByteArray startPayload(start.ByteSizeLong(), '\0');
        start.SerializeToArray(startPayload.data(), startPayload.size());
        handler.onMessage(oaa::AVMessageId::START_INDICATION, startPayload);

        QSignalSpy frameSpy(&handler, &oaa::hu::VideoChannelHandler::videoFrameData);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        QByteArray h264Data(4096, '\x00');
        handler.onMediaData(h264Data, 1234567890);

        QCOMPARE(frameSpy.count(), 1);
        auto sharedData = frameSpy[0][0].value<std::shared_ptr<const QByteArray>>();
        QVERIFY(sharedData);
        QCOMPARE(sharedData->size(), 4096);

        // Timestamp should be a steady_clock value (nanoseconds > 0),
        // NOT the AA protocol timestamp we passed in
        qint64 emittedTs = frameSpy[0][1].value<qint64>();
        QVERIFY(emittedTs > 0);
        QVERIFY(emittedTs != 1234567890); // Must NOT be the protocol timestamp

        // Should send ACK
        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::ACK_INDICATION));
    }

    void testVideoFocusIndication() {
        oaa::hu::VideoChannelHandler handler;
        QSignalSpy focusSpy(&handler, &oaa::hu::VideoChannelHandler::videoFocusChanged);

        handler.onChannelOpened();

        oaa::proto::messages::VideoFocusIndication indication;
        indication.set_focus_mode(oaa::proto::enums::VideoFocusMode::PROJECTED);
        indication.set_unrequested(false);
        QByteArray payload(indication.ByteSizeLong(), '\0');
        indication.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::VIDEO_FOCUS_INDICATION, payload);

        QCOMPARE(focusSpy.count(), 1);
        QCOMPARE(focusSpy[0][0].toInt(), 1); // PROJECTED
        QCOMPARE(focusSpy[0][1].toBool(), false);
    }

    void testRequestVideoFocus() {
        oaa::hu::VideoChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();
        handler.requestVideoFocus(true);

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::VIDEO_FOCUS_INDICATION));
    }

    void testMediaDataIgnoredWhenNotStreaming() {
        qRegisterMetaType<std::shared_ptr<const QByteArray>>();

        oaa::hu::VideoChannelHandler handler;
        QSignalSpy frameSpy(&handler, &oaa::hu::VideoChannelHandler::videoFrameData);

        handler.onChannelOpened();
        handler.onMediaData(QByteArray(1024, '\x00'), 0);
        QCOMPARE(frameSpy.count(), 0);
    }
};

QTEST_MAIN(TestVideoChannelHandler)
#include "test_video_channel_handler.moc"
