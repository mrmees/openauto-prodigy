#include <QTest>
#include <QSignalSpy>
#include "core/aa/handlers/AudioChannelHandler.hpp"
#include <oaa/Channel/ChannelId.hpp>
#include "AVChannelSetupRequestMessage.pb.h"
#include "AVChannelStartIndicationMessage.pb.h"

class TestAudioChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testMediaChannelId() {
        oap::aa::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QCOMPARE(handler.channelId(), oaa::ChannelId::MediaAudio);
    }

    void testSpeechChannelId() {
        oap::aa::AudioChannelHandler handler(oaa::ChannelId::SpeechAudio);
        QCOMPARE(handler.channelId(), oaa::ChannelId::SpeechAudio);
    }

    void testSystemChannelId() {
        oap::aa::AudioChannelHandler handler(oaa::ChannelId::SystemAudio);
        QCOMPARE(handler.channelId(), oaa::ChannelId::SystemAudio);
    }

    void testAVSetupRequestResponds() {
        oap::aa::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.onChannelOpened();

        oaa::proto::messages::AVChannelSetupRequest req;
        req.set_config_index(0);
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::SETUP_REQUEST, payload);

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::SETUP_RESPONSE));
    }

    void testStartIndicationEmitsSignal() {
        oap::aa::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy startSpy(&handler, &oap::aa::AudioChannelHandler::streamStarted);

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
        oap::aa::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        handler.onChannelOpened();

        // Start the stream
        oaa::proto::messages::AVChannelStartIndication start;
        start.set_session(1);
        start.set_config(0);
        QByteArray startPayload(start.ByteSizeLong(), '\0');
        start.SerializeToArray(startPayload.data(), startPayload.size());
        handler.onMessage(oaa::AVMessageId::START_INDICATION, startPayload);

        QSignalSpy dataSpy(&handler, &oap::aa::AudioChannelHandler::audioDataReceived);
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        QByteArray pcmData(960, '\x42');
        handler.onMediaData(pcmData, 1234567890);

        QCOMPARE(dataSpy.count(), 1);
        QCOMPARE(dataSpy[0][0].toByteArray().size(), 960);

        // Should send ACK
        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::ACK_INDICATION));
    }

    void testMediaDataIgnoredWhenNotStreaming() {
        oap::aa::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy dataSpy(&handler, &oap::aa::AudioChannelHandler::audioDataReceived);

        handler.onChannelOpened();
        // Not started — media data should be silently ignored
        handler.onMediaData(QByteArray(960, '\x42'), 0);
        QCOMPARE(dataSpy.count(), 0);
    }
};

QTEST_MAIN(TestAudioChannelHandler)
#include "test_audio_channel_handler.moc"
