// tests/test_media_status_service.cpp
// Playing-wins arbitration across AA / BT / local MediaPlayer (spec 2026-07-08 §6).
#include <QtTest/QtTest>
#include "core/services/MediaStatusService.hpp"
#include "core/services/IMediaStatusProvider.hpp"

class TestMediaStatusService : public QObject {
    Q_OBJECT
private slots:
    void testInitialStateIsEmpty();
    void testConnectIdleGrantsDisplayByRecency();
    void testPlayingSourceWinsOverConnectedIdle();
    void testMostRecentPlayStartBreaksTies();
    void testPauseKeepsDisplayUntilAnotherPlays();
    void testDisconnectFallsBackToMostRecentConnected();
    void testAaConnectDoesNotStealFromPlayingLocal();
    void testIsPlayingNormalizationPerSource();
    void testProgressFieldsFlowForBtAndMediaPlayer();
    void testAaHasNoPosition();
    void testArtUrlOnlyFromMediaPlayer();
    void testPlaybackControlsDelegate();
    void testSignalEmittedOnActiveMetadataChange();
    void testInactiveSourceUpdatesAreSilent();
};

void TestMediaStatusService::testInitialStateIsEmpty() {
    oap::MediaStatusService s;
    QCOMPARE(s.source(), QString());
    QVERIFY(s.title().isEmpty());
    QCOMPARE(s.playbackState(), 0);
    QVERIFY(!s.isPlaying());
    QVERIFY(!s.hasPosition());
    QCOMPARE(s.position(), qint64(-1));
    QCOMPARE(s.duration(), qint64(0));
    QVERIFY(s.artUrl().isEmpty());
}

void TestMediaStatusService::testConnectIdleGrantsDisplayByRecency() {
    oap::MediaStatusService s;
    s.setBtConnected(true);
    QCOMPARE(s.source(), QString("Bluetooth"));
    s.setAaConnected(true);            // nothing playing; BT keeps display (rule 2)
    QCOMPARE(s.source(), QString("Bluetooth"));
    s.setBtConnected(false);           // BT gone -> most recent connected = AA
    QCOMPARE(s.source(), QString("AndroidAuto"));
}

void TestMediaStatusService::testPlayingSourceWinsOverConnectedIdle() {
    oap::MediaStatusService s;
    s.setAaConnected(true);
    s.updateAaMetadata("AA Song", "AA Artist", "AA Album");
    s.setMediaPlayerConnected(true);
    s.updateMediaPlayerMetadata("Local Song", "Local Artist", "Local Album");
    s.updateMediaPlayerPlaybackState(1);   // local playing (MP raw 1 = playing)
    QCOMPARE(s.source(), QString("MediaPlayer"));
    QCOMPARE(s.title(), QString("Local Song"));
}

void TestMediaStatusService::testMostRecentPlayStartBreaksTies() {
    oap::MediaStatusService s;
    s.setBtConnected(true);
    s.updateBtPlaybackState(1);            // BT playing
    QCOMPARE(s.source(), QString("Bluetooth"));
    s.setMediaPlayerConnected(true);
    s.updateMediaPlayerPlaybackState(1);   // local starts later -> wins
    QCOMPARE(s.source(), QString("MediaPlayer"));
    s.updateBtPlaybackState(2);            // BT pauses
    s.updateBtPlaybackState(1);            // BT starts again -> most recent
    QCOMPARE(s.source(), QString("Bluetooth"));
}

void TestMediaStatusService::testPauseKeepsDisplayUntilAnotherPlays() {
    oap::MediaStatusService s;
    s.setMediaPlayerConnected(true);
    s.updateMediaPlayerPlaybackState(1);
    s.setBtConnected(true);
    QCOMPARE(s.source(), QString("MediaPlayer"));
    s.updateMediaPlayerPlaybackState(2);   // paused; nothing else playing
    QCOMPARE(s.source(), QString("MediaPlayer"));  // rule 2: keeps display
    s.updateBtPlaybackState(1);            // BT starts playing -> takes over
    QCOMPARE(s.source(), QString("Bluetooth"));
}

void TestMediaStatusService::testDisconnectFallsBackToMostRecentConnected() {
    oap::MediaStatusService s;
    s.setBtConnected(true);
    s.setMediaPlayerConnected(true);
    s.updateMediaPlayerPlaybackState(1);
    s.updateMediaPlayerPlaybackState(2);
    QCOMPARE(s.source(), QString("MediaPlayer"));
    s.setMediaPlayerConnected(false);      // local queue cleared
    QCOMPARE(s.source(), QString("Bluetooth"));
    s.setBtConnected(false);
    QCOMPARE(s.source(), QString());
}

