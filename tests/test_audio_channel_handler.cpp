#include <QTest>
#include <QSignalSpy>
#include <oaa/HU/Handlers/AudioChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include "oaa/av/AVChannelSetupRequestMessage.pb.h"
#include "oaa/av/AVChannelSetupResponseMessage.pb.h"
#include "oaa/av/AVMediaAckIndicationMessage.pb.h"
#include "oaa/av/MediaCodecTypeEnum.pb.h"
#include "oaa/av/AVChannelStartIndicationMessage.pb.h"
#include <google/protobuf/unknown_field_set.h>

class TestAudioChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testMediaChannelId() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QCOMPARE(handler.channelId(), oaa::ChannelId::MediaAudio);
    }

    void testSpeechChannelId() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::SpeechAudio);
        QCOMPARE(handler.channelId(), oaa::ChannelId::SpeechAudio);
    }

    void testSystemChannelId() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::SystemAudio);
        QCOMPARE(handler.channelId(), oaa::ChannelId::SystemAudio);
    }

    void testAVSetupRequestResponds() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();

        oaa::proto::messages::AVChannelSetupRequest req;
        req.set_media_codec_type(static_cast<oaa::proto::enums::MediaCodecType_Enum>(0));
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::SETUP_REQUEST, payload);

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::SETUP_RESPONSE));
        oaa::proto::messages::AVChannelSetupResponse response;
        const QByteArray responsePayload = sendSpy[0][2].toByteArray();
        QVERIFY(response.ParseFromArray(responsePayload.constData(),
                                        responsePayload.size()));
        QCOMPARE(response.max_unacked(), 10);
    }

    void testStartIndicationEmitsSignal() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy startSpy(&handler, &oaa::hu::AudioChannelHandler::streamStarted);
        QSignalSpy detailsSpy(
            &handler,
            &oaa::hu::AudioChannelHandler::streamStartDetailsReceived);

        handler.onChannelOpened();

        oaa::proto::messages::AVChannelStartIndication start;
        start.set_session(42);
        start.set_config(0);
        QByteArray payload(start.ByteSizeLong(), '\0');
        start.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::START_INDICATION, payload);

        QCOMPARE(startSpy.count(), 1);
        QCOMPARE(startSpy[0][0].value<int32_t>(), 42);
        QCOMPARE(detailsSpy.count(), 1);
        QCOMPARE(detailsSpy[0][0].value<int32_t>(), 42);
        QCOMPARE(detailsSpy[0][1].value<uint32_t>(), 0u);
        QCOMPARE(detailsSpy[0][2].toInt(), -1);
        QCOMPARE(detailsSpy[0][3].toBool(), false);
        QVERIFY(detailsSpy[0][4].toString().isEmpty());
        QVERIFY(handler.canAcceptMedia());
    }

    void testExtendedStartIndicationEmitsBoundedDetails() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy startSpy(&handler,
                            &oaa::hu::AudioChannelHandler::streamStarted);
        QSignalSpy detailsSpy(
            &handler,
            &oaa::hu::AudioChannelHandler::streamStartDetailsReceived);

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

        QByteArray payload(start.ByteSizeLong(), '\0');
        start.SerializeToArray(payload.data(), payload.size());
        handler.onMessage(oaa::AVMessageId::START_INDICATION, payload);

        QCOMPARE(startSpy.count(), 1);
        QCOMPARE(startSpy[0][0].value<int32_t>(), 73);
        QCOMPARE(detailsSpy.count(), 1);
        QCOMPARE(detailsSpy[0][0].value<int32_t>(), 73);
        QCOMPARE(detailsSpy[0][1].value<uint32_t>(), 2u);
        QCOMPARE(detailsSpy[0][2].toInt(), 2);
        QCOMPARE(detailsSpy[0][3].toBool(), true);
        const QString summary = detailsSpy[0][4].toString();
        QCOMPARE(summary.size(), 512);
        QVERIFY(summary.contains(QStringLiteral("config_value: 37")));
    }

    void testMediaDataAckPolicy_data() {
        QTest::addColumn<int>("galMajor");
        QTest::addColumn<int>("galMinor");
        QTest::addColumn<int>("expectedAckCount");

        QTest::newRow("GAL 1.7") << 1 << 7 << 3;
        QTest::newRow("GAL 4.3") << 4 << 3 << 3;
        QTest::newRow("GAL 5.0") << 5 << 0 << 0;
        QTest::newRow("GAL 5.1") << 5 << 1 << 0;
    }

    void testMediaDataAckPolicy() {
        QFETCH(int, galMajor);
        QFETCH(int, galMinor);
        QFETCH(int, expectedAckCount);

        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        handler.configureSession(oaa::SessionProtocolPolicy({
            static_cast<uint16_t>(galMajor),
            static_cast<uint16_t>(galMinor)}));
        handler.onChannelOpened();

        // Start the stream
        oaa::proto::messages::AVChannelStartIndication start;
        start.set_session(1);
        start.set_config(0);
        QByteArray startPayload(start.ByteSizeLong(), '\0');
        start.SerializeToArray(startPayload.data(), startPayload.size());
        handler.onMessage(oaa::AVMessageId::START_INDICATION, startPayload);

        QSignalSpy dataSpy(&handler, &oaa::hu::AudioChannelHandler::audioDataReceived);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        // Each accepted frame immediately replenishes one advertised permit.
        QByteArray pcmData(960, '\x42');
        for (int i = 0; i < 3; ++i)
            handler.onMediaData(pcmData, 1234567890 + i);

        QCOMPARE(dataSpy.count(), 3);
        QCOMPARE(dataSpy[0][0].toByteArray().size(), 960);

        QCOMPARE(sendSpy.count(), expectedAckCount);
        for (const auto& emission : sendSpy) {
            QCOMPARE(emission[1].value<uint16_t>(),
                     static_cast<uint16_t>(oaa::AVMessageId::ACK_INDICATION));
            oaa::proto::messages::AVMediaAckIndication ack;
            const QByteArray ackPayload = emission[2].toByteArray();
            QVERIFY(ack.ParseFromArray(ackPayload.constData(), ackPayload.size()));
            QCOMPARE(ack.session_id(), 1);
            QCOMPARE(ack.ack_count(), 1);
        }
    }

    void testMediaDataIgnoredWhenNotStreaming() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy dataSpy(&handler, &oaa::hu::AudioChannelHandler::audioDataReceived);

        handler.onChannelOpened();
        // Not started — media data should be silently ignored
        handler.onMediaData(QByteArray(960, '\x42'), 0);
        QCOMPARE(dataSpy.count(), 0);
    }

    void testRetractedMessageFallsToDefault() {
        // 0x8021 and 0x8022 are retracted — they now fall through to the
        // default case which logs and emits unknownMessage. Should not crash.
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        handler.onChannelOpened();

        QSignalSpy unknownSpy(&handler, &oaa::hu::AudioChannelHandler::unknownMessage);
        QByteArray payload("\x08\x01", 2);
        handler.onMessage(0x8021, payload);
        handler.onMessage(0x8022, payload);
        QCOMPARE(unknownSpy.count(), 2);
    }

    void testAuditedModernRawIdsAreExplicitlyClassified() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy unknownSpy(&handler,
                              &oaa::hu::AudioChannelHandler::unknownMessage);

        const QByteArray opaque("\x08\x01", 2);
        handler.onMessage(0x8014, opaque);
        QCOMPARE(unknownSpy.count(), 0);

        handler.onMessage(0x8010, opaque);
        QCOMPARE(unknownSpy.count(), 1);
        QCOMPARE(unknownSpy[0][0].value<uint16_t>(), uint16_t{0x8010});
    }

    void testStateResetsOnChannelClose() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        handler.onChannelOpened();

        // Start a stream
        oaa::proto::messages::AVChannelStartIndication start;
        start.set_session(1);
        start.set_config(0);
        QByteArray sp(start.ByteSizeLong(), '\0');
        start.SerializeToArray(sp.data(), sp.size());
        handler.onMessage(oaa::AVMessageId::START_INDICATION, sp);
        QVERIFY(handler.canAcceptMedia());

        // Close channel — state should reset
        handler.onChannelClosed();
        QVERIFY(!handler.canAcceptMedia());

        // Re-open — should be clean
        handler.onChannelOpened();
        QVERIFY(!handler.canAcceptMedia());
    }
};

QTEST_MAIN(TestAudioChannelHandler)
#include "test_audio_channel_handler.moc"
