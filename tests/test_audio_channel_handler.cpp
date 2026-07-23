#include <QTest>
#include <QSignalSpy>
#include <oaa/HU/Handlers/AudioChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include "oaa/av/AVChannelSetupRequestMessage.pb.h"
#include "oaa/av/AVChannelSetupResponseMessage.pb.h"
#include "oaa/av/AVMediaAckIndicationMessage.pb.h"
#include "oaa/av/MediaCodecTypeEnum.pb.h"
#include "oaa/av/AVChannelStartIndicationMessage.pb.h"

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

        handler.onChannelOpened();

        oaa::proto::messages::AVChannelStartIndication start;
        start.set_session(42);
        start.set_config(0);
        QByteArray payload(start.ByteSizeLong(), '\0');
        start.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::START_INDICATION, payload);

        QCOMPARE(startSpy.count(), 1);
        QCOMPARE(startSpy[0][0].value<int32_t>(), 42);
        QVERIFY(handler.canAcceptMedia());
    }

    void testMediaDataEmitsSignalAndAck() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
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

        QCOMPARE(sendSpy.count(), 3);
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
