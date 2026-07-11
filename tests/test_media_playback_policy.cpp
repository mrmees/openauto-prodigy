// Locks the stage-1 bench-hardened playback policy (design §10/§11):
// no-audio track-end counts as unplayable; 3 strikes stops; restore
// failure stays stopped; only user actions clear restoring.
#include <QtTest>
#include "plugins/media_player/PlaybackPolicy.hpp"

using oap::plugins::PlaybackPolicy;

class TestMediaPlaybackPolicy : public QObject {
    Q_OBJECT
private slots:
    void trackEndWithAudioAdvances() {
        PlaybackPolicy p;
        p.onTrackStarted();
        p.onProgress(4200);
        QCOMPARE(p.onTrackFinished(), PlaybackPolicy::TrackEndVerdict::Advance);
    }
    void trackEndWithoutAudioIsUnplayable() {
        PlaybackPolicy p;
        p.onTrackStarted();
        p.onProgress(120);  // below the 500 ms audibility watermark
        QCOMPARE(p.onTrackFinished(), PlaybackPolicy::TrackEndVerdict::Unplayable);
    }
    void watermarkResetsPerTrack() {
        PlaybackPolicy p;
        p.onTrackStarted();
        p.onProgress(9000);
        p.onTrackStarted();  // next track
        QCOMPARE(p.onTrackFinished(), PlaybackPolicy::TrackEndVerdict::Unplayable);
    }
    void threeStrikesStops() {
        PlaybackPolicy p;
        QCOMPARE(p.onUnplayableEdge(), PlaybackPolicy::UnplayableVerdict::SkipNext);
        QCOMPARE(p.onUnplayableEdge(), PlaybackPolicy::UnplayableVerdict::SkipNext);
        QCOMPARE(p.onUnplayableEdge(), PlaybackPolicy::UnplayableVerdict::StopAndNotify);
        // Counter survives the verdict so the caller can toast the exact
        // stage-1 message ("... 3 unplayable files in a row"), THEN resets.
        QCOMPARE(p.consecutiveErrors(), 3);
        p.resetStrikes();
        QCOMPARE(p.consecutiveErrors(), 0);
        QCOMPARE(p.onUnplayableEdge(), PlaybackPolicy::UnplayableVerdict::SkipNext);
    }
    void watermarkBoundaryIsExact() {
        // pos == 500 exactly: playable end (not < 500) but strikes NOT
        // cleared (not > 500) — locks both comparisons.
        PlaybackPolicy p;
        p.onUnplayableEdge();
        p.onTrackStarted();
        p.onProgress(500);
        QCOMPARE(p.onTrackFinished(), PlaybackPolicy::TrackEndVerdict::Advance);
        QCOMPARE(p.consecutiveErrors(), 1);
    }
    void audibleProgressClearsStrikes() {
        PlaybackPolicy p;
        p.onUnplayableEdge();
        p.onUnplayableEdge();
        p.onProgress(501);           // decode demonstrably working
        p.onUnplayableEdge();
        QCOMPARE(p.onUnplayableEdge(), PlaybackPolicy::UnplayableVerdict::SkipNext);
    }
    void restoreFailureStaysStopped() {
        PlaybackPolicy p;
        p.onRestoreBegan();
        QCOMPARE(p.onUnplayableEdge(), PlaybackPolicy::UnplayableVerdict::StayStopped);
        QVERIFY(!p.restoring());     // cleared by the failed edge
        QCOMPARE(p.onUnplayableEdge(), PlaybackPolicy::UnplayableVerdict::SkipNext);
    }
    void progressDoesNotClearRestoring() {
        // Bench 2026-07-09 row 11 addendum: restore-seek echoes position
        // before any decode; only user actions may clear restoring.
        PlaybackPolicy p;
        p.onRestoreBegan();
        p.onProgress(4200);
        QVERIFY(p.restoring());
        QVERIFY(!p.saveAllowed());
        p.onUserAction();
        QVERIFY(!p.restoring());
        QVERIFY(p.saveAllowed());
    }
    void shutdownBlocksSaves() {
        PlaybackPolicy p;
        QVERIFY(p.saveAllowed());
        p.onShutdownBegan();
        QVERIFY(!p.saveAllowed());
    }
    void newQueueClearsStrikes() {
        PlaybackPolicy p;
        p.onUnplayableEdge();
        p.onNewQueue();
        p.onUnplayableEdge();
        QCOMPARE(p.consecutiveErrors(), 1);
    }
};

QTEST_APPLESS_MAIN(TestMediaPlaybackPolicy)
#include "test_media_playback_policy.moc"
