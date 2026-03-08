#include <QTest>
#include <QSignalSpy>
#include <oaa/HU/Handlers/MediaStatusChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>
#include "oaa/media/MediaPlaybackStatusMessage.pb.h"
#include "oaa/media/MediaPlaybackMetadataMessage.pb.h"

class TestMediaStatusChannelHandler : public QObject {
    Q_OBJECT
private slots:
    void testChannelId() {
        oaa::hu::MediaStatusChannelHandler handler;
        QCOMPARE(handler.channelId(), oaa::ChannelId::MediaStatus);
    }

    void testPlaybackStatusPlaying() {
        oaa::hu::MediaStatusChannelHandler handler;
        QSignalSpy spy(&handler, &oaa::hu::MediaStatusChannelHandler::playbackStateChanged);

        oaa::proto::messages::MediaPlaybackStatus status;
        status.set_playback_state(oaa::proto::messages::PLAYBACK_STATE_PLAYING);
        status.set_source_app("Spotify");
        QByteArray payload(status.ByteSizeLong(), '\0');
        status.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::MediaStatusMessageId::PLAYBACK_STATUS, payload);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toInt(), static_cast<int>(oaa::hu::MediaStatusChannelHandler::Playing));
        QCOMPARE(spy[0][1].toString(), QString("Spotify"));
    }

    void testPlaybackStatusPaused() {
        oaa::hu::MediaStatusChannelHandler handler;
        QSignalSpy spy(&handler, &oaa::hu::MediaStatusChannelHandler::playbackStateChanged);

        oaa::proto::messages::MediaPlaybackStatus status;
        status.set_playback_state(oaa::proto::messages::PLAYBACK_STATE_PAUSED);
        status.set_source_app("YouTube Music");
        QByteArray payload(status.ByteSizeLong(), '\0');
        status.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::MediaStatusMessageId::PLAYBACK_STATUS, payload);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toInt(), static_cast<int>(oaa::hu::MediaStatusChannelHandler::Paused));
    }

    void testPlaybackMetadata() {
        oaa::hu::MediaStatusChannelHandler handler;
        QSignalSpy spy(&handler, &oaa::hu::MediaStatusChannelHandler::metadataChanged);

        oaa::proto::messages::MediaPlaybackMetadata meta;
        meta.set_title("Test Song");
        meta.set_artist("Test Artist");
        meta.set_album("Test Album");
        meta.set_album_art(std::string("\x89PNG\x0d\x0a", 6));
        QByteArray payload(meta.ByteSizeLong(), '\0');
        meta.SerializeToArray(payload.data(), payload.size());

        handler.onMessage(oaa::MediaStatusMessageId::PLAYBACK_METADATA, payload);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].toString(), QString("Test Song"));
        QCOMPARE(spy[0][1].toString(), QString("Test Artist"));
        QCOMPARE(spy[0][2].toString(), QString("Test Album"));
        QCOMPARE(spy[0][3].toByteArray().size(), 6);
    }

    void testInvalidPayloadNoCrash() {
        oaa::hu::MediaStatusChannelHandler handler;
        QSignalSpy spy(&handler, &oaa::hu::MediaStatusChannelHandler::playbackStateChanged);

        QByteArray garbage("\x0b\xff\xff", 3);
        handler.onMessage(oaa::MediaStatusMessageId::PLAYBACK_STATUS, garbage);

        // Proto2 is lenient — may or may not parse. Key invariant is no crash.
        QVERIFY(spy.count() <= 1);
    }
};

QTEST_MAIN(TestMediaStatusChannelHandler)
#include "test_media_status_channel_handler.moc"
