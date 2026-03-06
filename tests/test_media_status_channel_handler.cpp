#include <QTest>
#include <QSignalSpy>
#include <oaa/HU/Handlers/MediaStatusChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>
#include "oaa/media/MediaPlaybackCommandMessage.pb.h"
#include "oaa/media/MediaPlaybackStatusMessage.pb.h"

class TestMediaStatusChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testSendPlaybackCommandPause() {
        oaa::hu::MediaStatusChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        QSignalSpy cmdSpy(&handler, &oaa::hu::MediaStatusChannelHandler::playbackCommandSent);

        handler.sendPlaybackCommand(
            static_cast<int>(oaa::proto::messages::PLAYBACK_COMMAND_PAUSE));

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(sendSpy[0][0].value<uint8_t>(), oaa::ChannelId::MediaStatus);
        QCOMPARE(sendSpy[0][1].value<uint16_t>(),
                 static_cast<uint16_t>(oaa::MediaStatusMessageId::PLAYBACK_COMMAND));

        // Deserialize and verify payload
        QByteArray payload = sendSpy[0][2].toByteArray();
        oaa::proto::messages::MediaPlaybackCommand cmd;
        QVERIFY(cmd.ParseFromArray(payload.constData(), payload.size()));
        QCOMPARE(cmd.command(), oaa::proto::messages::PLAYBACK_COMMAND_PAUSE);

        // Signal should have been emitted
        QCOMPARE(cmdSpy.count(), 1);
        QCOMPARE(cmdSpy[0][0].toInt(),
                 static_cast<int>(oaa::proto::messages::PLAYBACK_COMMAND_PAUSE));
    }

    void testSendPlaybackCommandResume() {
        oaa::hu::MediaStatusChannelHandler handler;
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);

        handler.sendPlaybackCommand(
            static_cast<int>(oaa::proto::messages::PLAYBACK_COMMAND_RESUME));

        QCOMPARE(sendSpy.count(), 1);

        QByteArray payload = sendSpy[0][2].toByteArray();
        oaa::proto::messages::MediaPlaybackCommand cmd;
        QVERIFY(cmd.ParseFromArray(payload.constData(), payload.size()));
        QCOMPARE(cmd.command(), oaa::proto::messages::PLAYBACK_COMMAND_RESUME);
    }

    void testTogglePlaybackWhenPlaying() {
        oaa::hu::MediaStatusChannelHandler handler;

        // Simulate incoming PlaybackStatus with state=Playing (2)
        oaa::proto::messages::MediaPlaybackStatus status;
        status.set_playback_state(oaa::proto::messages::PLAYBACK_STATE_PLAYING);
        status.set_source_app("TestApp");
        QByteArray statusPayload(status.ByteSizeLong(), '\0');
        status.SerializeToArray(statusPayload.data(), statusPayload.size());
        handler.onMessage(oaa::MediaStatusMessageId::PLAYBACK_STATUS, statusPayload);

        // Now toggle -- should send PAUSE
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        handler.togglePlayback();

        QCOMPARE(sendSpy.count(), 1);
        QByteArray payload = sendSpy[0][2].toByteArray();
        oaa::proto::messages::MediaPlaybackCommand cmd;
        QVERIFY(cmd.ParseFromArray(payload.constData(), payload.size()));
        QCOMPARE(cmd.command(), oaa::proto::messages::PLAYBACK_COMMAND_PAUSE);
    }

    void testTogglePlaybackWhenPaused() {
        oaa::hu::MediaStatusChannelHandler handler;

        // Simulate incoming PlaybackStatus with state=Paused (3)
        oaa::proto::messages::MediaPlaybackStatus status;
        status.set_playback_state(oaa::proto::messages::PLAYBACK_STATE_PAUSED);
        status.set_source_app("TestApp");
        QByteArray statusPayload(status.ByteSizeLong(), '\0');
        status.SerializeToArray(statusPayload.data(), statusPayload.size());
        handler.onMessage(oaa::MediaStatusMessageId::PLAYBACK_STATUS, statusPayload);

        // Now toggle -- should send RESUME
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        handler.togglePlayback();

        QCOMPARE(sendSpy.count(), 1);
        QByteArray payload = sendSpy[0][2].toByteArray();
        oaa::proto::messages::MediaPlaybackCommand cmd;
        QVERIFY(cmd.ParseFromArray(payload.constData(), payload.size()));
        QCOMPARE(cmd.command(), oaa::proto::messages::PLAYBACK_COMMAND_RESUME);
    }

    void testTogglePlaybackDefaultState() {
        oaa::hu::MediaStatusChannelHandler handler;

        // No incoming status -- default state is 0 (UNKNOWN/not playing)
        // Should send RESUME
        QSignalSpy sendSpy(&handler, &oaa::IChannelHandler::sendRequested);
        handler.togglePlayback();

        QCOMPARE(sendSpy.count(), 1);
        QByteArray payload = sendSpy[0][2].toByteArray();
        oaa::proto::messages::MediaPlaybackCommand cmd;
        QVERIFY(cmd.ParseFromArray(payload.constData(), payload.size()));
        QCOMPARE(cmd.command(), oaa::proto::messages::PLAYBACK_COMMAND_RESUME);
    }
};

QTEST_MAIN(TestMediaStatusChannelHandler)
#include "test_media_status_channel_handler.moc"
