#include <QtTest/QtTest>
#include <QSet>
#include "plugins/media_player/PlayQueue.hpp"

using oap::plugins::PlayQueue;

class TestPlayQueue : public QObject {
    Q_OBJECT
private slots:
    void testEmptyQueueIsSafe();
    void testSetTracksSelectsStart();
    void testAdvanceLinearAndStopsAtEnd();
    void testRepeatAllWraps();
    void testRepeatOneAutoStaysManualMoves();
    void testRepeatOneManualWrapsAtEnd();
    void testRetreat();
    void testShuffleDeterministicCoversAllCurrentFirst();
    void testShuffleOffRestoresLinear();
    void testJumpToSyncsShuffleOrder();
};

static QStringList tracks5() {
    return {"/m/a.mp3", "/m/b.mp3", "/m/c.mp3", "/m/d.mp3", "/m/e.mp3"};
}

void TestPlayQueue::testEmptyQueueIsSafe() {
    PlayQueue q;
    QCOMPARE(q.count(), 0);
    QCOMPARE(q.currentTrack(), QString());
    QVERIFY(!q.advance(false));
    QVERIFY(!q.advance(true));
    QVERIFY(!q.retreat());
}

void TestPlayQueue::testSetTracksSelectsStart() {
    PlayQueue q;
    QSignalSpy spy(&q, &PlayQueue::currentChanged);
    q.setTracks(tracks5(), 2);
    QCOMPARE(q.count(), 5);
    QCOMPARE(q.currentIndex(), 2);
    QCOMPARE(q.currentTrack(), QString("/m/c.mp3"));
    QVERIFY(spy.count() >= 1);
}

void TestPlayQueue::testAdvanceLinearAndStopsAtEnd() {
    PlayQueue q;
    q.setTracks(tracks5(), 3);
    QVERIFY(q.advance(false));
    QCOMPARE(q.currentTrack(), QString("/m/e.mp3"));
    QVERIFY(!q.advance(false));                       // repeat off: end of queue
    QCOMPARE(q.currentTrack(), QString("/m/e.mp3"));  // stays put
}

void TestPlayQueue::testRepeatAllWraps() {
    PlayQueue q;
    q.setTracks(tracks5(), 4);
    q.setRepeatMode(PlayQueue::RepeatAll);
    QVERIFY(q.advance(false));
    QCOMPARE(q.currentTrack(), QString("/m/a.mp3"));
}

void TestPlayQueue::testRepeatOneAutoStaysManualMoves() {
    PlayQueue q;
    q.setTracks(tracks5(), 1);
    q.setRepeatMode(PlayQueue::RepeatOne);
    QSignalSpy spy(&q, &PlayQueue::currentChanged);
    QVERIFY(q.advance(false));                        // auto: replay same track
    QCOMPARE(q.currentTrack(), QString("/m/b.mp3"));
    QVERIFY(spy.count() >= 1);                        // re-emitted for replay
    QVERIFY(q.advance(true));                         // manual: really moves
    QCOMPARE(q.currentTrack(), QString("/m/c.mp3"));
}

void TestPlayQueue::testRepeatOneManualWrapsAtEnd() {
    PlayQueue q;
    q.setTracks(tracks5(), 4);              // start on the last track
    q.setRepeatMode(PlayQueue::RepeatOne);
    QVERIFY(q.advance(true));               // manual next at queue end: wraps
    QCOMPARE(q.currentTrack(), QString("/m/a.mp3"));
    QVERIFY(q.advance(false));              // auto under RepeatOne: replays
    QCOMPARE(q.currentTrack(), QString("/m/a.mp3"));
}

void TestPlayQueue::testRetreat() {
    PlayQueue q;
    q.setTracks(tracks5(), 1);
    QVERIFY(q.retreat());
    QCOMPARE(q.currentTrack(), QString("/m/a.mp3"));
    QVERIFY(!q.retreat());                            // start, repeat off
    q.setRepeatMode(PlayQueue::RepeatAll);
    QVERIFY(q.retreat());                             // wraps
    QCOMPARE(q.currentTrack(), QString("/m/e.mp3"));
}

void TestPlayQueue::testShuffleDeterministicCoversAllCurrentFirst() {
    PlayQueue q;
    q.setTracks(tracks5(), 2);
    q.setShuffleSeed(1234);
    q.setShuffle(true);
    QCOMPARE(q.currentTrack(), QString("/m/c.mp3"));  // current unchanged
    QSet<QString> seen{q.currentTrack()};
    while (q.advance(false))
        seen.insert(q.currentTrack());
    QCOMPARE(seen.size(), 5);                         // full coverage, no repeats

    // Same seed → same order
    PlayQueue q2;
    q2.setTracks(tracks5(), 2);
    q2.setShuffleSeed(1234);
    q2.setShuffle(true);
    QStringList order1, order2;
    PlayQueue q1b;
    q1b.setTracks(tracks5(), 2);
    q1b.setShuffleSeed(1234);
    q1b.setShuffle(true);
    do { order1 << q1b.currentTrack(); } while (q1b.advance(false));
    do { order2 << q2.currentTrack(); } while (q2.advance(false));
    QCOMPARE(order1, order2);
}

void TestPlayQueue::testShuffleOffRestoresLinear() {
    PlayQueue q;
    q.setTracks(tracks5(), 0);
    q.setShuffleSeed(99);
    q.setShuffle(true);
    q.advance(false);
    const QString cur = q.currentTrack();
    q.setShuffle(false);
    QCOMPARE(q.currentTrack(), cur);                  // current preserved
    const int idx = q.currentIndex();
    if (idx < 4) {
        QVERIFY(q.advance(false));
        QCOMPARE(q.currentIndex(), idx + 1);          // linear again
    }
}

void TestPlayQueue::testJumpToSyncsShuffleOrder() {
    PlayQueue q;
    q.setTracks(tracks5(), 0);
    q.setShuffleSeed(7);
    q.setShuffle(true);
    q.jumpTo(3);
    QCOMPARE(q.currentIndex(), 3);
    QCOMPARE(q.currentTrack(), QString("/m/d.mp3"));
    // A desynced orderPos_ would revisit an already-passed track (including
    // /m/d.mp3 itself) during the remaining walk. Assert no revisits.
    QSet<QString> seen{q.currentTrack()};
    while (q.advance(false)) {
        QVERIFY2(!seen.contains(q.currentTrack()),
                 "advance revisited a track after jumpTo — orderPos_ desync");
        seen.insert(q.currentTrack());
    }
}

QTEST_GUILESS_MAIN(TestPlayQueue)
#include "test_play_queue.moc"
