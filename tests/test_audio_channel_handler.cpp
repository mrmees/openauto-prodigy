#include <QTest>
#include <QSignalSpy>
#include <oaa/HU/Handlers/AudioChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include "oaa/av/AVChannelSetupRequestMessage.pb.h"
#include "oaa/av/MediaCodecTypeEnum.pb.h"
#include "oaa/av/AVChannelStartIndicationMessage.pb.h"
#include "oaa/audio/AudioFocusStateMessage.pb.h"
#include "oaa/audio/AudioStreamTypeMessage.pb.h"
#include "oaa/audio/AudioStreamTypeEnum.pb.h"

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
        req.set_config_index(static_cast<oaa::proto::enums::MediaCodecType_Enum>(0));
        QByteArray payload(req.ByteSizeLong(), '\0');
        req.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::SETUP_REQUEST, payload);

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::SETUP_RESPONSE));
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

        // Send 10 frames to trigger batched ACK (max_unacked=10)
        QByteArray pcmData(960, '\x42');
        for (int i = 0; i < 10; ++i)
            handler.onMediaData(pcmData, 1234567890 + i);

        QCOMPARE(dataSpy.count(), 10);
        QCOMPARE(dataSpy[0][0].toByteArray().size(), 960);

        // Should send one batched ACK after 10 frames
        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::AVMessageId::ACK_INDICATION));
    }

    void testMediaDataIgnoredWhenNotStreaming() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy dataSpy(&handler, &oaa::hu::AudioChannelHandler::audioDataReceived);

        handler.onChannelOpened();
        // Not started — media data should be silently ignored
        handler.onMediaData(QByteArray(960, '\x42'), 0);
        QCOMPARE(dataSpy.count(), 0);
    }

    // --- AudioFocusState (0x8021) tests ---

    void testAudioFocusStateGainEmitsSignal() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy spy(&handler, &oaa::hu::AudioChannelHandler::audioFocusStateChanged);
        handler.onChannelOpened();

        oaa::proto::messages::AudioFocusState msg;
        msg.set_has_focus(true);
        QByteArray payload(msg.ByteSizeLong(), '\0');
        msg.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::AUDIO_FOCUS_STATE, payload);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toBool(), true);
    }

    void testAudioFocusStateLossEmitsSignal() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy spy(&handler, &oaa::hu::AudioChannelHandler::audioFocusStateChanged);
        handler.onChannelOpened();

        // First send focus=true
        oaa::proto::messages::AudioFocusState msg;
        msg.set_has_focus(true);
        QByteArray payload(msg.ByteSizeLong(), '\0');
        msg.SerializeToArray(payload.data(), payload.size());
        handler.onMessage(oaa::AVMessageId::AUDIO_FOCUS_STATE, payload);

        // Then send focus=false
        msg.set_has_focus(false);
        QByteArray payload2(msg.ByteSizeLong(), '\0');
        msg.SerializeToArray(payload2.data(), payload2.size());
        handler.onMessage(oaa::AVMessageId::AUDIO_FOCUS_STATE, payload2);

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy[1][0].toBool(), false);
    }

    void testAudioFocusStateNoChangeGuard() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy spy(&handler, &oaa::hu::AudioChannelHandler::audioFocusStateChanged);
        handler.onChannelOpened();

        oaa::proto::messages::AudioFocusState msg;
        msg.set_has_focus(true);
        QByteArray payload(msg.ByteSizeLong(), '\0');
        msg.SerializeToArray(payload.data(), payload.size());

        // Send same value twice — should only emit once
        handler.onMessage(oaa::AVMessageId::AUDIO_FOCUS_STATE, payload);
        handler.onMessage(oaa::AVMessageId::AUDIO_FOCUS_STATE, payload);

        QCOMPARE(spy.count(), 1);
    }

    void testAudioFocusStateInvalidPayload() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy spy(&handler, &oaa::hu::AudioChannelHandler::audioFocusStateChanged);
        handler.onChannelOpened();

        // Garbage payload that won't parse as protobuf
        // Use bytes that are definitely invalid protobuf (wrong wire type for field 1)
        QByteArray garbage;
        garbage.append('\x0b'); // field 1, wire type 3 (start group) — invalid for bool
        garbage.append('\xff');
        garbage.append('\xff');
        handler.onMessage(oaa::AVMessageId::AUDIO_FOCUS_STATE, garbage);

        QCOMPARE(spy.count(), 0);
    }

    // --- AudioStreamType (0x8022) tests ---

    void testAudioStreamTypeMediaEmitsSignal() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy spy(&handler, &oaa::hu::AudioChannelHandler::audioStreamTypeChanged);
        handler.onChannelOpened();

        oaa::proto::messages::AudioStreamType msg;
        msg.set_stream_type(oaa::proto::enums::AudioStreamType::MEDIA);
        QByteArray payload(msg.ByteSizeLong(), '\0');
        msg.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::AUDIO_STREAM_TYPE, payload);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toInt(), 1);  // MEDIA = 1
    }

    void testAudioStreamTypeGuidanceEmitsSignal() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::SpeechAudio);
        QSignalSpy spy(&handler, &oaa::hu::AudioChannelHandler::audioStreamTypeChanged);
        handler.onChannelOpened();

        oaa::proto::messages::AudioStreamType msg;
        msg.set_stream_type(oaa::proto::enums::AudioStreamType::GUIDANCE);
        QByteArray payload(msg.ByteSizeLong(), '\0');
        msg.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::AVMessageId::AUDIO_STREAM_TYPE, payload);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toInt(), 2);  // GUIDANCE = 2
    }

    void testAudioStreamTypeInvalidPayload() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy spy(&handler, &oaa::hu::AudioChannelHandler::audioStreamTypeChanged);
        handler.onChannelOpened();

        QByteArray garbage;
        garbage.append('\x0b');
        garbage.append('\xff');
        garbage.append('\xff');
        handler.onMessage(oaa::AVMessageId::AUDIO_STREAM_TYPE, garbage);

        QCOMPARE(spy.count(), 0);
    }

    // --- State reset tests ---

    void testStateResetsOnChannelClose() {
        oaa::hu::AudioChannelHandler handler(oaa::ChannelId::MediaAudio);
        QSignalSpy focusSpy(&handler, &oaa::hu::AudioChannelHandler::audioFocusStateChanged);
        handler.onChannelOpened();

        // Set focus to true
        oaa::proto::messages::AudioFocusState focusMsg;
        focusMsg.set_has_focus(true);
        QByteArray fp(focusMsg.ByteSizeLong(), '\0');
        focusMsg.SerializeToArray(fp.data(), fp.size());
        handler.onMessage(oaa::AVMessageId::AUDIO_FOCUS_STATE, fp);
        QCOMPARE(focusSpy.count(), 1);

        // Close channel — state should reset
        handler.onChannelClosed();

        // Re-open and send focus=true again — should emit because state was reset
        handler.onChannelOpened();
        handler.onMessage(oaa::AVMessageId::AUDIO_FOCUS_STATE, fp);
        QCOMPARE(focusSpy.count(), 2);  // emitted again after reset
    }
};

QTEST_MAIN(TestAudioChannelHandler)
#include "test_audio_channel_handler.moc"
