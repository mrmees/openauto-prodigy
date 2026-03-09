#include <QTest>
#include <QSignalSpy>
#include <memory>
#include <oaa/HU/Handlers/VideoChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include "oaa/av/AVChannelSetupRequestMessage.pb.h"
#include "oaa/av/MediaCodecTypeEnum.pb.h"
#include "oaa/av/AVChannelStartIndicationMessage.pb.h"
#include "oaa/av/AVMediaAckIndicationMessage.pb.h"
#include "oaa/video/VideoFocusIndicationMessage.pb.h"
#include "oaa/video/VideoFocusModeEnum.pb.h"
#include "oaa/video/UiConfigRequestMessage.pb.h"
#include "oaa/video/UpdateHuUiConfigResponse.pb.h"

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
        req.set_media_codec_type(oaa::proto::enums::MediaCodecType_Enum_MEDIA_CODEC_VIDEO_H264_BP); // Phone sends its own internal index
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
        handler.onMediaData(h264Data, 1234567891);

        QCOMPARE(frameSpy.count(), 2);
        auto sharedData = frameSpy[0][0].value<std::shared_ptr<const QByteArray>>();
        QVERIFY(sharedData);
        QCOMPARE(sharedData->size(), 4096);

        // Timestamp should be a steady_clock value (nanoseconds > 0),
        // NOT the AA protocol timestamp we passed in
        qint64 emittedTs = frameSpy[0][1].value<qint64>();
        QVERIFY(emittedTs > 0);
        QVERIFY(emittedTs != 1234567890); // Must NOT be the protocol timestamp

        // Should send ACK for each frame
        QCOMPARE(sendSpy.count(), 2);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::ACK_INDICATION));
        QCOMPARE(sendSpy[1][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::ACK_INDICATION));

        // ACK value must be delta-per-message (1), not cumulative count.
        oaa::proto::messages::AVMediaAckIndication ack0;
        QByteArray ackPayload0 = sendSpy[0][2].toByteArray();
        QVERIFY(ack0.ParseFromArray(ackPayload0.constData(), ackPayload0.size()));
        QCOMPARE(ack0.ack_count(), 1);

        oaa::proto::messages::AVMediaAckIndication ack1;
        QByteArray ackPayload1 = sendSpy[1][2].toByteArray();
        QVERIFY(ack1.ParseFromArray(ackPayload1.constData(), ackPayload1.size()));
        QCOMPARE(ack1.ack_count(), 1);
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

    void testUiConfigRequestParsesDayNightTokens() {
        qRegisterMetaType<QMap<QString, uint32_t>>("QMap<QString,uint32_t>");

        oaa::hu::VideoChannelHandler handler;
        QSignalSpy tokenSpy(&handler, &oaa::hu::VideoChannelHandler::uiConfigTokensReceived);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();

        // Build UiConfigRequest with day (primary) and night (secondary) tokens
        oaa::proto::messages::UiConfigRequest request;
        auto* config = request.mutable_config();

        // Day tokens (primary_configs)
        auto* day1 = config->add_primary_configs();
        day1->set_key("primary");
        day1->mutable_config_value()->set_value(0xFF1234AB);

        auto* day2 = config->add_primary_configs();
        day2->set_key("on-surface");
        day2->mutable_config_value()->set_value(0xFFCCCCCC);

        // Night tokens (secondary_configs)
        auto* night1 = config->add_secondary_configs();
        night1->set_key("primary");
        night1->mutable_config_value()->set_value(0xFF000000);

        auto* night2 = config->add_secondary_configs();
        night2->set_key("on-surface");
        night2->mutable_config_value()->set_value(0xFF333333);

        QByteArray payload(request.ByteSizeLong(), '\0');
        request.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::OVERLAY_SESSION_UPDATE, payload);

        // Verify uiConfigTokensReceived emitted once
        QCOMPARE(tokenSpy.count(), 1);

        auto dayTokens = tokenSpy[0][0].value<QMap<QString, uint32_t>>();
        auto nightTokens = tokenSpy[0][1].value<QMap<QString, uint32_t>>();

        QCOMPARE(dayTokens.size(), 2);
        QCOMPARE(dayTokens["primary"], 0xFF1234ABu);
        QCOMPARE(dayTokens["on-surface"], 0xFFCCCCCCu);

        QCOMPARE(nightTokens.size(), 2);
        QCOMPARE(nightTokens["primary"], 0xFF000000u);
        QCOMPARE(nightTokens["on-surface"], 0xFF333333u);

        // Verify ACCEPTED response (0x8012) sent
        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::UPDATE_HU_UI_CONFIG_REQUEST));

        // Parse the response payload to verify ACCEPTED status
        QByteArray respPayload = sendSpy[0][2].toByteArray();
        oaa::proto::messages::UpdateHuUiConfigResponse resp;
        QVERIFY(resp.ParseFromArray(respPayload.constData(), respPayload.size()));
        QCOMPARE(static_cast<int>(resp.status()),
                 static_cast<int>(oaa::proto::messages::THEMING_TOKENS_ACCEPTED));
    }

    void testUiConfigRequestEmptyConfig() {
        qRegisterMetaType<QMap<QString, uint32_t>>("QMap<QString,uint32_t>");

        oaa::hu::VideoChannelHandler handler;
        QSignalSpy tokenSpy(&handler, &oaa::hu::VideoChannelHandler::uiConfigTokensReceived);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();

        // Empty UiConfigRequest (no config field set)
        oaa::proto::messages::UiConfigRequest request;
        QByteArray payload(request.ByteSizeLong(), '\0');
        request.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::OVERLAY_SESSION_UPDATE, payload);

        // Should still emit with empty maps (no crash)
        QCOMPARE(tokenSpy.count(), 1);

        auto dayTokens = tokenSpy[0][0].value<QMap<QString, uint32_t>>();
        auto nightTokens = tokenSpy[0][1].value<QMap<QString, uint32_t>>();
        QCOMPARE(dayTokens.size(), 0);
        QCOMPARE(nightTokens.size(), 0);

        // Should still send ACCEPTED response
        QCOMPARE(sendSpy.count(), 1);
    }
};

QTEST_MAIN(TestVideoChannelHandler)
#include "test_video_channel_handler.moc"
