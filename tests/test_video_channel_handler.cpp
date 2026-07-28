#include <QTest>
#include <QSignalSpy>
#include <memory>
#include <oaa/HU/Handlers/VideoChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include "oaa/av/AVChannelSetupRequestMessage.pb.h"
#include "oaa/av/AVChannelSetupResponseMessage.pb.h"
#include "oaa/av/MediaCodecTypeEnum.pb.h"
#include "oaa/av/AVChannelStartIndicationMessage.pb.h"
#include "oaa/av/AVChannelMediaOptionsMessage.pb.h"
#include "oaa/av/AVMediaAckIndicationMessage.pb.h"
#include "oaa/video/VideoFocusIndicationMessage.pb.h"
#include "oaa/video/VideoFocusModeEnum.pb.h"
#include "oaa/video/UiConfigRequestMessage.pb.h"
#include "oaa/video/UpdateHuUiConfigResponse.pb.h"
#include <google/protobuf/unknown_field_set.h>

class TestVideoChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testSetupResponseConfigCount_data() {
        QTest::addColumn<int>("count");
        QTest::newRow("one") << 1;
        QTest::newRow("two") << 2;
        QTest::newRow("four") << 4;
    }

    void testSetupResponseConfigCount() {
        QFETCH(int, count);
        oaa::hu::VideoChannelHandler handler;
        handler.setNumVideoConfigs(static_cast<uint32_t>(count));
        handler.onChannelOpened();
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        oaa::proto::messages::AVChannelSetupRequest request;
        request.set_media_codec_type(
            oaa::proto::enums::MediaCodecType_Enum_MEDIA_CODEC_VIDEO_H264_BP);
        QByteArray payload(request.ByteSizeLong(), '\0');
        QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
        handler.onMessage(oaa::AVMessageId::SETUP_REQUEST, payload);

        QCOMPARE(sendSpy.count(), 2);
        oaa::proto::messages::AVChannelSetupResponse response;
        const QByteArray responsePayload = sendSpy[0][2].toByteArray();
        QVERIFY(response.ParseFromArray(responsePayload.constData(), responsePayload.size()));
        QCOMPARE(response.configs_size(), count);
        for (int index = 0; index < count; ++index)
            QCOMPARE(response.configs(index), static_cast<uint32_t>(index));
    }

    void testClusterVideoUsesInjectedChannelAndNoInputFocus() {
        oaa::hu::VideoChannelHandler handler(
            oaa::ChannelId::ClusterVideo,
            oaa::proto::enums::VideoFocusMode::PROJECTED_NO_INPUT_FOCUS);
        handler.setNumVideoConfigs(1);
        handler.onChannelOpened();
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        QSignalSpy setupSpy(&handler,
                            &oaa::hu::VideoChannelHandler::setupRequested);

        oaa::proto::messages::AVChannelSetupRequest request;
        request.set_media_codec_type(
            oaa::proto::enums::MediaCodecType::MEDIA_CODEC_VIDEO_H264_BP);
        QByteArray payload(request.ByteSizeLong(), '\0');
        QVERIFY(request.SerializeToArray(payload.data(), payload.size()));
        handler.onMessage(oaa::AVMessageId::SETUP_REQUEST, payload);

        QCOMPARE(sendSpy.count(), 2);
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::ClusterVideo);
        QCOMPARE(sendSpy[1][0].value<uint8_t>(), oaa::ChannelId::ClusterVideo);
        oaa::proto::messages::VideoFocusIndication focus;
        const QByteArray focusPayload = sendSpy[1][2].toByteArray();
        QVERIFY(focus.ParseFromArray(focusPayload.constData(), focusPayload.size()));
        QCOMPARE(focus.focus_mode(),
                 oaa::proto::enums::VideoFocusMode::PROJECTED_NO_INPUT_FOCUS);
        QCOMPARE(setupSpy.count(), 1);
        QCOMPARE(setupSpy[0][0].toInt(),
                 static_cast<int>(oaa::proto::enums::MediaCodecType::MEDIA_CODEC_VIDEO_H264_BP));
    }

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
        QSignalSpy detailsSpy(
            &handler,
            &oaa::hu::VideoChannelHandler::streamStartDetailsReceived);

        handler.onChannelOpened();

        oaa::proto::messages::AVChannelStartIndication start;
        start.set_session(7);
        start.set_config(0);
        QByteArray payload(start.ByteSizeLong(), '\0');
        start.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::START_INDICATION, payload);

        QCOMPARE(startSpy.count(), 1);
        QCOMPARE(startSpy[0][0].value<int32_t>(), 7);
        QVERIFY(detailsSpy.isValid());
        QCOMPARE(detailsSpy.count(), 1);
        QCOMPARE(detailsSpy[0][0].value<int32_t>(), 7);
        QCOMPARE(detailsSpy[0][1].value<uint32_t>(), 0u);
        QCOMPARE(detailsSpy[0][2].toInt(), -1);
        QCOMPARE(detailsSpy[0][3].toBool(), false);
        QVERIFY(detailsSpy[0][4].toString().isEmpty());
        QVERIFY(handler.canAcceptMedia());
    }

    void testExtendedStartIndicationEmitsBoundedDetails() {
        oaa::hu::VideoChannelHandler handler;
        QSignalSpy startSpy(&handler,
                            &oaa::hu::VideoChannelHandler::streamStarted);
        QSignalSpy detailsSpy(
            &handler,
            &oaa::hu::VideoChannelHandler::streamStartDetailsReceived);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();

        oaa::proto::messages::AVChannelStartIndication start;
        start.set_session(73);
        start.set_config(2);
        start.set_session_type(
            oaa::proto::enums::AVChannelSessionType::SESSION_TYPE_ALTERNATE);
        auto* mediaConfig = start.mutable_media_config();
        mediaConfig->set_feature_flag_1(true);
        mediaConfig->set_config_value(37);
        mediaConfig->GetReflection()
            ->MutableUnknownFields(mediaConfig)
            ->AddLengthDelimited(100, std::string(800, 'x'));

        const QByteArray payload = QByteArray::fromStdString(
            start.SerializeAsString());
        handler.onMessage(oaa::AVMessageId::START_INDICATION, payload);

        QCOMPARE(startSpy.count(), 1);
        QCOMPARE(startSpy[0][0].value<int32_t>(), 73);
        QCOMPARE(startSpy[0][1].value<uint32_t>(), 2u);
        QVERIFY(detailsSpy.isValid());
        QCOMPARE(detailsSpy.count(), 1);
        QCOMPARE(detailsSpy[0][0].value<int32_t>(), 73);
        QCOMPARE(detailsSpy[0][1].value<uint32_t>(), 2u);
        QCOMPARE(detailsSpy[0][2].toInt(), 2);
        QCOMPARE(detailsSpy[0][3].toBool(), true);
        const QString summary = detailsSpy[0][4].toString();
        QCOMPARE(summary, QString::fromStdString(
                              mediaConfig->ShortDebugString()).left(512));
        QCOMPARE(summary.size(), 512);
        QVERIFY(summary.contains(QStringLiteral("config_value: 37")));
        QCOMPARE(sendSpy.count(), 0);
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
        QCOMPARE(handler.receivedFrameCount(), 2u);
        QCOMPARE(handler.ackCount(), 2u);

        // ACK value must be delta-per-message (1), not cumulative count.
        oaa::proto::messages::AVMediaAckIndication ack0;
        QByteArray ackPayload0 = sendSpy[0][2].toByteArray();
        QVERIFY(ack0.ParseFromArray(ackPayload0.constData(), ackPayload0.size()));
        QCOMPARE(ack0.ack_count(), 1);

        oaa::proto::messages::AVMediaAckIndication ack1;
        QByteArray ackPayload1 = sendSpy[1][2].toByteArray();
        QVERIFY(ack1.ParseFromArray(ackPayload1.constData(), ackPayload1.size()));
        QCOMPARE(ack1.ack_count(), 1);

        handler.onChannelOpened();
        QCOMPARE(handler.receivedFrameCount(), 0u);
        QCOMPARE(handler.ackCount(), 0u);
    }

    void testVideoMediaAckPolicy_data() {
        QTest::addColumn<int>("galMajor");
        QTest::addColumn<int>("galMinor");

        QTest::newRow("GAL 1.7") << 1 << 7;
        QTest::newRow("GAL 4.3") << 4 << 3;
        QTest::newRow("GAL 5.0") << 5 << 0;
        QTest::newRow("GAL 5.1") << 5 << 1;
        QTest::newRow("GAL 6.0") << 6 << 0;
    }

    void testVideoMediaAckPolicy() {
        QFETCH(int, galMajor);
        QFETCH(int, galMinor);
        qRegisterMetaType<std::shared_ptr<const QByteArray>>();

        oaa::hu::VideoChannelHandler handler;
        handler.configureSession(oaa::SessionProtocolPolicy(
            {static_cast<uint16_t>(galMajor),
             static_cast<uint16_t>(galMinor)}));
        handler.onChannelOpened();

        oaa::proto::messages::AVChannelStartIndication start;
        start.set_session(60);
        start.set_config(0);
        const QByteArray startPayload = QByteArray::fromStdString(
            start.SerializeAsString());
        handler.onMessage(oaa::AVMessageId::START_INDICATION, startPayload);

        QSignalSpy frameSpy(&handler,
                            &oaa::hu::VideoChannelHandler::videoFrameData);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        handler.onMediaData(QByteArray(256, '\x01'), 100);

        QCOMPARE(frameSpy.count(), 1);
        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::ACK_INDICATION));
        oaa::proto::messages::AVMediaAckIndication ack;
        const QByteArray ackPayload = sendSpy[0][2].toByteArray();
        QVERIFY(ack.ParseFromArray(ackPayload.constData(), ackPayload.size()));
        QCOMPARE(ack.session_id(), 60);
        QCOMPARE(ack.ack_count(), 1);
        QCOMPARE(handler.receivedFrameCount(), 1u);
        QCOMPARE(handler.ackCount(), 1u);
    }

    void testMediaOptionsEmitsOneBoundedTypedSummary() {
        oaa::hu::VideoChannelHandler handler;
        QSignalSpy optionsSpy(
            &handler,
            &oaa::hu::VideoChannelHandler::mediaOptionsReceived);
        QSignalSpy unknownSpy(&handler,
                              &oaa::IChannelHandler::unknownMessage);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        oaa::proto::messages::AVChannelMediaOptions options;
        const auto populatePing = [](auto* ping, qint64 interval,
                                     int timeout) {
            ping->set_ping_interval_ns(interval);
            ping->set_ping_timeout_ms(timeout);
        };
        populatePing(options.mutable_ping_configuration_1(), 1001, 101);
        options.set_bool_value_2(true);
        populatePing(options.mutable_ping_configuration_3(), 1003, 103);
        populatePing(options.mutable_ping_configuration_4(), 1004, 104);
        populatePing(options.mutable_ping_configuration_5(), 1005, 105);
        populatePing(options.mutable_ping_configuration_6(), 1006, 106);
        options.set_uint32_value_7(77);
        populatePing(options.mutable_ping_configuration_8(), 1008, 108);
        options.set_bool_value_9(true);
        populatePing(options.mutable_ping_configuration_10(), 1010, 110);
        options.set_bool_value_11(true);
        populatePing(options.mutable_ping_configuration_12(), 1012, 112);
        populatePing(options.mutable_ping_configuration_13(), 1013, 113);
        options.GetReflection()
            ->MutableUnknownFields(&options)
            ->AddLengthDelimited(100, std::string(800, 'x'));

        const QByteArray payload = QByteArray::fromStdString(
            options.SerializeAsString());
        handler.onMessage(oaa::AVMessageId::MEDIA_OPTIONS, payload);

        QVERIFY(optionsSpy.isValid());
        QCOMPARE(optionsSpy.count(), 1);
        const QString summary = optionsSpy[0][0].toString();
        QCOMPARE(summary, QString::fromStdString(
                              options.ShortDebugString()).left(512));
        QCOMPARE(summary.size(), 512);
        QVERIFY(summary.contains(QStringLiteral("ping_configuration_1")));
        QVERIFY(summary.contains(QStringLiteral("uint32_value_7: 77")));
        QCOMPARE(unknownSpy.count(), 0);
        QCOMPARE(sendSpy.count(), 0);
    }

    void testMalformedMediaOptionsPreservesStreamingAndAckState() {
        qRegisterMetaType<std::shared_ptr<const QByteArray>>();

        oaa::hu::VideoChannelHandler handler;
        handler.configureSession(oaa::SessionProtocolPolicy(
            oaa::kGalVersion6_0));
        handler.onChannelOpened();

        oaa::proto::messages::AVChannelStartIndication start;
        start.set_session(61);
        start.set_config(0);
        const QByteArray startPayload = QByteArray::fromStdString(
            start.SerializeAsString());
        handler.onMessage(oaa::AVMessageId::START_INDICATION, startPayload);

        QSignalSpy optionsSpy(
            &handler,
            &oaa::hu::VideoChannelHandler::mediaOptionsReceived);
        QSignalSpy unknownSpy(&handler,
                              &oaa::IChannelHandler::unknownMessage);
        QSignalSpy frameSpy(&handler,
                            &oaa::hu::VideoChannelHandler::videoFrameData);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onMessage(oaa::AVMessageId::MEDIA_OPTIONS,
                          QByteArray::fromHex("0a0508"));

        QVERIFY(optionsSpy.isValid());
        QCOMPARE(optionsSpy.count(), 0);
        QCOMPARE(unknownSpy.count(), 0);
        QCOMPARE(sendSpy.count(), 0);
        QVERIFY(handler.canAcceptMedia());

        handler.onMediaData(QByteArray(256, '\x02'), 101);
        QCOMPARE(frameSpy.count(), 1);
        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::ACK_INDICATION));
        oaa::proto::messages::AVMediaAckIndication ack;
        const QByteArray ackPayload = sendSpy[0][2].toByteArray();
        QVERIFY(ack.ParseFromArray(ackPayload.constData(), ackPayload.size()));
        QCOMPARE(ack.session_id(), 61);
        QCOMPARE(ack.ack_count(), 1);
    }

    void testMalformedSetupReportsBoundedError() {
        oaa::hu::VideoChannelHandler handler;
        QSignalSpy errorSpy(&handler,
                            &oaa::hu::VideoChannelHandler::handlerError);
        handler.onChannelOpened();
        handler.onMessage(oaa::AVMessageId::SETUP_REQUEST,
                          QByteArray("\x80", 1));
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(!errorSpy[0][0].toString().isEmpty());
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

        handler.onMessage(0x8011, payload);

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
        QCOMPARE(sendSpy[0][1].value<uint16_t>(), uint16_t{0x8012});

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

        handler.onMessage(0x8011, payload);

        // Should still emit with empty maps (no crash)
        QCOMPARE(tokenSpy.count(), 1);

        auto dayTokens = tokenSpy[0][0].value<QMap<QString, uint32_t>>();
        auto nightTokens = tokenSpy[0][1].value<QMap<QString, uint32_t>>();
        QCOMPARE(dayTokens.size(), 0);
        QCOMPARE(nightTokens.size(), 0);

        // Should still send ACCEPTED response
        QCOMPARE(sendSpy.count(), 1);
    }

    void testAuditedModernRawIdsAreExplicitlyClassified() {
        oaa::hu::VideoChannelHandler handler;
        QSignalSpy unknownSpy(&handler,
                              &oaa::hu::VideoChannelHandler::unknownMessage);

        const QByteArray opaque("\x08\x01", 2);
        for (const uint16_t id : {uint16_t{0x800A}, uint16_t{0x800C},
                                  uint16_t{0x800D}, uint16_t{0x8014},
                                  uint16_t{0x8015}}) {
            handler.onMessage(id, opaque);
        }
        QCOMPARE(unknownSpy.count(), 0);

        handler.onMessage(0x8010, opaque);
        QCOMPARE(unknownSpy.count(), 1);
        QCOMPARE(unknownSpy[0][0].value<uint16_t>(), uint16_t{0x8010});
    }
};

QTEST_MAIN(TestVideoChannelHandler)
#include "test_video_channel_handler.moc"