void TestMediaStatusService::testAaConnectDoesNotStealFromPlayingLocal() {
    oap::MediaStatusService s;
    s.setMediaPlayerConnected(true);
    s.updateMediaPlayerMetadata("Local Song", "", "");
    s.updateMediaPlayerPlaybackState(1);
    s.setAaConnected(true);                // phone connects for nav
    QCOMPARE(s.source(), QString("MediaPlayer"));  // music keeps the display
    s.updateAaPlaybackState(2, "Spotify"); // phone actually plays -> AA wins
    QCOMPARE(s.source(), QString("AndroidAuto"));
    QCOMPARE(s.appName(), QString("Spotify"));
}

void TestMediaStatusService::testIsPlayingNormalizationPerSource() {
    oap::MediaStatusService s;
    // BT: raw 1 = playing
    s.setBtConnected(true);
    s.updateBtPlaybackState(1);
    QVERIFY(s.isPlaying());
    s.updateBtPlaybackState(2);
    QVERIFY(!s.isPlaying());
    s.setBtConnected(false);
    // AA: raw 2 = playing (raw 1 is STOPPED — the old widget bug)
    s.setAaConnected(true);
    s.updateAaPlaybackState(1, "App");
    QVERIFY(!s.isPlaying());
    s.updateAaPlaybackState(2, "App");
    QVERIFY(s.isPlaying());
    s.setAaConnected(false);
    // MediaPlayer: raw 1 = playing
    s.setMediaPlayerConnected(true);
    s.updateMediaPlayerPlaybackState(1);
    QVERIFY(s.isPlaying());
}

void TestMediaStatusService::testProgressFieldsFlowForBtAndMediaPlayer() {
    oap::MediaStatusService s;
    s.setMediaPlayerConnected(true);
    s.updateMediaPlayerPlaybackState(1);
    QSignalSpy spy(&s, &oap::IMediaStatusProvider::progressChanged);
    s.updateMediaPlayerProgress(61000, 245000);
    QCOMPARE(s.position(), qint64(61000));
    QCOMPARE(s.duration(), qint64(245000));
    QVERIFY(s.hasPosition());
    QVERIFY(spy.count() >= 1);

    s.setBtConnected(true);
    s.updateBtPlaybackState(1);            // BT takes over
    s.updateBtProgress(5000, 180000);
    QCOMPARE(s.position(), qint64(5000));
    QCOMPARE(s.duration(), qint64(180000));
    QVERIFY(s.hasPosition());
}

void TestMediaStatusService::testAaHasNoPosition() {
    oap::MediaStatusService s;
    s.setAaConnected(true);
    s.updateAaPlaybackState(2, "App");
    QVERIFY(s.isPlaying());
    QVERIFY(!s.hasPosition());
    QCOMPARE(s.position(), qint64(-1));
}

void TestMediaStatusService::testArtUrlOnlyFromMediaPlayer() {
    oap::MediaStatusService s;
    s.setMediaPlayerConnected(true);
    s.updateMediaPlayerPlaybackState(1);
    s.updateMediaPlayerArt("image://mediaart/current/3");
    QCOMPARE(s.artUrl(), QString("image://mediaart/current/3"));
    s.setBtConnected(true);
    s.updateBtPlaybackState(1);            // BT active now
    QVERIFY(s.artUrl().isEmpty());         // BT has no art
}

void TestMediaStatusService::testPlaybackControlsDelegate() {
    oap::MediaStatusService s;
    bool play = false, next = false, prev = false;
    s.setPlaybackCallbacks([&] { play = true; }, [&] { next = true; }, [&] { prev = true; });
    s.playPause();
    s.next();
    s.previous();
    QVERIFY(play); QVERIFY(next); QVERIFY(prev);
}

void TestMediaStatusService::testSignalEmittedOnActiveMetadataChange() {
    oap::MediaStatusService s;
    s.setBtConnected(true);
    QSignalSpy spy(&s, &oap::IMediaStatusProvider::mediaStatusChanged);
    s.updateBtMetadata("Song", "Artist", "Album");
    QVERIFY(spy.count() >= 1);
}

void TestMediaStatusService::testInactiveSourceUpdatesAreSilent() {
    oap::MediaStatusService s;
    s.setMediaPlayerConnected(true);
    s.updateMediaPlayerPlaybackState(1);   // MediaPlayer active
    QSignalSpy spy(&s, &oap::IMediaStatusProvider::mediaStatusChanged);
    s.updateAaMetadata("AA Song", "x", "y");   // AA not even connected
    QCOMPARE(spy.count(), 0);
    QCOMPARE(s.title(), QString());        // MediaPlayer metadata unset, AA cached silently
}

QTEST_GUILESS_MAIN(TestMediaStatusService)
#include "test_media_status_service.moc"
