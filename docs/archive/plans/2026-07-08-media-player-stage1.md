# Local Media Player — Stage 1 Implementation Plan

Status: COMPLETED 2026-07-09 (bench complete, Pi-deployed)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A playable, fully integrated local media player plugin — folder-browse playback from `~/Music` and mounted USB, PCM routed through AudioService (EQ/volume/ducking), 3-source playing-wins arbitration, widened now-playing surface (position/duration/art), API v1 `LOCAL_MEDIA` source, upgraded NowPlayingWidget.

**Architecture:** New static plugin `MediaPlayerPlugin` (`org.openauto.media-player`) composed of three units: `PlaybackEngine` (QMediaPlayer + Qt 6.8 `QAudioBufferOutput` PCM tap → `AudioService::writeAudio`, same mechanics as AA media audio), `PlayQueue` (pure queue logic), `FolderModel` (filesystem browse). `MediaStatusService` is rewritten from a hard-coded AA>BT switch to a 3-source playing-wins arbiter; `IMediaStatusProvider` widens additively (position/duration/hasPosition/artUrl/isPlaying).

**Tech Stack:** Qt 6.8.2 (Core, Multimedia, Quick, Test), PipeWire via existing AudioService, protobuf (External API v1), QtQuick QML, CMake.

**Spec:** `docs/superpowers/specs/2026-07-08-media-player-design.md` (approved 2026-07-08). Stage 2 (library scanner, art cache, udisks2 automount) is planned separately after stage-1 Pi verification.

## Global Constraints

- Branch: all work on `develop`; commit per task; do NOT push (multi-machine repo — push only after review passes).
- `proto/api/` is **additive-only**: never renumber or retype existing fields; new fields only consume documented reserved slots and must shrink the `reserved` declaration accordingly.
- `libs/prodigy-oaa-protocol/` (open-android-auto submodule) is **read-only**. Never modify.
- Playback-state raw ints are **source-native and never interchangeable**: BT 0=Stopped/1=Playing/2=Paused; AA 1=Stopped/2=Playing/3=Paused; MediaPlayer (new, this plan) 0=Stopped/1=Playing/2=Paused. Always map per-source; never `static_cast` between them.
- Source strings are exact: `"AndroidAuto"`, `"Bluetooth"`, `"MediaPlayer"`, `""` (none).
- `media.playPause` / `media.next` / `media.previous` are the only media actions (host-owned, registered in `main.cpp` — do not register actions from the plugin).
- WebEngine must not be involved anywhere (design-philosophy §8: media playback is native core).
- Dev build/test on WSL2 Debian Trixie: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`. Pi deploys use `./cross-build.sh` + rsync (never build on the Pi unless cross-build breaks).
- Follow existing idiom: `oap::plugins` namespace for plugins, `.hpp/.cpp` pairs, Q_PROPERTY with NOTIFY signals, `qCInfo(lcXyz)` logging categories.
- QTimer only works on threads with a Qt event loop; marshal cross-thread work with `QMetaObject::invokeMethod(obj, lambda, Qt::QueuedConnection)`.

## File Map

**Created:**

| File | Responsibility |
|------|----------------|
| `tools/spike-qmp-tap/main.cpp` + `CMakeLists.txt` | Task 1 spike harness (standalone, kept for reference) |
| `src/plugins/media_player/PlayQueue.hpp/.cpp` | Queue semantics: order, shuffle, repeat |
| `src/plugins/media_player/FolderModel.hpp/.cpp` | Filesystem browse model (roots → dirs → audio files) |
| `src/plugins/media_player/PlaybackEngine.hpp/.cpp` | QMediaPlayer + PCM tap + AudioService stream + focus |
| `src/plugins/media_player/MediaArtProvider.hpp/.cpp` | QQuickImageProvider for current-track embedded art |
| `src/plugins/media_player/MediaPlayerPlugin.hpp/.cpp` | IPlugin glue: composition, persistence, QML surface |
| `qml/applications/media_player/MediaPlayerView.qml` | Folder browse + bottom now-playing bar |
| `tests/test_play_queue.cpp`, `tests/test_folder_model.cpp`, `tests/test_playback_engine.cpp`, `tests/test_media_art_provider.cpp` | Unit tests |
| `tests/data/media/tone-44k.mp3`, `tests/data/media/tone-48k.flac` | Tiny generated fixtures (committed) |

**Modified:**

| File | Change |
|------|--------|
| `src/core/services/IMediaStatusProvider.hpp` | +isPlaying/position/duration/hasPosition/artUrl (default-implemented virtuals), +progressChanged signal |
| `src/core/services/MediaStatusService.hpp/.cpp` | 3-source playing-wins rewrite |
| `tests/test_media_status_service.cpp` | Full rewrite: playing-wins matrix |
| `proto/api/media.proto` | `MEDIA_SOURCE_LOCAL_MEDIA = 4`, `position_ms`/`duration_ms`/`has_position` fields 8–10 |
| `src/core/api/ApiSerializers.cpp` | New source branch + progress fields + state mapping |
| `src/core/api/ApiPublishers.cpp` | +1 connect: `progressChanged` → `scheduleEmit()` |
| `tests/test_api_serializers.cpp` | +3 media tests |
| `qml/widgets/NowPlayingWidget.qml` | Art, progress bar, source badge, isPlaying fix |
| `src/main.cpp` | Plugin registration, provider wiring, callback routing, pause-others policy, image provider |
| `src/CMakeLists.txt`, `tests/CMakeLists.txt` | New sources/tests/QML registration |

---

### Task 1: Spike — QAudioBufferOutput real-time pacing (GO/NO-GO gate)

The whole audio design rests on one bet: **QMediaPlayer delivers PCM to a `QAudioBufferOutput` paced by the media clock even when no `QAudioOutput` device sink is attached.** If it decodes as-fast-as-possible instead, the tap approach needs a crutch or the fallback. Nothing else in Tasks 2–5 depends on this, but Task 6 (PlaybackEngine) does — run this first.

**Files:**
- Create: `tools/spike-qmp-tap/main.cpp`
- Create: `tools/spike-qmp-tap/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from the repo (standalone Qt6 console app).
- Produces: a GO/NO-GO verdict recorded in `docs/session-handoffs.md`; determines whether Task 6 uses the tap as written, the muted-sink crutch, or the direct-PipeWire fallback.

- [ ] **Step 1: Write the spike harness**

`tools/spike-qmp-tap/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.22)
project(spike-qmp-tap CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 REQUIRED COMPONENTS Core Multimedia)
add_executable(spike-qmp-tap main.cpp)
target_link_libraries(spike-qmp-tap PRIVATE Qt6::Core Qt6::Multimedia)
```

`tools/spike-qmp-tap/main.cpp`:

```cpp
// Spike: does QMediaPlayer pace QAudioBufferOutput delivery in real time
// with NO QAudioOutput attached? Also checks: format conversion to
// 48kHz/S16/stereo, position()/duration() advance, seek, EndOfMedia.
//
// Usage: spike-qmp-tap <audiofile> [seekToMs]
// GO criteria: see plan Task 1 Step 4.
#include <QAudioBuffer>
#include <QAudioBufferOutput>
#include <QAudioFormat>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QTimer>
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) { fprintf(stderr, "usage: spike-qmp-tap <audiofile> [seekToMs]\n"); return 2; }
    const QString file = QString::fromLocal8Bit(argv[1]);
    const qint64 seekMs = argc > 2 ? atoll(argv[2]) : -1;

    QAudioFormat fmt;
    fmt.setSampleRate(48000);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Int16);

    QMediaPlayer player;
    QAudioBufferOutput tap(fmt);
    player.setAudioBufferOutput(&tap);

    QElapsedTimer wall;
    wall.start();
    qint64 pcmBytes = 0;
    qint64 firstBufWallMs = -1;
    int bufCount = 0;
    bool formatOk = true;

    QObject::connect(&tap, &QAudioBufferOutput::audioBufferReceived,
                     [&](const QAudioBuffer& buf) {
        if (firstBufWallMs < 0) firstBufWallMs = wall.elapsed();
        pcmBytes += buf.byteCount();
        if (buf.format().sampleRate() != 48000 || buf.format().channelCount() != 2
            || buf.format().sampleFormat() != QAudioFormat::Int16) {
            formatOk = false;
            printf("FORMAT MISMATCH: got %d Hz / %d ch / fmt %d\n",
                   buf.format().sampleRate(), buf.format().channelCount(),
                   (int)buf.format().sampleFormat());
        }
        const double mediaS = double(pcmBytes) / (48000.0 * 2 * 2);
        const double wallS  = double(wall.elapsed() - firstBufWallMs) / 1000.0;
        if (++bufCount % 20 == 0)
            printf("buf#%-4d media=%6.2fs wall=%6.2fs ratio=%.2f playerPos=%lld ms\n",
                   bufCount, mediaS, wallS, wallS > 0.05 ? mediaS / wallS : 0.0,
                   (long long)player.position());
    });

    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged,
                     [&](QMediaPlayer::MediaStatus s) {
        printf("mediaStatus=%d pos=%lld dur=%lld title='%s'\n", (int)s,
               (long long)player.position(), (long long)player.duration(),
               qPrintable(player.metaData().value(QMediaMetaData::Title).toString()));
        if (s == QMediaPlayer::EndOfMedia) {
            const double mediaS = double(pcmBytes) / (48000.0 * 2 * 2);
            const double wallS  = double(wall.elapsed() - firstBufWallMs) / 1000.0;
            printf("RESULT: pcm=%.2fs wall=%.2fs ratio=%.2f formatOk=%d bufs=%d\n",
                   mediaS, wallS, wallS > 0.05 ? mediaS / wallS : 0.0,
                   formatOk ? 1 : 0, bufCount);
            app.quit();
        }
    });

    QObject::connect(&player, &QMediaPlayer::errorOccurred,
                     [&](QMediaPlayer::Error e, const QString& msg) {
        fprintf(stderr, "ERROR %d: %s\n", (int)e, qPrintable(msg));
        app.exit(1);
    });

    player.setSource(QUrl::fromLocalFile(file));
    player.play();

    if (seekMs >= 0) {
        QTimer::singleShot(2000, &player, [&] {
            printf("SEEK -> %lld ms (pos before: %lld)\n",
                   (long long)seekMs, (long long)player.position());
            player.setPosition(seekMs);
        });
        QTimer::singleShot(3000, &player, [&] {
            printf("pos 1s after seek: %lld ms\n", (long long)player.position());
        });
    }

    QTimer::singleShot(90000, &app, [&] { fprintf(stderr, "TIMEOUT\n"); app.exit(3); });
    return app.exec();
}
```

- [ ] **Step 2: Generate throwaway test tones (scratchpad, NOT committed)**

```bash
SCRATCH=/tmp/claude-spike-media && mkdir -p $SCRATCH
ffmpeg -y -f lavfi -i "sine=frequency=440:duration=20" -ar 44100 -ac 2 \
  -metadata title="Spike Tone 44" -metadata artist="Prodigy Bench" \
  -b:a 128k $SCRATCH/tone44.mp3
ffmpeg -y -f lavfi -i "sine=frequency=440:duration=20" -ar 48000 -ac 2 \
  -metadata title="Spike Tone 48" $SCRATCH/tone48.flac
```

Expected: two files created (~300KB mp3, ~1.5MB flac).

- [ ] **Step 3: Build and run the spike (both files, plus a seek run)**

```bash
cmake -S tools/spike-qmp-tap -B /tmp/claude-spike-media/build && cmake --build /tmp/claude-spike-media/build
/tmp/claude-spike-media/build/spike-qmp-tap /tmp/claude-spike-media/tone44.mp3
/tmp/claude-spike-media/build/spike-qmp-tap /tmp/claude-spike-media/tone48.flac
/tmp/claude-spike-media/build/spike-qmp-tap /tmp/claude-spike-media/tone44.mp3 15000
```

- [ ] **Step 4: Interpret against GO criteria**

GO (tap approach works as designed) requires ALL of:
1. **Pacing:** steady-state `ratio` ≈ 1.0 (0.85–1.15) and the 20s file takes ~20s wall — NOT ~1s (that would mean decode-as-fast-as-possible).
2. **Format:** `formatOk=1` for BOTH the 44.1kHz mp3 and 48kHz flac (Qt converts to requested format).
3. **Transport:** `playerPos` advances with wall clock; after `SEEK -> 15000`, position reads ~15000–16200; `duration` ≈ 20000; EndOfMedia fires (~5s after the seek run's jump).
4. **Metadata:** title 'Spike Tone 44' visible in a mediaStatus line.

Decision table:
- **All pass → GO.** Task 6 proceeds as written.
- **Ratio ≪ 1 (decode races ahead):** retest once with a muted device sink added (`QAudioOutput out; out.setVolume(0); player.setAudioOutput(&out);` — 6 lines added to the spike). If that paces correctly, Task 6 adds the muted-sink crutch (one extra member + 2 lines; wasted silent PipeWire stream, acceptable). Record which variant won.
- **Muted-sink also fails, or format conversion broken → NO-GO:** STOP. Report to Matthew before proceeding — Task 6 must be replanned to spec fallback (QMediaPlayer → its own PipeWire stream, no AudioService integration; spec §5 explicitly authorizes this). Tasks 2–5 remain valid either way.

- [ ] **Step 5: Record verdict + commit the spike tool**

Append a dated entry to `docs/session-handoffs.md` summarizing: ratio observed per file, format conversion result, seek behavior, verdict (GO / GO-with-crutch / NO-GO).

```bash
git add tools/spike-qmp-tap/ docs/session-handoffs.md
git commit -m "spike(media): QAudioBufferOutput pacing harness + verdict"
```

---

### Task 2: PlayQueue — queue/shuffle/repeat logic (TDD)

**Files:**
- Create: `src/plugins/media_player/PlayQueue.hpp`
- Create: `src/plugins/media_player/PlayQueue.cpp`
- Create: `tests/test_play_queue.cpp`
- Modify: `src/CMakeLists.txt` (add `plugins/media_player/PlayQueue.cpp` to the `openauto-core` source list, next to `plugins/bt_audio/BtAudioPlugin.cpp`)
- Modify: `tests/CMakeLists.txt` (add `oap_add_test(test_play_queue SOURCES test_play_queue.cpp)` in the Plugin tests section)

**Interfaces:**
- Consumes: nothing (pure Qt Core).
- Produces (used by Task 8): `void setTracks(const QStringList& paths, int startIndex)`, `QString currentTrack() const`, `int currentIndex() const`, `int count() const`, `QStringList tracks() const`, `bool advance(bool manual)`, `bool retreat()`, `void jumpTo(int index)`, `bool shuffle() const` / `void setShuffle(bool)`, `int repeatMode() const` / `void setRepeatMode(int)` (0=off, 1=all, 2=one), `void setShuffleSeed(quint32)` (test hook), signals `currentChanged()`, `queueChanged()`, `shuffleChanged()`, `repeatModeChanged()`.

Semantics (also the test spec):
- `advance(manual=false)` is the end-of-track auto-advance: RepeatOne re-emits `currentChanged` and stays; RepeatAll wraps at end; RepeatOff at end returns `false` without moving.
- `advance(manual=true)` is the user's next button: always moves (RepeatOne behaves like RepeatAll — user intent beats loop).
- `retreat()` moves back; wraps only under RepeatAll; returns `false` at the start otherwise. (The "restart track if >3s in" behavior lives in the plugin, Task 8 — not here.)
- Shuffle builds a Fisher-Yates order with the current track first; disabling restores linear order at the current track's position.

- [ ] **Step 1: Write the failing test**

`tests/test_play_queue.cpp`:

```cpp
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
    QSet<QString> seen{q.currentTrack()};
    while (q.advance(false))
        seen.insert(q.currentTrack());
    QVERIFY(seen.size() >= 1);                        // no crash, coherent walk
}

QTEST_GUILESS_MAIN(TestPlayQueue)
#include "test_play_queue.moc"
```

- [ ] **Step 2: Add test to CMake, run to verify it fails to build**

In `tests/CMakeLists.txt`, after the `test_plugin_model` line, add:

```cmake
oap_add_test(test_play_queue SOURCES test_play_queue.cpp)
```

Run: `cd build && cmake .. && make test_play_queue 2>&1 | tail -5`
Expected: FAIL — `plugins/media_player/PlayQueue.hpp: No such file or directory`.

- [ ] **Step 3: Implement PlayQueue**

`src/plugins/media_player/PlayQueue.hpp`:

```cpp
#pragma once

#include <QObject>
#include <QRandomGenerator>
#include <QStringList>
#include <QVector>

namespace oap {
namespace plugins {

/// Play-order logic for the local media player. Pure logic, no I/O.
/// The queue is a snapshot of one container (folder, later album/artist);
/// shuffle re-orders traversal without changing the underlying list.
class PlayQueue : public QObject {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentChanged)
    Q_PROPERTY(int count READ count NOTIFY queueChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY shuffleChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)

public:
    enum RepeatMode { RepeatOff = 0, RepeatAll = 1, RepeatOne = 2 };
    Q_ENUM(RepeatMode)

    explicit PlayQueue(QObject* parent = nullptr);

    void setTracks(const QStringList& paths, int startIndex);
    void clear();

    QString currentTrack() const;
    QStringList tracks() const { return tracks_; }
    int currentIndex() const { return currentIndex_; }
    int count() const { return tracks_.size(); }

    bool shuffle() const { return shuffle_; }
    void setShuffle(bool on);
    int repeatMode() const { return repeatMode_; }
    void setRepeatMode(int mode);

    /// Move to the next track. manual=true is the user's next button
    /// (RepeatOne moves anyway); manual=false is end-of-track auto-advance
    /// (RepeatOne replays). Returns false when the queue end is reached
    /// under RepeatOff (current track unchanged).
    bool advance(bool manual);

    /// Move to the previous track. Wraps only under RepeatAll.
    bool retreat();

    /// Jump to an absolute index in tracks() (e.g. user taps a row).
    void jumpTo(int index);

    /// Test hook: deterministic shuffle order.
    void setShuffleSeed(quint32 seed);

signals:
    void currentChanged();
    void queueChanged();
    void shuffleChanged();
    void repeatModeChanged();

private:
    void rebuildOrder();

    QStringList tracks_;
    QVector<int> order_;     // traversal order (identity when not shuffled)
    int orderPos_ = -1;      // position within order_
    int currentIndex_ = -1;  // index into tracks_
    bool shuffle_ = false;
    int repeatMode_ = RepeatOff;
    QRandomGenerator rng_{QRandomGenerator::global()->generate()};
};

} // namespace plugins
} // namespace oap
```

`src/plugins/media_player/PlayQueue.cpp`:

```cpp
#include "PlayQueue.hpp"

#include <numeric>

namespace oap {
namespace plugins {

PlayQueue::PlayQueue(QObject* parent) : QObject(parent) {}

void PlayQueue::setTracks(const QStringList& paths, int startIndex) {
    tracks_ = paths;
    currentIndex_ = tracks_.isEmpty() ? -1 : qBound(0, startIndex, tracks_.size() - 1);
    rebuildOrder();
    emit queueChanged();
    emit currentChanged();
}

void PlayQueue::clear() {
    tracks_.clear();
    order_.clear();
    orderPos_ = -1;
    currentIndex_ = -1;
    emit queueChanged();
    emit currentChanged();
}

QString PlayQueue::currentTrack() const {
    return (currentIndex_ >= 0 && currentIndex_ < tracks_.size())
        ? tracks_.at(currentIndex_) : QString();
}

void PlayQueue::setShuffle(bool on) {
    if (shuffle_ == on) return;
    shuffle_ = on;
    rebuildOrder();
    emit shuffleChanged();
}

void PlayQueue::setRepeatMode(int mode) {
    if (repeatMode_ == mode) return;
    repeatMode_ = mode;
    emit repeatModeChanged();
}

void PlayQueue::setShuffleSeed(quint32 seed) {
    rng_ = QRandomGenerator(seed);
    if (shuffle_) rebuildOrder();
}

bool PlayQueue::advance(bool manual) {
    if (tracks_.isEmpty()) return false;
    if (repeatMode_ == RepeatOne && !manual) {
        emit currentChanged();  // replay the same track
        return true;
    }
    if (orderPos_ + 1 < order_.size()) {
        ++orderPos_;
    } else if (repeatMode_ == RepeatAll || (repeatMode_ == RepeatOne && manual)) {
        orderPos_ = 0;
    } else {
        return false;
    }
    currentIndex_ = order_.at(orderPos_);
    emit currentChanged();
    return true;
}

bool PlayQueue::retreat() {
    if (tracks_.isEmpty()) return false;
    if (orderPos_ > 0) {
        --orderPos_;
    } else if (repeatMode_ == RepeatAll) {
        orderPos_ = order_.size() - 1;
    } else {
        return false;
    }
    currentIndex_ = order_.at(orderPos_);
    emit currentChanged();
    return true;
}

void PlayQueue::jumpTo(int index) {
    if (index < 0 || index >= tracks_.size()) return;
    currentIndex_ = index;
    orderPos_ = order_.indexOf(index);
    if (orderPos_ < 0) orderPos_ = 0;  // defensive; order_ always covers all indices
    emit currentChanged();
}

void PlayQueue::rebuildOrder() {
    order_.resize(tracks_.size());
    std::iota(order_.begin(), order_.end(), 0);
    if (shuffle_ && tracks_.size() > 1) {
        for (int i = order_.size() - 1; i > 0; --i) {
            const int j = int(rng_.bounded(quint32(i + 1)));
            std::swap(order_[i], order_[j]);
        }
        // Current track plays first; traversal continues from the shuffled order.
        const int cur = order_.indexOf(currentIndex_);
        if (cur > 0) std::swap(order_[0], order_[cur]);
        orderPos_ = 0;
    } else {
        orderPos_ = qMax(0, currentIndex_);
    }
    if (order_.isEmpty()) orderPos_ = -1;
}

} // namespace plugins
} // namespace oap
```

In `src/CMakeLists.txt`, add to the `openauto-core` source list directly after `plugins/bt_audio/BtAudioPlugin.cpp`:

```cmake
    plugins/media_player/PlayQueue.cpp
```

- [ ] **Step 4: Run the test, verify it passes**

Run: `cd build && cmake .. && make -j$(nproc) test_play_queue && ctest -R test_play_queue --output-on-failure`
Expected: `100% tests passed` (9 test functions).

- [ ] **Step 5: Commit**

```bash
git add src/plugins/media_player/PlayQueue.hpp src/plugins/media_player/PlayQueue.cpp \
        tests/test_play_queue.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(media-player): PlayQueue with shuffle/repeat semantics (TDD)"
```

---

### Task 3: FolderModel — filesystem browse model (TDD)

**Files:**
- Create: `src/plugins/media_player/FolderModel.hpp`
- Create: `src/plugins/media_player/FolderModel.cpp`
- Create: `tests/test_folder_model.cpp`
- Modify: `src/CMakeLists.txt` (add `plugins/media_player/FolderModel.cpp` after `PlayQueue.cpp`)
- Modify: `tests/CMakeLists.txt` (add `oap_add_test(test_folder_model SOURCES test_folder_model.cpp)`)

**Interfaces:**
- Consumes: nothing.
- Produces (used by Tasks 8–9): `void setRoots(const QVector<QPair<QString, QString>>& labelPathPairs)` (called from C++), QML-invokable `void enter(const QString& path)`, `bool up()`, `void refresh()`, `QStringList audioFilesInCurrentDir() const`; Q_PROPERTY `QString breadcrumb`, `bool atTopLevel`; roles `name` (QString), `path` (QString), `isDir` (bool); signal `pathChanged()`.
- Audio extensions (single source of truth, used verbatim): `mp3, flac, ogg, opus, m4a, aac, wav`.

Behavior: at top level the rows are the configured roots (label shown, isDir=true). Inside a directory, rows are subdirectories first then audio files, each group sorted case-insensitively by name. Non-audio files never appear. `up()` from a root's top directory returns to the root list; `up()` at top level returns false.

- [ ] **Step 1: Write the failing test**

`tests/test_folder_model.cpp`:

```cpp
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include "plugins/media_player/FolderModel.hpp"

using oap::plugins::FolderModel;

class TestFolderModel : public QObject {
    Q_OBJECT
private slots:
    void init();          // fresh fixture tree per test
    void testTopLevelShowsRoots();
    void testEnterListsDirsFirstThenAudioSorted();
    void testNonAudioFilesExcluded();
    void testUpNavigation();
    void testAudioFilesInCurrentDir();
    void testEmptyAndMissingDirSafe();

private:
    QString rowName(const FolderModel& m, int row) const {
        return m.data(m.index(row, 0), FolderModel::NameRole).toString();
    }
    bool rowIsDir(const FolderModel& m, int row) const {
        return m.data(m.index(row, 0), FolderModel::IsDirRole).toBool();
    }
    QString rowPath(const FolderModel& m, int row) const {
        return m.data(m.index(row, 0), FolderModel::PathRole).toString();
    }
    void makeFile(const QString& path) {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
    }

    QScopedPointer<QTemporaryDir> tmp_;
    QString root_;
};

void TestFolderModel::init() {
    tmp_.reset(new QTemporaryDir);
    QVERIFY(tmp_->isValid());
    root_ = tmp_->path();
    QDir(root_).mkpath("Zeppelin/IV");
    QDir(root_).mkpath("Apple");
    makeFile(root_ + "/Zeppelin/02 Rock and Roll.mp3");
    makeFile(root_ + "/Zeppelin/01 Black Dog.flac");
    makeFile(root_ + "/Zeppelin/cover.jpg");
    makeFile(root_ + "/Zeppelin/notes.txt");
    makeFile(root_ + "/Zeppelin/IV/04 Stairway.opus");
}

void TestFolderModel::testTopLevelShowsRoots() {
    FolderModel m;
    m.setRoots({{QStringLiteral("Music"), root_},
                {QStringLiteral("USB1"), root_ + "/Zeppelin"}});
    QVERIFY(m.atTopLevel());
    QCOMPARE(m.rowCount(), 2);
    QCOMPARE(rowName(m, 0), QString("Music"));
    QCOMPARE(rowName(m, 1), QString("USB1"));
    QVERIFY(rowIsDir(m, 0));
}

void TestFolderModel::testEnterListsDirsFirstThenAudioSorted() {
    FolderModel m;
    m.setRoots({{QStringLiteral("Music"), root_}});
    m.enter(root_ + "/Zeppelin");
    QVERIFY(!m.atTopLevel());
    // dirs first (IV), then audio sorted: 01 Black Dog.flac, 02 Rock and Roll.mp3
    QCOMPARE(m.rowCount(), 3);
    QCOMPARE(rowName(m, 0), QString("IV"));
    QVERIFY(rowIsDir(m, 0));
    QCOMPARE(rowName(m, 1), QString("01 Black Dog.flac"));
    QVERIFY(!rowIsDir(m, 1));
    QCOMPARE(rowName(m, 2), QString("02 Rock and Roll.mp3"));
}

void TestFolderModel::testNonAudioFilesExcluded() {
    FolderModel m;
    m.setRoots({{QStringLiteral("Music"), root_}});
    m.enter(root_ + "/Zeppelin");
    for (int i = 0; i < m.rowCount(); ++i) {
        QVERIFY(!rowName(m, i).endsWith(".jpg"));
        QVERIFY(!rowName(m, i).endsWith(".txt"));
    }
}

void TestFolderModel::testUpNavigation() {
    FolderModel m;
    m.setRoots({{QStringLiteral("Music"), root_}});
    m.enter(root_ + "/Zeppelin");
    m.enter(root_ + "/Zeppelin/IV");
    QVERIFY(m.breadcrumb().contains("IV"));
    QVERIFY(m.up());                       // -> Zeppelin
    QVERIFY(m.up());                       // -> root_ (the "Music" root dir)
    QVERIFY(m.up());                       // -> top level (root list)
    QVERIFY(m.atTopLevel());
    QVERIFY(!m.up());                      // already at top
}

void TestFolderModel::testAudioFilesInCurrentDir() {
    FolderModel m;
    m.setRoots({{QStringLiteral("Music"), root_}});
    m.enter(root_ + "/Zeppelin");
    const QStringList files = m.audioFilesInCurrentDir();
    QCOMPARE(files.size(), 2);
    QVERIFY(files.at(0).endsWith("01 Black Dog.flac"));
    QVERIFY(files.at(1).endsWith("02 Rock and Roll.mp3"));
}

void TestFolderModel::testEmptyAndMissingDirSafe() {
    FolderModel m;
    m.setRoots({{QStringLiteral("Music"), root_ + "/Apple"}});
    m.enter(root_ + "/Apple");
    QCOMPARE(m.rowCount(), 0);
    QCOMPARE(m.audioFilesInCurrentDir(), QStringList());
    m.enter(root_ + "/DoesNotExist");      // must not crash; stays put or empties
    QVERIFY(m.rowCount() >= 0);
}

QTEST_GUILESS_MAIN(TestFolderModel)
#include "test_folder_model.moc"
```

- [ ] **Step 2: Add to tests/CMakeLists.txt, verify build failure**

```cmake
oap_add_test(test_folder_model SOURCES test_folder_model.cpp)
```

Run: `cd build && cmake .. && make test_folder_model 2>&1 | tail -3`
Expected: FAIL — `FolderModel.hpp: No such file or directory`.

- [ ] **Step 3: Implement FolderModel**

`src/plugins/media_player/FolderModel.hpp`:

```cpp
#pragma once

#include <QAbstractListModel>
#include <QPair>
#include <QVector>

namespace oap {
namespace plugins {

/// Filesystem browse model for the media player's Folders view.
/// Top level = configured roots (~/Music + mounted USB volumes); inside a
/// directory: subdirs first, then audio files, both sorted case-insensitively.
class FolderModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString breadcrumb READ breadcrumb NOTIFY pathChanged)
    Q_PROPERTY(bool atTopLevel READ atTopLevel NOTIFY pathChanged)

public:
    enum Roles { NameRole = Qt::UserRole + 1, PathRole, IsDirRole };

    explicit FolderModel(QObject* parent = nullptr);

    /// Replace the root set (label, absolute path). Resets to top level.
    void setRoots(const QVector<QPair<QString, QString>>& roots);

    Q_INVOKABLE void enter(const QString& path);
    Q_INVOKABLE bool up();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QStringList audioFilesInCurrentDir() const;

    QString breadcrumb() const;
    bool atTopLevel() const { return currentDir_.isEmpty(); }

    // QAbstractListModel
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Single source of truth for playable extensions (lowercase, no dot).
    static const QStringList& audioExtensions();

signals:
    void pathChanged();

private:
    struct Entry { QString name; QString path; bool isDir; };
    void rebuild();

    QVector<QPair<QString, QString>> roots_;  // label, path
    QString currentDir_;                      // empty = top level
    QVector<Entry> entries_;
};

} // namespace plugins
} // namespace oap
```

`src/plugins/media_player/FolderModel.cpp`:

```cpp
#include "FolderModel.hpp"

#include <QDir>
#include <QFileInfo>

namespace oap {
namespace plugins {

FolderModel::FolderModel(QObject* parent) : QAbstractListModel(parent) {}

const QStringList& FolderModel::audioExtensions() {
    static const QStringList exts = {
        QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("ogg"),
        QStringLiteral("opus"), QStringLiteral("m4a"), QStringLiteral("aac"),
        QStringLiteral("wav"),
    };
    return exts;
}

void FolderModel::setRoots(const QVector<QPair<QString, QString>>& roots) {
    roots_ = roots;
    currentDir_.clear();
    rebuild();
    emit pathChanged();
}

void FolderModel::enter(const QString& path) {
    if (!QFileInfo(path).isDir()) return;
    currentDir_ = path;
    rebuild();
    emit pathChanged();
}

bool FolderModel::up() {
    if (atTopLevel()) return false;
    // If the current dir IS one of the roots, go back to the root list.
    for (const auto& r : roots_) {
        if (QDir::cleanPath(currentDir_) == QDir::cleanPath(r.second)) {
            currentDir_.clear();
            rebuild();
            emit pathChanged();
            return true;
        }
    }
    QDir d(currentDir_);
    if (!d.cdUp()) return false;
    currentDir_ = d.absolutePath();
    rebuild();
    emit pathChanged();
    return true;
}

void FolderModel::refresh() {
    rebuild();
}

QStringList FolderModel::audioFilesInCurrentDir() const {
    QStringList files;
    for (const auto& e : entries_)
        if (!e.isDir) files << e.path;
    return files;
}

QString FolderModel::breadcrumb() const {
    if (atTopLevel()) return QStringLiteral("Sources");
    // Show "<RootLabel> / relative / path" when under a known root.
    for (const auto& r : roots_) {
        const QString rootPath = QDir::cleanPath(r.second);
        const QString cur = QDir::cleanPath(currentDir_);
        if (cur == rootPath) return r.first;
        if (cur.startsWith(rootPath + QLatin1Char('/')))
            return r.first + QStringLiteral(" / ")
                 + cur.mid(rootPath.size() + 1).replace(QLatin1Char('/'), QStringLiteral(" / "));
    }
    return QFileInfo(currentDir_).fileName();
}

void FolderModel::rebuild() {
    beginResetModel();
    entries_.clear();
    if (atTopLevel()) {
        for (const auto& r : roots_)
            if (QFileInfo(r.second).isDir())
                entries_.append({r.first, r.second, true});
    } else {
        QDir dir(currentDir_);
        QStringList nameFilters;
        for (const QString& ext : audioExtensions())
            nameFilters << QStringLiteral("*.") + ext;
        const auto dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                            QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& fi : dirs)
            entries_.append({fi.fileName(), fi.absoluteFilePath(), true});
        const auto files = dir.entryInfoList(nameFilters, QDir::Files,
                                             QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& fi : files)
            entries_.append({fi.fileName(), fi.absoluteFilePath(), false});
    }
    endResetModel();
}

int FolderModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : entries_.size();
}

QVariant FolderModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= entries_.size()) return {};
    const Entry& e = entries_.at(index.row());
    switch (role) {
    case NameRole: return e.name;
    case PathRole: return e.path;
    case IsDirRole: return e.isDir;
    default: return {};
    }
}

QHash<int, QByteArray> FolderModel::roleNames() const {
    return {{NameRole, "name"}, {PathRole, "path"}, {IsDirRole, "isDir"}};
}

} // namespace plugins
} // namespace oap
```

In `src/CMakeLists.txt`, add after `plugins/media_player/PlayQueue.cpp`:

```cmake
    plugins/media_player/FolderModel.cpp
```

- [ ] **Step 4: Run the test, verify it passes**

Run: `cd build && cmake .. && make -j$(nproc) test_folder_model && ctest -R test_folder_model --output-on-failure`
Expected: PASS (6 test functions).

- [ ] **Step 5: Commit**

```bash
git add src/plugins/media_player/FolderModel.hpp src/plugins/media_player/FolderModel.cpp \
        tests/test_folder_model.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(media-player): FolderModel filesystem browse model (TDD)"
```

---

### Task 4: Widen IMediaStatusProvider + rewrite MediaStatusService to 3-source playing-wins

The seam change. New provider fields use **default-implemented virtuals** so existing fakes and implementors keep compiling. The service replaces per-source member soup with a `SourceState` array and a sequence-number arbiter.

Arbitration rules (the test spec — spec §6):
1. Any connected source whose state is *playing* owns the display; most recent play-start (highest seq) breaks ties.
2. Nothing playing → the currently active source keeps the display while it stays connected.
3. Otherwise → the most recently active (highest seq) connected source; none connected → no source.
4. Connecting alone no longer steals the display from a playing or previously-active source (behavior change vs. today's "AA connected wins" — the old tests asserting that get replaced).

**Files:**
- Modify: `src/core/services/IMediaStatusProvider.hpp`
- Modify: `src/core/services/MediaStatusService.hpp`
- Modify: `src/core/services/MediaStatusService.cpp`
- Rewrite: `tests/test_media_status_service.cpp`

**Interfaces:**
- Consumes: existing `IMediaStatusService` base (unchanged).
- Produces (used by Tasks 5, 8, 10):
  - Provider additions: `bool isPlaying() const`, `qint64 position() const` (ms, -1 unknown), `qint64 duration() const` (ms, 0 unknown), `bool hasPosition() const`, `QString artUrl() const`, signal `void progressChanged()`.
  - Service additions: `void setMediaPlayerConnected(bool)`, `void updateMediaPlayerMetadata(const QString& title, const QString& artist, const QString& album)`, `void updateMediaPlayerPlaybackState(int state)` (0/1/2), `void updateMediaPlayerProgress(qint64 posMs, qint64 durMs)`, `void updateMediaPlayerArt(const QString& artUrl)`, `void updateBtProgress(qint64 posMs, qint64 durMs)`.
  - `source()` now may return `"MediaPlayer"`.

- [ ] **Step 1: Rewrite the test file (failing first)**

Replace the entire content of `tests/test_media_status_service.cpp` with:

```cpp
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
```

- [ ] **Step 2: Run to verify failure**

Run: `cd build && cmake .. && make test_media_status_service 2>&1 | tail -5`
Expected: FAIL — `no member named 'setMediaPlayerConnected'` (and friends).

- [ ] **Step 3: Widen IMediaStatusProvider**

Replace the body of `src/core/services/IMediaStatusProvider.hpp` with:

```cpp
#pragma once

#include <QObject>
#include <QString>

namespace oap {

class IMediaStatusProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString title READ title NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString album READ album NOTIFY mediaStatusChanged)
    Q_PROPERTY(int playbackState READ playbackState NOTIFY mediaStatusChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString source READ source NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString appName READ appName NOTIFY mediaStatusChanged)
    Q_PROPERTY(QString artUrl READ artUrl NOTIFY mediaStatusChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY progressChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY progressChanged)
    Q_PROPERTY(bool hasPosition READ hasPosition NOTIFY progressChanged)
public:
    using QObject::QObject;

    virtual bool hasMedia() const = 0;
    virtual QString title() const = 0;
    virtual QString artist() const = 0;
    virtual QString album() const = 0;
    /// Raw source-native state (BT 0/1/2, AA 1/2/3, MediaPlayer 0/1/2).
    /// Prefer isPlaying() for UI; consumers mapping this int MUST branch on
    /// source() (see ApiSerializers::buildMediaStatus).
    virtual int playbackState() const = 0;
    virtual QString source() const = 0;
    virtual QString appName() const = 0;

    // Additive surface (2026-07-08 media-player design §6). Default
    // implementations keep existing implementors/fakes compiling.
    virtual bool isPlaying() const { return false; }
    virtual qint64 position() const { return -1; }   ///< ms; -1 = unknown
    virtual qint64 duration() const { return 0; }    ///< ms; 0 = unknown
    virtual bool hasPosition() const { return false; }
    virtual QString artUrl() const { return {}; }    ///< QML-loadable; "" = none

    Q_INVOKABLE virtual void playPause() = 0;
    Q_INVOKABLE virtual void next() = 0;
    Q_INVOKABLE virtual void previous() = 0;

signals:
    void mediaStatusChanged();
    void progressChanged();
};

} // namespace oap
```

- [ ] **Step 4: Rewrite MediaStatusService**

Replace `src/core/services/MediaStatusService.hpp` with:

```cpp
#pragma once

#include "IMediaStatusService.hpp"
#include <array>
#include <functional>

namespace oap {

/// Core service merging media status from AA, BT, and the local MediaPlayer.
/// Arbitration is "playing wins" (spec 2026-07-08 §6): the actively-playing
/// source owns the display; most-recently-started-playing breaks ties; when
/// nothing plays, the current source keeps the display while connected, else
/// the most recently active connected source takes over.
class MediaStatusService : public IMediaStatusService {
    Q_OBJECT
public:
    explicit MediaStatusService(QObject* parent = nullptr);

    // IMediaStatusProvider
    bool hasMedia() const override;
    QString title() const override;
    QString artist() const override;
    QString album() const override;
    int playbackState() const override;
    bool isPlaying() const override;
    QString source() const override;
    QString appName() const override;
    qint64 position() const override;
    qint64 duration() const override;
    bool hasPosition() const override;
    QString artUrl() const override;
    void playPause() override;
    void next() override;
    void previous() override;

    /// Set playback control callbacks (delegated to active source by the
    /// main.cpp wiring, which branches on source()).
    using Callback = std::function<void()>;
    void setPlaybackCallbacks(Callback play, Callback next, Callback prev);

    // AA source (raw states: 1=stopped, 2=playing, 3=paused)
    void setAaConnected(bool connected);
    void updateAaMetadata(const QString& title, const QString& artist, const QString& album);
    void updateAaPlaybackState(int state, const QString& appName = {});

    // BT source (raw states: 0=stopped, 1=playing, 2=paused)
    void setBtConnected(bool connected);
    void updateBtMetadata(const QString& title, const QString& artist, const QString& album);
    void updateBtPlaybackState(int state);
    void updateBtProgress(qint64 positionMs, qint64 durationMs);

    // Local MediaPlayer source (raw states: 0=stopped, 1=playing, 2=paused)
    void setMediaPlayerConnected(bool connected);
    void updateMediaPlayerMetadata(const QString& title, const QString& artist, const QString& album);
    void updateMediaPlayerPlaybackState(int state);
    void updateMediaPlayerProgress(qint64 positionMs, qint64 durationMs);
    void updateMediaPlayerArt(const QString& artUrl);

private:
    enum SourceId { SrcBluetooth = 0, SrcAndroidAuto = 1, SrcMediaPlayer = 2, SrcCount = 3 };
    static constexpr int SrcNone = -1;

    struct SourceState {
        bool connected = false;
        bool playing = false;
        QString title, artist, album, appName, artUrl;
        int playbackState = 0;
        qint64 position = -1;
        qint64 duration = 0;
        quint64 seq = 0;   ///< bumped on connect and on transition-to-playing
    };

    static bool isPlayingState(int sourceId, int rawState);
    void setConnected(int id, bool connected);
    void applyPlaybackState(int id, int rawState);
    void recompute(bool emitEvenIfUnchanged);
    void clearSource(int id);
    const SourceState* active() const;

    std::array<SourceState, SrcCount> src_;
    int active_ = SrcNone;
    quint64 seq_ = 0;
    Callback playCallback_, nextCallback_, prevCallback_;
};

} // namespace oap
```

Replace `src/core/services/MediaStatusService.cpp` with:

```cpp
#include "MediaStatusService.hpp"

namespace oap {

MediaStatusService::MediaStatusService(QObject* parent)
    : IMediaStatusService(parent)
{
}

const MediaStatusService::SourceState* MediaStatusService::active() const {
    return (active_ >= 0 && active_ < SrcCount) ? &src_[size_t(active_)] : nullptr;
}

bool MediaStatusService::isPlayingState(int sourceId, int rawState) {
    switch (sourceId) {
    case SrcBluetooth:   return rawState == 1;  // BT: 0 stop / 1 play / 2 pause
    case SrcAndroidAuto: return rawState == 2;  // AA: 1 stop / 2 play / 3 pause
    case SrcMediaPlayer: return rawState == 1;  // MP: 0 stop / 1 play / 2 pause
    default:             return false;
    }
}

QString MediaStatusService::title() const  { const auto* a = active(); return a ? a->title : QString(); }
QString MediaStatusService::artist() const { const auto* a = active(); return a ? a->artist : QString(); }
QString MediaStatusService::album() const  { const auto* a = active(); return a ? a->album : QString(); }
int MediaStatusService::playbackState() const { const auto* a = active(); return a ? a->playbackState : 0; }
bool MediaStatusService::isPlaying() const { const auto* a = active(); return a && a->playing; }
qint64 MediaStatusService::position() const { const auto* a = active(); return a ? a->position : -1; }
qint64 MediaStatusService::duration() const { const auto* a = active(); return a ? a->duration : 0; }
bool MediaStatusService::hasPosition() const {
    const auto* a = active();
    return a && a->position >= 0 && a->duration > 0;
}
QString MediaStatusService::artUrl() const { const auto* a = active(); return a ? a->artUrl : QString(); }

QString MediaStatusService::source() const {
    switch (active_) {
    case SrcAndroidAuto: return QStringLiteral("AndroidAuto");
    case SrcBluetooth:   return QStringLiteral("Bluetooth");
    case SrcMediaPlayer: return QStringLiteral("MediaPlayer");
    default:             return {};
    }
}

QString MediaStatusService::appName() const {
    return active_ == SrcAndroidAuto ? src_[SrcAndroidAuto].appName : QString();
}

bool MediaStatusService::hasMedia() const {
    return !title().isEmpty() || !artist().isEmpty();
}

void MediaStatusService::playPause() { if (playCallback_) playCallback_(); }
void MediaStatusService::next()      { if (nextCallback_) nextCallback_(); }
void MediaStatusService::previous()  { if (prevCallback_) prevCallback_(); }

void MediaStatusService::setPlaybackCallbacks(Callback play, Callback next, Callback prev) {
    playCallback_ = std::move(play);
    nextCallback_ = std::move(next);
    prevCallback_ = std::move(prev);
}

void MediaStatusService::clearSource(int id) {
    auto& s = src_[size_t(id)];
    const quint64 keepSeq = s.seq;  // recency survives a metadata clear
    s = SourceState{};
    s.seq = keepSeq;
}

void MediaStatusService::setConnected(int id, bool connected) {
    auto& s = src_[size_t(id)];
    if (s.connected == connected) return;
    s.connected = connected;
    if (connected) {
        clearSource(id);            // fresh session — stale metadata dropped
        s.connected = true;
        s.seq = ++seq_;
    } else {
        clearSource(id);
        s.connected = false;
    }
    recompute(true);
}

void MediaStatusService::applyPlaybackState(int id, int rawState) {
    auto& s = src_[size_t(id)];
    const bool wasPlaying = s.playing;
    s.playbackState = rawState;
    s.playing = s.connected && isPlayingState(id, rawState);
    if (s.playing && !wasPlaying)
        s.seq = ++seq_;             // play-start: most recent wins ties
    recompute(active_ == id);
}

// ---- AA ----
void MediaStatusService::setAaConnected(bool connected) { setConnected(SrcAndroidAuto, connected); }

void MediaStatusService::updateAaMetadata(const QString& t, const QString& a, const QString& al) {
    auto& s = src_[SrcAndroidAuto];
    s.title = t; s.artist = a; s.album = al;
    if (active_ == SrcAndroidAuto) emit mediaStatusChanged();
}

void MediaStatusService::updateAaPlaybackState(int state, const QString& app) {
    src_[SrcAndroidAuto].appName = app;
    applyPlaybackState(SrcAndroidAuto, state);
}

// ---- BT ----
void MediaStatusService::setBtConnected(bool connected) { setConnected(SrcBluetooth, connected); }

void MediaStatusService::updateBtMetadata(const QString& t, const QString& a, const QString& al) {
    auto& s = src_[SrcBluetooth];
    s.title = t; s.artist = a; s.album = al;
    if (active_ == SrcBluetooth) emit mediaStatusChanged();
}

void MediaStatusService::updateBtPlaybackState(int state) {
    applyPlaybackState(SrcBluetooth, state);
}

void MediaStatusService::updateBtProgress(qint64 positionMs, qint64 durationMs) {
    auto& s = src_[SrcBluetooth];
    s.position = positionMs;
    s.duration = durationMs;
    if (active_ == SrcBluetooth) emit progressChanged();
}

// ---- Local MediaPlayer ----
void MediaStatusService::setMediaPlayerConnected(bool connected) { setConnected(SrcMediaPlayer, connected); }

void MediaStatusService::updateMediaPlayerMetadata(const QString& t, const QString& a, const QString& al) {
    auto& s = src_[SrcMediaPlayer];
    s.title = t; s.artist = a; s.album = al;
    if (active_ == SrcMediaPlayer) emit mediaStatusChanged();
}

void MediaStatusService::updateMediaPlayerPlaybackState(int state) {
    applyPlaybackState(SrcMediaPlayer, state);
}

void MediaStatusService::updateMediaPlayerProgress(qint64 positionMs, qint64 durationMs) {
    auto& s = src_[SrcMediaPlayer];
    s.position = positionMs;
    s.duration = durationMs;
    if (active_ == SrcMediaPlayer) emit progressChanged();
}

void MediaStatusService::updateMediaPlayerArt(const QString& artUrl) {
    src_[SrcMediaPlayer].artUrl = artUrl;
    if (active_ == SrcMediaPlayer) emit mediaStatusChanged();
}

// ---- Arbitration ----
void MediaStatusService::recompute(bool emitEvenIfUnchanged) {
    int next = SrcNone;
    quint64 best = 0;

    // Rule 1: playing sources — most recent play-start wins.
    for (int i = 0; i < SrcCount; ++i) {
        const auto& s = src_[size_t(i)];
        if (s.connected && s.playing && s.seq >= best) { next = i; best = s.seq; }
    }
    // Rule 2: nothing playing — current keeps display while connected.
    if (next == SrcNone && active_ != SrcNone && src_[size_t(active_)].connected)
        next = active_;
    // Rule 3: most recently active connected source.
    if (next == SrcNone) {
        best = 0;
        for (int i = 0; i < SrcCount; ++i) {
            const auto& s = src_[size_t(i)];
            if (s.connected && s.seq >= best) { next = i; best = s.seq; }
        }
    }

    const bool changed = (next != active_);
    active_ = next;
    if (changed || emitEvenIfUnchanged) {
        emit mediaStatusChanged();
        emit progressChanged();
    }
}

} // namespace oap
```

- [ ] **Step 5: Run the rewritten tests, verify they pass**

Run: `cd build && cmake .. && make -j$(nproc) test_media_status_service && ctest -R test_media_status_service --output-on-failure`
Expected: PASS (14 test functions).

- [ ] **Step 6: Run the FULL suite — other tests touch this seam**

Run: `ctest --output-on-failure`
Expected: all pass. Known dependents that must still compile/pass unchanged: `test_api_serializers`, `test_api_publishers`, `test_media_data_bridge`. If `test_api_serializers` fails on playback-state expectations, that's Task 5's job — check whether the failure is a compile error (fix here) vs. a new-source mapping (belongs to Task 5).

- [ ] **Step 7: Commit**

```bash
git add src/core/services/IMediaStatusProvider.hpp \
        src/core/services/MediaStatusService.hpp src/core/services/MediaStatusService.cpp \
        tests/test_media_status_service.cpp
git commit -m "feat(media): 3-source playing-wins arbitration + widened provider surface"
```

---

### Task 5: External API — proto fields, serializer branches, publisher connect

Additive-only proto changes: `LOCAL_MEDIA` consumes reserved slot 4; progress fields consume 8–10 (the exact slots the proto comment anticipated).

**Files:**
- Modify: `proto/api/media.proto`
- Modify: `src/core/api/ApiSerializers.cpp` (buildMediaStatus, lines ~108–163)
- Modify: `src/core/api/ApiPublishers.cpp` (~line 52)
- Modify: `tests/test_api_serializers.cpp` (add 3 tests; existing media tests unchanged)

**Interfaces:**
- Consumes: Task 4's provider surface (`position()`, `duration()`, `hasPosition()`, `progressChanged`), `MediaStatusService` update methods.
- Produces: wire contract `MEDIA_SOURCE_LOCAL_MEDIA = 4`, `position_ms = 8` (int64), `duration_ms = 9` (int64), `has_position = 10` (bool).

- [ ] **Step 1: Write the failing tests**

Add to `tests/test_api_serializers.cpp` — declare in the private slots list:

```cpp
    void testMediaLocalMediaPlaying();
    void testMediaProgressFieldsBtAndLocal();
    void testMediaAaNoPosition();
```

and add the implementations next to the existing media tests:

```cpp
void TestApiSerializers::testMediaLocalMediaPlaying() {
    oap::MediaStatusService media;
    media.setMediaPlayerConnected(true);
    media.updateMediaPlayerMetadata("L", "LA", "LAl");
    media.updateMediaPlayerPlaybackState(1);  // MP raw 1 = Playing
    const pb::MediaStatus st = buildMediaStatus(media);
    QCOMPARE(st.source(), pb::MEDIA_SOURCE_LOCAL_MEDIA);
    QCOMPARE(st.playback_state(), pb::PLAYBACK_STATE_PLAYING);
    QCOMPARE(QString::fromStdString(st.title()), QString("L"));

    media.updateMediaPlayerPlaybackState(2);  // MP raw 2 = Paused
    QCOMPARE(buildMediaStatus(media).playback_state(), pb::PLAYBACK_STATE_PAUSED);
    media.updateMediaPlayerPlaybackState(0);  // MP raw 0 = Stopped
    QCOMPARE(buildMediaStatus(media).playback_state(), pb::PLAYBACK_STATE_STOPPED);
}

void TestApiSerializers::testMediaProgressFieldsBtAndLocal() {
    oap::MediaStatusService media;
    media.setMediaPlayerConnected(true);
    media.updateMediaPlayerPlaybackState(1);
    media.updateMediaPlayerProgress(61000, 245000);
    const pb::MediaStatus st = buildMediaStatus(media);
    QCOMPARE(st.position_ms(), (long long)61000);
    QCOMPARE(st.duration_ms(), (long long)245000);
    QVERIFY(st.has_position());
}

void TestApiSerializers::testMediaAaNoPosition() {
    oap::MediaStatusService media;
    media.setAaConnected(true);
    media.updateAaPlaybackState(2, "App");
    const pb::MediaStatus st = buildMediaStatus(media);
    QVERIFY(!st.has_position());
    QCOMPARE(st.position_ms(), (long long)-1);
}
```

Run: `cd build && cmake .. && make test_api_serializers 2>&1 | tail -3`
Expected: FAIL — `MEDIA_SOURCE_LOCAL_MEDIA` not declared / `position_ms` no member.

- [ ] **Step 2: Amend the proto (additive)**

In `proto/api/media.proto`, replace the `MediaSource` enum with:

```proto
enum MediaSource {
  MEDIA_SOURCE_UNSPECIFIED = 0;
  MEDIA_SOURCE_NONE = 1;          // no active media source
  MEDIA_SOURCE_BLUETOOTH = 2;     // A2DP/AVRCP
  MEDIA_SOURCE_ANDROID_AUTO = 3;  // projection media channel
  MEDIA_SOURCE_LOCAL_MEDIA = 4;   // head-unit local media player plugin
  reserved 5 to 10;
}
```

and replace the trailing part of `message MediaStatus` (the `reserved 8 to 15;` line and its comment) with:

```proto
  // Track progress (2026-07-08 media-player design). position_ms is -1 when
  // the active source does not report progress (e.g. Android Auto);
  // duration_ms is 0 when unknown. has_position is the display gate: clients
  // must not render a progress bar when it is false.
  int64 position_ms = 8;
  int64 duration_ms = 9;
  bool has_position = 10;

  // 11..15 reserved for future additive fields (artwork reference is the
  // anticipated addition once a network-consumable art transport exists).
  reserved 11 to 15;
```

- [ ] **Step 3: Extend the serializer**

In `src/core/api/ApiSerializers.cpp` `buildMediaStatus()`:

(a) In the source-normalization chain, add before the final `else`:

```cpp
    } else if (sourceStr == QStringLiteral("MediaPlayer")) {
        source = pb::MEDIA_SOURCE_LOCAL_MEDIA;
```

(b) In the source-dependent playback-state switch, add a case before `case pb::MEDIA_SOURCE_NONE:` (MediaPlayer uses the BT-style 0/1/2 convention — but keep the explicit separate case; the conventions are only coincidentally identical):

```cpp
    case pb::MEDIA_SOURCE_LOCAL_MEDIA:
        // MediaPlayer (MediaPlayerPlugin): 0=Stopped, 1=Playing, 2=Paused.
        switch (raw) {
        case 0: playback = pb::PLAYBACK_STATE_STOPPED; break;
        case 1: playback = pb::PLAYBACK_STATE_PLAYING; break;
        case 2: playback = pb::PLAYBACK_STATE_PAUSED; break;
        default: playback = pb::PLAYBACK_STATE_UNSPECIFIED; break;
        }
        break;
```

(c) After `status.set_playback_state(playback);`, add:

```cpp
    // Progress (additive v1 fields 8-10). Passed through verbatim from the
    // provider; has_position gates client-side progress rendering.
    status.set_position_ms(p.position());
    status.set_duration_ms(p.duration());
    status.set_has_position(p.hasPosition());
```

- [ ] **Step 4: Publisher — progress updates trigger emission**

In `src/core/api/ApiPublishers.cpp`, directly after the existing line ~52
(`connect(p_, &oap::IMediaStatusProvider::mediaStatusChanged, ...)`), add:

```cpp
    connect(p_, &oap::IMediaStatusProvider::progressChanged, this, [this] { scheduleEmit(); });
```

(`scheduleEmit()` already coalesces bursts within one event-loop turn, so 2Hz progress ticks cost one message each at most — no extra throttling needed here.)

- [ ] **Step 5: Run tests, verify pass**

Run: `cd build && cmake .. && make -j$(nproc) test_api_serializers && ctest -R "test_api_serializers|test_api_proto_roundtrip|test_api_publishers" --output-on-failure`
Expected: PASS. (Proto regen happens automatically via the `prodigy-api-proto` target on `cmake --build`.)

- [ ] **Step 6: Commit**

```bash
git add proto/api/media.proto src/core/api/ApiSerializers.cpp src/core/api/ApiPublishers.cpp \
        tests/test_api_serializers.cpp
git commit -m "feat(api): LOCAL_MEDIA source + position/duration fields (additive v1)"
```

---

### Task 6: PlaybackEngine — QMediaPlayer + PCM tap → AudioService

Apply the Task 1 verdict here: **GO** → implement as written; **GO-with-crutch** → additionally create a muted `QAudioOutput` in the constructor (2 members, 3 lines — noted inline below); **NO-GO** → STOP, replan with Matthew (spec §5 fallback).

**Files:**
- Create: `src/plugins/media_player/PlaybackEngine.hpp`
- Create: `src/plugins/media_player/PlaybackEngine.cpp`
- Create: `tests/test_playback_engine.cpp`
- Create: `tests/data/media/tone-44k.mp3`, `tests/data/media/tone-48k.flac` (generated, committed)
- Modify: `src/CMakeLists.txt` (add `plugins/media_player/PlaybackEngine.cpp`)
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `IAudioService::createStream/writeAudio/destroyStream/requestAudioFocus/releaseAudioFocus` (`src/core/services/IAudioService.hpp`), `AudioStreamHandle::eqEngine` (`src/core/services/AudioService.hpp:51`), `EqualizerEngine` (fwd-declared).
- Produces (used by Task 8):
  - `void setAudioService(IAudioService*)`, `void setEqEngine(EqualizerEngine*)`, `void setBufferMs(int)`
  - `void playFile(const QString& path)` (load + play), `void restorePaused(const QString& path, qint64 positionMs)` (load, seek, stay paused)
  - `void play()`, `void pause()`, `void stop()`, `void seek(qint64 ms)`
  - `int playbackState() const` (0/1/2), `qint64 position() const`, `qint64 duration() const`
  - `QString title() const`, `QString artist() const`, `QString album() const`, `QImage coverArt() const`
  - signals: `playbackStateChanged()`, `progressChanged(qint64 positionMs, qint64 durationMs)`, `metadataChanged()`, `trackFinished()`, `errorOccurred(const QString& message)`

- [ ] **Step 1: Generate and commit the audio fixtures**

```bash
mkdir -p tests/data/media
ffmpeg -y -f lavfi -i "sine=frequency=440:duration=0.5" -ar 44100 -ac 2 \
  -metadata title="Tone 44" -metadata artist="Fixture Artist" -metadata album="Fixture Album" \
  -b:a 96k tests/data/media/tone-44k.mp3
ffmpeg -y -f lavfi -i "sine=frequency=440:duration=0.5" -ar 48000 -ac 2 \
  -metadata title="Tone 48" -metadata artist="Fixture Artist" \
  tests/data/media/tone-48k.flac
ls -la tests/data/media/   # expect ~8KB mp3, ~50KB flac
```

- [ ] **Step 2: Write the failing test**

`tests/test_playback_engine.cpp`:

```cpp
// Integration-ish smoke test: decodes real fixture files through QMediaPlayer's
// ffmpeg backend with NO audio device (QAudioBufferOutput tap only), verifying
// PCM lands in a fake IAudioService, metadata is read, and EOM auto-advances.
#include <QtTest/QtTest>
#include "plugins/media_player/PlaybackEngine.hpp"
#include "core/services/IAudioService.hpp"
#include "core/services/AudioService.hpp"  // AudioStreamHandle definition

using oap::plugins::PlaybackEngine;

class FakeAudioService : public oap::IAudioService {
public:
    oap::AudioStreamHandle* createStream(const QString& name, int priority,
                                         int sampleRate, int channels,
                                         const QString&, int) override {
        lastName = name; lastPriority = priority;
        lastRate = sampleRate; lastChannels = channels;
        ++created;
        return &handle;
    }
    void destroyStream(oap::AudioStreamHandle*) override { ++destroyed; }
    int writeAudio(oap::AudioStreamHandle*, const uint8_t*, int size) override {
        bytesWritten += size;
        return size;
    }
    void setMasterVolume(int) override {}
    int masterVolume() const override { return 100; }
    void requestAudioFocus(oap::AudioStreamHandle*, oap::AudioFocusType t) override {
        ++focusRequests; lastFocusType = t;
    }
    void releaseAudioFocus(oap::AudioStreamHandle*) override { ++focusReleases; }
    void setOutputDevice(const QString&) override {}
    void setInputDevice(const QString&) override {}
    QString outputDevice() const override { return "auto"; }
    QString inputDevice() const override { return "auto"; }

    oap::AudioStreamHandle handle;
    QString lastName;
    int lastPriority = 0, lastRate = 0, lastChannels = 0;
    int created = 0, destroyed = 0, focusRequests = 0, focusReleases = 0;
    qint64 bytesWritten = 0;
    oap::AudioFocusType lastFocusType = oap::AudioFocusType::Gain;
};

class TestPlaybackEngine : public QObject {
    Q_OBJECT
private slots:
    void testPlaysFixtureToCompletion();
    void testMetadataAndState();
    void testErrorOnGarbagePath();
private:
    QString fixture(const char* name) const {
        return QStringLiteral(TEST_DATA_DIR "/media/") + QLatin1String(name);
    }
};

void TestPlaybackEngine::testPlaysFixtureToCompletion() {
    FakeAudioService audio;
    PlaybackEngine eng;
    eng.setAudioService(&audio);
    QSignalSpy finished(&eng, &PlaybackEngine::trackFinished);
    QSignalSpy errors(&eng, &PlaybackEngine::errorOccurred);

    eng.playFile(fixture("tone-44k.mp3"));
    // 0.5 s fixture; allow generous slack for backend spin-up.
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1 || errors.count() > 0, 15000);
    if (errors.count() > 0)
        QSKIP("Multimedia backend unavailable in this environment (spike env should match!)");

    QCOMPARE(audio.created, 1);
    QCOMPARE(audio.lastName, QString("Local Media"));
    QCOMPARE(audio.lastPriority, 50);
    QCOMPARE(audio.lastRate, 48000);
    QCOMPARE(audio.lastChannels, 2);
    // 0.5 s @ 48kHz stereo S16 = 96000 bytes; tolerate codec padding/trim.
    QVERIFY2(audio.bytesWritten > 96000 * 0.5 && audio.bytesWritten < 96000 * 2.0,
             qPrintable(QString("bytesWritten=%1").arg(audio.bytesWritten)));
    QVERIFY(audio.focusRequests >= 1);
    QVERIFY(audio.focusReleases >= 1);   // released on EOM/stop
}

void TestPlaybackEngine::testMetadataAndState() {
    FakeAudioService audio;
    PlaybackEngine eng;
    eng.setAudioService(&audio);
    QSignalSpy meta(&eng, &PlaybackEngine::metadataChanged);
    QSignalSpy finished(&eng, &PlaybackEngine::trackFinished);
    QSignalSpy errors(&eng, &PlaybackEngine::errorOccurred);

    eng.playFile(fixture("tone-48k.flac"));
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1 || errors.count() > 0, 15000);
    if (errors.count() > 0)
        QSKIP("Multimedia backend unavailable in this environment");

    QVERIFY(meta.count() >= 1);
    QCOMPARE(eng.title(), QString("Tone 48"));
    QCOMPARE(eng.artist(), QString("Fixture Artist"));
    QVERIFY(eng.duration() >= 400 && eng.duration() <= 700);  // ~500 ms
    QCOMPARE(eng.playbackState(), 0);   // stopped after EOM
}

void TestPlaybackEngine::testErrorOnGarbagePath() {
    FakeAudioService audio;
    PlaybackEngine eng;
    eng.setAudioService(&audio);
    QSignalSpy errors(&eng, &PlaybackEngine::errorOccurred);
    eng.playFile("/nonexistent/nope.mp3");
    QTRY_VERIFY_WITH_TIMEOUT(errors.count() >= 1, 10000);
}

QTEST_GUILESS_MAIN(TestPlaybackEngine)
#include "test_playback_engine.moc"
```

In `tests/CMakeLists.txt` add (note TEST_DATA_DIR define + fixture copy):

```cmake
oap_add_test(test_playback_engine
    SOURCES test_playback_engine.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
file(COPY data/media DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data)
```

Run: `cd build && cmake .. && make test_playback_engine 2>&1 | tail -3`
Expected: FAIL — `PlaybackEngine.hpp: No such file or directory`.

- [ ] **Step 3: Implement PlaybackEngine**

`src/plugins/media_player/PlaybackEngine.hpp`:

```cpp
#pragma once

#include <QAudioBufferOutput>
#include <QElapsedTimer>
#include <QImage>
#include <QMediaPlayer>
#include <QObject>
#include <QString>

namespace oap {

class IAudioService;
struct AudioStreamHandle;
class EqualizerEngine;

namespace plugins {

/// Transport + decode for the local media player. QMediaPlayer drives
/// decode/clock/seek; decoded PCM is tapped via QAudioBufferOutput
/// (48 kHz / S16 / stereo, converted by Qt) and pushed into an AudioService
/// stream — the exact mechanics AA media audio uses (createStream +
/// writeAudio + eqEngine), so EQ / master volume / ducking / focus all apply.
/// No QAudioOutput device sink is attached (spike-verified, Task 1).
class PlaybackEngine : public QObject {
    Q_OBJECT

public:
    explicit PlaybackEngine(QObject* parent = nullptr);
    ~PlaybackEngine() override;

    void setAudioService(IAudioService* service) { audioService_ = service; }
    void setEqEngine(EqualizerEngine* engine) { eqEngine_ = engine; }
    void setBufferMs(int ms) { bufferMs_ = ms; }

    void playFile(const QString& path);
    void restorePaused(const QString& path, qint64 positionMs);
    void play();
    void pause();
    void stop();
    void seek(qint64 ms);

    /// 0=Stopped, 1=Playing, 2=Paused (the MediaPlayer source convention).
    int playbackState() const;
    qint64 position() const { return player_.position(); }
    qint64 duration() const { return player_.duration(); }
    QString title() const { return title_; }
    QString artist() const { return artist_; }
    QString album() const { return album_; }
    QImage coverArt() const { return coverArt_; }

signals:
    void playbackStateChanged();
    void progressChanged(qint64 positionMs, qint64 durationMs);
    void metadataChanged();
    void trackFinished();
    void errorOccurred(const QString& message);

private:
    void ensureStream();
    void onAudioBuffer(const QAudioBuffer& buffer);
    void onMediaStatus(QMediaPlayer::MediaStatus status);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void readMetadata();

    QMediaPlayer player_;
    QAudioBufferOutput tap_;
    IAudioService* audioService_ = nullptr;
    AudioStreamHandle* stream_ = nullptr;
    EqualizerEngine* eqEngine_ = nullptr;
    int bufferMs_ = 50;

    QString title_, artist_, album_;
    QImage coverArt_;
    qint64 pendingSeekMs_ = -1;
    bool pauseAfterLoad_ = false;
    QElapsedTimer progressEmitTimer_;
};

} // namespace plugins
} // namespace oap
```

`src/plugins/media_player/PlaybackEngine.cpp`:

```cpp
#include "PlaybackEngine.hpp"

#include <QAudioBuffer>
#include <QAudioFormat>
#include <QLoggingCategory>
#include <QMediaMetaData>
#include <QUrl>

#include "core/services/IAudioService.hpp"

Q_LOGGING_CATEGORY(lcMediaPlayer, "oap.mediaplayer")

namespace {
QAudioFormat tapFormat() {
    QAudioFormat fmt;
    fmt.setSampleRate(48000);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Int16);
    return fmt;
}
} // namespace

namespace oap {
namespace plugins {

PlaybackEngine::PlaybackEngine(QObject* parent)
    : QObject(parent)
    , tap_(tapFormat())
{
    player_.setAudioBufferOutput(&tap_);
    // NOTE (Task 1 verdict): if the spike required the muted-sink crutch,
    // add here:  audioOut_ = new QAudioOutput(this); audioOut_->setVolume(0);
    //            player_.setAudioOutput(audioOut_);
    // and declare QAudioOutput* audioOut_ in the header.

    connect(&tap_, &QAudioBufferOutput::audioBufferReceived,
            this, &PlaybackEngine::onAudioBuffer);
    connect(&player_, &QMediaPlayer::mediaStatusChanged,
            this, &PlaybackEngine::onMediaStatus);
    connect(&player_, &QMediaPlayer::playbackStateChanged,
            this, &PlaybackEngine::onPlaybackStateChanged);
    connect(&player_, &QMediaPlayer::metaDataChanged,
            this, &PlaybackEngine::readMetadata);
    connect(&player_, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString& msg) {
        qCWarning(lcMediaPlayer) << "playback error:" << msg << "file:" << player_.source();
        emit errorOccurred(msg);
    });
    connect(&player_, &QMediaPlayer::positionChanged, this, [this](qint64 pos) {
        if (!progressEmitTimer_.isValid() || progressEmitTimer_.elapsed() >= 500) {
            progressEmitTimer_.restart();
            emit progressChanged(pos, player_.duration());
        }
    });
    connect(&player_, &QMediaPlayer::durationChanged, this, [this](qint64 dur) {
        emit progressChanged(player_.position(), dur);
    });
}

PlaybackEngine::~PlaybackEngine() {
    player_.stop();
    if (audioService_ && stream_) {
        audioService_->releaseAudioFocus(stream_);
        audioService_->destroyStream(stream_);
        stream_ = nullptr;
    }
}

void PlaybackEngine::playFile(const QString& path) {
    pendingSeekMs_ = -1;
    pauseAfterLoad_ = false;
    player_.setSource(QUrl::fromLocalFile(path));
    player_.play();
}

void PlaybackEngine::restorePaused(const QString& path, qint64 positionMs) {
    pendingSeekMs_ = positionMs > 0 ? positionMs : -1;
    pauseAfterLoad_ = true;
    player_.setSource(QUrl::fromLocalFile(path));
    // Seek + pause complete in onMediaStatus(LoadedMedia).
}

void PlaybackEngine::play()  { player_.play(); }
void PlaybackEngine::pause() { player_.pause(); }

void PlaybackEngine::stop() {
    player_.stop();
    if (audioService_ && stream_)
        audioService_->releaseAudioFocus(stream_);
}

void PlaybackEngine::seek(qint64 ms) { player_.setPosition(ms); }

int PlaybackEngine::playbackState() const {
    switch (player_.playbackState()) {
    case QMediaPlayer::PlayingState: return 1;
    case QMediaPlayer::PausedState:  return 2;
    default:                         return 0;
    }
}

void PlaybackEngine::ensureStream() {
    if (stream_ || !audioService_) return;
    stream_ = audioService_->createStream(QStringLiteral("Local Media"), 50,
                                          48000, 2, QStringLiteral("auto"), bufferMs_);
    if (!stream_) {
        qCWarning(lcMediaPlayer) << "AudioService stream creation failed";
        return;
    }
    stream_->eqEngine = eqEngine_;  // same attach pattern as AA media
}

void PlaybackEngine::onAudioBuffer(const QAudioBuffer& buffer) {
    ensureStream();
    if (!stream_ || !audioService_) return;
    const int written = audioService_->writeAudio(
        stream_, buffer.constData<uint8_t>(), int(buffer.byteCount()));
    if (written >= 0 && written < buffer.byteCount()) {
        static int dropWarnings = 0;
        if (++dropWarnings % 100 == 1)
            qCWarning(lcMediaPlayer) << "ring buffer overrun, dropped"
                                     << (buffer.byteCount() - written) << "bytes";
    }
}

void PlaybackEngine::onMediaStatus(QMediaPlayer::MediaStatus status) {
    switch (status) {
    case QMediaPlayer::LoadedMedia:
        if (pendingSeekMs_ >= 0) {
            player_.setPosition(pendingSeekMs_);
            pendingSeekMs_ = -1;
        }
        if (pauseAfterLoad_) {
            pauseAfterLoad_ = false;
            player_.pause();
        }
        break;
    case QMediaPlayer::EndOfMedia:
        if (audioService_ && stream_)
            audioService_->releaseAudioFocus(stream_);
        emit trackFinished();
        break;
    default:
        break;
    }
}

void PlaybackEngine::onPlaybackStateChanged(QMediaPlayer::PlaybackState state) {
    if (audioService_) {
        if (state == QMediaPlayer::PlayingState) {
            ensureStream();
            if (stream_) audioService_->requestAudioFocus(stream_, AudioFocusType::Gain);
        } else if (stream_) {
            audioService_->releaseAudioFocus(stream_);
        }
    }
    emit playbackStateChanged();
    emit progressChanged(player_.position(), player_.duration());
}

void PlaybackEngine::readMetadata() {
    const QMediaMetaData md = player_.metaData();
    title_ = md.value(QMediaMetaData::Title).toString();
    artist_ = md.value(QMediaMetaData::ContributingArtist).toString();
    if (artist_.isEmpty())
        artist_ = md.value(QMediaMetaData::AlbumArtist).toString();
    album_ = md.value(QMediaMetaData::AlbumTitle).toString();
    coverArt_ = md.value(QMediaMetaData::CoverArtImage).value<QImage>();
    if (coverArt_.isNull())
        coverArt_ = md.value(QMediaMetaData::ThumbnailImage).value<QImage>();
    if (title_.isEmpty())
        title_ = QUrl(player_.source()).fileName();
    emit metadataChanged();
}

} // namespace plugins
} // namespace oap
```

In `src/CMakeLists.txt`, add after `plugins/media_player/FolderModel.cpp`:

```cmake
    plugins/media_player/PlaybackEngine.cpp
```

- [ ] **Step 4: Run the test, verify it passes**

Run: `cd build && cmake .. && make -j$(nproc) test_playback_engine && ctest -R test_playback_engine --output-on-failure`
Expected: PASS (3 test functions; the two decode tests must NOT hit the QSKIP on the dev box — if they skip, the environment regressed vs. the Task 1 spike; investigate before continuing).

Platform note: if QMediaPlayer refuses to run under `QTEST_GUILESS_MAIN` (backend wants a GUI app), switch the macro to `QTEST_MAIN` and run with `QT_QPA_PLATFORM=offscreen` (add `set_tests_properties(test_playback_engine PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")` in tests/CMakeLists.txt). The Task 1 spike used QCoreApplication, so GUILESS is expected to work.

- [ ] **Step 5: Commit**

```bash
git add src/plugins/media_player/PlaybackEngine.hpp src/plugins/media_player/PlaybackEngine.cpp \
        tests/test_playback_engine.cpp tests/data/media/ src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(media-player): PlaybackEngine — QMediaPlayer PCM tap into AudioService"
```

---

### Task 7: MediaArtProvider — current-track art for QML

**Files:**
- Create: `src/plugins/media_player/MediaArtProvider.hpp`
- Create: `src/plugins/media_player/MediaArtProvider.cpp`
- Create: `tests/test_media_art_provider.cpp`
- Modify: `src/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `QImage` from `PlaybackEngine::coverArt()`.
- Produces (used by Tasks 8, 10): `void setCurrentArt(const QImage&)`, `QString currentUrl() const` (`"image://mediaart/current/<rev>"`, or `""` when no art), `QQuickImageProvider::requestImage` override.
- **Lifetime rule:** `QQmlEngine::addImageProvider` takes ownership. main.cpp creates the provider, hands a NON-owning pointer to the plugin, then registers it with the engine. The plugin must never delete it.

- [ ] **Step 1: Write the failing test**

`tests/test_media_art_provider.cpp`:

```cpp
#include <QtTest/QtTest>
#include <QImage>
#include "plugins/media_player/MediaArtProvider.hpp"

using oap::plugins::MediaArtProvider;

class TestMediaArtProvider : public QObject {
    Q_OBJECT
private slots:
    void testEmptyByDefault();
    void testSetArtBumpsRevisionAndServesImage();
    void testClearArt();
};

void TestMediaArtProvider::testEmptyByDefault() {
    MediaArtProvider p;
    QCOMPARE(p.currentUrl(), QString());
    QSize size;
    const QImage img = p.requestImage("current/0", &size, QSize());
    QVERIFY(!img.isNull());          // placeholder pixel, never a null image
}

void TestMediaArtProvider::testSetArtBumpsRevisionAndServesImage() {
    MediaArtProvider p;
    QImage art(64, 64, QImage::Format_RGB32);
    art.fill(Qt::red);
    p.setCurrentArt(art);
    const QString url1 = p.currentUrl();
    QVERIFY(url1.startsWith("image://mediaart/current/"));
    QSize size;
    const QImage served = p.requestImage("current/1", &size, QSize());
    QCOMPARE(served.size(), QSize(64, 64));

    QImage art2(32, 32, QImage::Format_RGB32);
    art2.fill(Qt::blue);
    p.setCurrentArt(art2);
    QVERIFY(p.currentUrl() != url1);  // revision changed -> QML cache busted
}

void TestMediaArtProvider::testClearArt() {
    MediaArtProvider p;
    QImage art(8, 8, QImage::Format_RGB32);
    art.fill(Qt::green);
    p.setCurrentArt(art);
    QVERIFY(!p.currentUrl().isEmpty());
    p.setCurrentArt(QImage());
    QCOMPARE(p.currentUrl(), QString());
}

QTEST_GUILESS_MAIN(TestMediaArtProvider)
#include "test_media_art_provider.moc"
```

In `tests/CMakeLists.txt`:

```cmake
oap_add_test(test_media_art_provider SOURCES test_media_art_provider.cpp)
```

Run: `cd build && cmake .. && make test_media_art_provider 2>&1 | tail -3`
Expected: FAIL — header not found.

- [ ] **Step 2: Implement**

`src/plugins/media_player/MediaArtProvider.hpp`:

```cpp
#pragma once

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

namespace oap {
namespace plugins {

/// Serves the current track's embedded cover art to QML as
/// image://mediaart/current/<rev>. The revision suffix busts QML's image
/// cache on track change. Thread-safe: requestImage() is called from QML's
/// image-loader threads while setCurrentArt() runs on the main thread.
class MediaArtProvider : public QQuickImageProvider {
public:
    MediaArtProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    /// Main thread. Null image = no art (currentUrl() becomes empty).
    void setCurrentArt(const QImage& image);

    /// "" when no art is available.
    QString currentUrl() const;

    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requestedSize) override;

private:
    mutable QMutex mutex_;
    QImage art_;
    quint64 revision_ = 0;
};

} // namespace plugins
} // namespace oap
```

`src/plugins/media_player/MediaArtProvider.cpp`:

```cpp
#include "MediaArtProvider.hpp"

namespace oap {
namespace plugins {

void MediaArtProvider::setCurrentArt(const QImage& image) {
    QMutexLocker lock(&mutex_);
    art_ = image;
    ++revision_;
}

QString MediaArtProvider::currentUrl() const {
    QMutexLocker lock(&mutex_);
    if (art_.isNull()) return {};
    return QStringLiteral("image://mediaart/current/%1").arg(revision_);
}

QImage MediaArtProvider::requestImage(const QString& /*id*/, QSize* size,
                                      const QSize& requestedSize) {
    QMutexLocker lock(&mutex_);
    QImage img = art_;
    if (img.isNull()) {
        img = QImage(1, 1, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
    }
    if (requestedSize.isValid() && !requestedSize.isEmpty())
        img = img.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (size) *size = img.size();
    return img;
}

} // namespace plugins
} // namespace oap
```

In `src/CMakeLists.txt`, add after `plugins/media_player/PlaybackEngine.cpp`:

```cmake
    plugins/media_player/MediaArtProvider.cpp
```

- [ ] **Step 3: Run the test, verify it passes**

Run: `cd build && cmake .. && make -j$(nproc) test_media_art_provider && ctest -R test_media_art_provider --output-on-failure`
Expected: PASS (3 test functions).

- [ ] **Step 4: Commit**

```bash
git add src/plugins/media_player/MediaArtProvider.hpp src/plugins/media_player/MediaArtProvider.cpp \
        tests/test_media_art_provider.cpp src/CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(media-player): MediaArtProvider QML image provider for track art"
```

---

### Task 8: MediaPlayerPlugin — composition, persistence, registration, main.cpp wiring

Glues Tasks 2/3/6/7 together behind `IPlugin`, registers with the host, and wires the provider + pause-others policy in `main.cpp`. Ships with a minimal placeholder view so the app is runnable after this task; Task 9 replaces it.

Stage-1 note: source discovery is a plugin method (`refreshSources()` using `QStorageInfo`), not the spec's `MediaSourceWatcher` class — the class shape only pays off when the udisks2 hot-plug watcher arrives in stage 2. Deliberate simplification, noted for the stage-2 plan.

**Files:**
- Create: `src/plugins/media_player/MediaPlayerPlugin.hpp`
- Create: `src/plugins/media_player/MediaPlayerPlugin.cpp`
- Create: `qml/applications/media_player/MediaPlayerView.qml` (placeholder)
- Modify: `src/CMakeLists.txt` (source list; QML source properties; QML file list)
- Modify: `src/main.cpp` (5 insertion points, exact anchors below)

**Interfaces:**
- Consumes: `PlayQueue`, `FolderModel`, `PlaybackEngine`, `MediaArtProvider` (Tasks 2/3/6/7); `IHostContext::audioService()/configService()/notificationService()/equalizerService()`; `IConfigService::pluginValue/setPluginValue`; `EqualizerService::engineForStream(StreamId::Media)` via `dynamic_cast` (same pattern as `AndroidAutoPlugin.cpp:51`); `MediaStatusService` update methods (Task 4).
- Produces (used by Tasks 9–10 and main.cpp): Q_PROPERTYs `playbackState` (int 0/1/2), `isPlaying`, `trackTitle`, `trackArtist`, `trackAlbum`, `trackPosition` (qint64 ms), `trackDuration` (qint64 ms), `artUrl`, `shuffle`, `repeatMode`, `hasTrack`, `folderModel` (QObject*); invokables `playFileFromFolder(QString)`, `playPause()`, `next()`, `previous()`, `pauseIfPlaying()`, `seekTo(qint64)`, `toggleShuffle()`, `cycleRepeat()`, `refreshSources()`; signals `playbackStateChanged()`, `metadataChanged()`, `progressChanged(qint64, qint64)`, `modesChanged()`, `hasTrackChanged()`, `playbackStarted()`.
- Config namespace `plugin_config.org.openauto.media-player`: keys `music_dirs` (list, default `["~/Music"]`), `last_queue`, `last_index`, `last_position_ms`, `shuffle`, `repeat_mode`.

- [ ] **Step 1: Write the plugin header**

`src/plugins/media_player/MediaPlayerPlugin.hpp`:

```cpp
#pragma once

#include "core/plugin/IPlugin.hpp"
#include <QObject>
#include <QString>

class QQmlContext;

namespace oap {

class IHostContext;

namespace plugins {

class FolderModel;
class MediaArtProvider;
class PlaybackEngine;
class PlayQueue;

/// Local media player: folder-browse playback of USB / ~/Music files.
/// Composition of PlaybackEngine (transport + PCM tap into AudioService),
/// PlayQueue (order/shuffle/repeat) and FolderModel (browse). Feeds
/// MediaStatusService as source "MediaPlayer" via main.cpp wiring.
/// Spec: docs/superpowers/specs/2026-07-08-media-player-design.md
class MediaPlayerPlugin : public QObject, public IPlugin {
    Q_OBJECT
    Q_INTERFACES(oap::IPlugin)

    Q_PROPERTY(int playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playbackStateChanged)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY metadataChanged)
    Q_PROPERTY(QString trackArtist READ trackArtist NOTIFY metadataChanged)
    Q_PROPERTY(QString trackAlbum READ trackAlbum NOTIFY metadataChanged)
    Q_PROPERTY(qint64 trackPosition READ trackPosition NOTIFY progressUpdated)
    Q_PROPERTY(qint64 trackDuration READ trackDuration NOTIFY progressUpdated)
    Q_PROPERTY(QString artUrl READ artUrl NOTIFY metadataChanged)
    Q_PROPERTY(bool shuffle READ shuffle NOTIFY modesChanged)
    Q_PROPERTY(int repeatMode READ repeatMode NOTIFY modesChanged)
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY hasTrackChanged)
    Q_PROPERTY(QObject* folderModel READ folderModelObject CONSTANT)

public:
    explicit MediaPlayerPlugin(QObject* parent = nullptr);
    ~MediaPlayerPlugin() override;

    // IPlugin — Identity
    QString id() const override { return QStringLiteral("org.openauto.media-player"); }
    QString name() const override { return QStringLiteral("Media Player"); }
    QString version() const override { return QStringLiteral("1.0.0"); }
    int apiVersion() const override { return 1; }

    // IPlugin — Lifecycle
    bool initialize(IHostContext* context) override;
    void shutdown() override;

    // IPlugin — Activation
    void onActivated(QQmlContext* context) override;
    void onDeactivated() override;

    // IPlugin — UI
    QUrl qmlComponent() const override;
    QUrl iconSource() const override { return {}; }
    QString iconText() const override { return QString(QChar(0xe030)); }  // library_music
    QStringList requiredServices() const override { return {}; }

    /// Non-owning; the QML engine owns the provider (main.cpp registers it).
    void setArtProvider(MediaArtProvider* provider) { artProvider_ = provider; }

    // Properties
    int playbackState() const;
    bool isPlaying() const { return playbackState() == 1; }
    QString trackTitle() const;
    QString trackArtist() const;
    QString trackAlbum() const;
    qint64 trackPosition() const;
    qint64 trackDuration() const;
    QString artUrl() const;
    bool shuffle() const;
    int repeatMode() const;
    bool hasTrack() const { return hasTrack_; }
    QObject* folderModelObject() const;

    // Controls (QML + main.cpp callback routing)
    Q_INVOKABLE void playFileFromFolder(const QString& path);
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void pauseIfPlaying();
    Q_INVOKABLE void seekTo(qint64 positionMs);
    Q_INVOKABLE void toggleShuffle();
    Q_INVOKABLE void cycleRepeat();
    Q_INVOKABLE void refreshSources();

signals:
    void playbackStateChanged();
    void metadataChanged();
    void progressChanged(qint64 positionMs, qint64 durationMs);
    void progressUpdated();   ///< argument-free companion for Q_PROPERTY NOTIFY
    void modesChanged();
    void hasTrackChanged();
    void playbackStarted();   ///< false->true playing edge, for pause-others wiring

private:
    void setHasTrack(bool has);
    void saveState();
    void restoreState();

    IHostContext* hostContext_ = nullptr;
    PlaybackEngine* engine_ = nullptr;
    PlayQueue* queue_ = nullptr;
    FolderModel* folderModel_ = nullptr;
    MediaArtProvider* artProvider_ = nullptr;  // non-owning
    bool hasTrack_ = false;
    bool wasPlaying_ = false;
    int consecutiveErrors_ = 0;
};

} // namespace plugins
} // namespace oap
```

- [ ] **Step 2: Write the plugin implementation**

`src/plugins/media_player/MediaPlayerPlugin.cpp`:

```cpp
#include "MediaPlayerPlugin.hpp"

#include <QDir>
#include <QLoggingCategory>
#include <QQmlContext>
#include <QStorageInfo>
#include <QUrl>

#include "FolderModel.hpp"
#include "MediaArtProvider.hpp"
#include "PlaybackEngine.hpp"
#include "PlayQueue.hpp"
#include "core/plugin/IHostContext.hpp"
#include "core/services/EqualizerService.hpp"
#include "core/services/IConfigService.hpp"
#include "core/services/INotificationService.hpp"

Q_LOGGING_CATEGORY(lcMediaPlayerPlugin, "oap.mediaplayer.plugin")

namespace {
const QString kPluginId = QStringLiteral("org.openauto.media-player");
constexpr int kMaxConsecutiveErrors = 3;
} // namespace

namespace oap {
namespace plugins {

MediaPlayerPlugin::MediaPlayerPlugin(QObject* parent) : QObject(parent) {}
MediaPlayerPlugin::~MediaPlayerPlugin() = default;

bool MediaPlayerPlugin::initialize(IHostContext* context) {
    hostContext_ = context;
    engine_ = new PlaybackEngine(this);
    queue_ = new PlayQueue(this);
    folderModel_ = new FolderModel(this);

    engine_->setAudioService(context->audioService());
    // Same EQ attach pattern as AndroidAutoPlugin.cpp:51 — the concrete
    // service exposes engineForStream(); local playback shares the Media EQ.
    if (auto* eqService = dynamic_cast<oap::EqualizerService*>(context->equalizerService()))
        engine_->setEqEngine(eqService->engineForStream(oap::StreamId::Media));

    // Auto-advance on track end.
    connect(engine_, &PlaybackEngine::trackFinished, this, [this]() {
        if (queue_->advance(false))
            engine_->playFile(queue_->currentTrack());
        // else: end of queue, repeat off — remain stopped on the last track.
    });

    // Unplayable-file policy (spec §11): skip forward; after 3 consecutive
    // failures stop and toast (a dead USB stick must not machine-gun skips).
    connect(engine_, &PlaybackEngine::errorOccurred, this, [this](const QString& msg) {
        ++consecutiveErrors_;
        if (consecutiveErrors_ >= kMaxConsecutiveErrors) {
            engine_->stop();
            if (hostContext_ && hostContext_->notificationService())
                hostContext_->notificationService()->notify(
                    QStringLiteral("Media Player"),
                    QStringLiteral("Playback stopped: %1 unplayable files in a row").arg(consecutiveErrors_));
            consecutiveErrors_ = 0;
            return;
        }
        qCWarning(lcMediaPlayerPlugin) << "skipping unplayable file:" << msg;
        if (queue_->advance(true))          // manual semantics: never re-loop one broken file
            engine_->playFile(queue_->currentTrack());
    });

    connect(engine_, &PlaybackEngine::playbackStateChanged, this, [this]() {
        const bool playing = (engine_->playbackState() == 1);
        if (playing && !wasPlaying_)
            emit playbackStarted();
        wasPlaying_ = playing;
        emit playbackStateChanged();
    });

    connect(engine_, &PlaybackEngine::metadataChanged, this, [this]() {
        if (artProvider_)
            artProvider_->setCurrentArt(engine_->coverArt());
        emit metadataChanged();
    });

    connect(engine_, &PlaybackEngine::progressChanged, this,
            [this](qint64 pos, qint64 dur) {
        if (pos > 500) consecutiveErrors_ = 0;  // decode demonstrably working
        emit progressChanged(pos, dur);
        emit progressUpdated();
    });

    connect(queue_, &PlayQueue::shuffleChanged, this, &MediaPlayerPlugin::modesChanged);
    connect(queue_, &PlayQueue::repeatModeChanged, this, &MediaPlayerPlugin::modesChanged);

    refreshSources();
    restoreState();
    return true;
}

void MediaPlayerPlugin::shutdown() {
    saveState();
    if (engine_) engine_->stop();
}

void MediaPlayerPlugin::onActivated(QQmlContext* context) {
    if (!context) return;
    context->setContextProperty("MediaPlayerPlugin", this);
    refreshSources();  // volumes may have been (un)mounted since last visit
}

void MediaPlayerPlugin::onDeactivated() {
    // Playback continues in the background; child context destroyed by host.
}

QUrl MediaPlayerPlugin::qmlComponent() const {
    return QUrl(QStringLiteral("qrc:/OpenAutoProdigy/MediaPlayerView.qml"));
}

int MediaPlayerPlugin::playbackState() const { return engine_ ? engine_->playbackState() : 0; }
QString MediaPlayerPlugin::trackTitle() const { return engine_ ? engine_->title() : QString(); }
QString MediaPlayerPlugin::trackArtist() const { return engine_ ? engine_->artist() : QString(); }
QString MediaPlayerPlugin::trackAlbum() const { return engine_ ? engine_->album() : QString(); }
qint64 MediaPlayerPlugin::trackPosition() const { return engine_ ? engine_->position() : 0; }
qint64 MediaPlayerPlugin::trackDuration() const { return engine_ ? engine_->duration() : 0; }
QString MediaPlayerPlugin::artUrl() const { return artProvider_ ? artProvider_->currentUrl() : QString(); }
bool MediaPlayerPlugin::shuffle() const { return queue_ && queue_->shuffle(); }
int MediaPlayerPlugin::repeatMode() const { return queue_ ? queue_->repeatMode() : 0; }
QObject* MediaPlayerPlugin::folderModelObject() const { return folderModel_; }

void MediaPlayerPlugin::setHasTrack(bool has) {
    if (hasTrack_ == has) return;
    hasTrack_ = has;
    emit hasTrackChanged();
}

void MediaPlayerPlugin::playFileFromFolder(const QString& path) {
    const QStringList files = folderModel_->audioFilesInCurrentDir();
    const int idx = files.indexOf(path);
    if (idx < 0) return;
    consecutiveErrors_ = 0;
    queue_->setTracks(files, idx);
    setHasTrack(true);
    engine_->playFile(path);
}

void MediaPlayerPlugin::playPause() {
    if (!hasTrack_) return;
    switch (engine_->playbackState()) {
    case 1:  engine_->pause(); break;
    case 2:  engine_->play(); break;
    default: engine_->playFile(queue_->currentTrack()); break;  // stopped: restart
    }
}

void MediaPlayerPlugin::next() {
    if (!hasTrack_) return;
    if (queue_->advance(true))
        engine_->playFile(queue_->currentTrack());
}

void MediaPlayerPlugin::previous() {
    if (!hasTrack_) return;
    // Classic head-unit behavior: >3 s into the track = restart it.
    if (engine_->position() > 3000) {
        engine_->seek(0);
    } else if (queue_->retreat()) {
        engine_->playFile(queue_->currentTrack());
    } else {
        engine_->seek(0);
    }
}

void MediaPlayerPlugin::pauseIfPlaying() {
    if (engine_ && engine_->playbackState() == 1)
        engine_->pause();
}

void MediaPlayerPlugin::seekTo(qint64 positionMs) {
    if (hasTrack_) engine_->seek(positionMs);
}

void MediaPlayerPlugin::toggleShuffle() { queue_->setShuffle(!queue_->shuffle()); }

void MediaPlayerPlugin::cycleRepeat() {
    queue_->setRepeatMode((queue_->repeatMode() + 1) % 3);
}

void MediaPlayerPlugin::refreshSources() {
    QVector<QPair<QString, QString>> roots;

    QStringList musicDirs;
    if (hostContext_ && hostContext_->configService()) {
        const QVariant v = hostContext_->configService()->pluginValue(
            kPluginId, QStringLiteral("music_dirs"));
        musicDirs = v.toStringList();
    }
    if (musicDirs.isEmpty())
        musicDirs << QStringLiteral("~/Music");
    for (QString dir : musicDirs) {
        if (dir.startsWith(QLatin1String("~/")))
            dir = QDir::homePath() + dir.mid(1);
        if (QDir(dir).exists())
            roots.append({QFileInfo(dir).fileName(), dir});
    }

    // Already-mounted removable volumes (stage 1: snapshot; stage 2 adds
    // udisks2 hot-plug + automount).
    for (const QStorageInfo& vol : QStorageInfo::mountedVolumes()) {
        if (!vol.isValid() || !vol.isReady() || vol.isRoot()) continue;
        const QString mount = vol.rootPath();
        if (!mount.startsWith(QLatin1String("/media/"))
            && !mount.startsWith(QLatin1String("/run/media/"))
            && !mount.startsWith(QLatin1String("/mnt/")))
            continue;
        QString label = vol.displayName();
        if (label.isEmpty() || label == mount)
            label = QFileInfo(mount).fileName();
        roots.append({label, mount});
    }

    folderModel_->setRoots(roots);
}

void MediaPlayerPlugin::saveState() {
    if (!hostContext_ || !hostContext_->configService()) return;
    auto* cfg = hostContext_->configService();
    cfg->setPluginValue(kPluginId, QStringLiteral("last_queue"), queue_->tracks());
    cfg->setPluginValue(kPluginId, QStringLiteral("last_index"), queue_->currentIndex());
    cfg->setPluginValue(kPluginId, QStringLiteral("last_position_ms"),
                        hasTrack_ ? engine_->position() : qint64(0));
    cfg->setPluginValue(kPluginId, QStringLiteral("shuffle"), queue_->shuffle());
    cfg->setPluginValue(kPluginId, QStringLiteral("repeat_mode"), queue_->repeatMode());
    cfg->save();
}

void MediaPlayerPlugin::restoreState() {
    if (!hostContext_ || !hostContext_->configService()) return;
    auto* cfg = hostContext_->configService();
    queue_->setShuffle(cfg->pluginValue(kPluginId, QStringLiteral("shuffle")).toBool());
    queue_->setRepeatMode(cfg->pluginValue(kPluginId, QStringLiteral("repeat_mode")).toInt());

    const QStringList saved = cfg->pluginValue(kPluginId, QStringLiteral("last_queue")).toStringList();
    if (saved.isEmpty()) return;
    // Files may have vanished (USB stick removed) — keep only what exists.
    QStringList existing;
    const QString savedCurrent =
        saved.value(cfg->pluginValue(kPluginId, QStringLiteral("last_index")).toInt());
    for (const QString& p : saved)
        if (QFileInfo::exists(p)) existing << p;
    if (existing.isEmpty()) return;
    int startIdx = existing.indexOf(savedCurrent);
    if (startIdx < 0) startIdx = 0;
    queue_->setTracks(existing, startIdx);
    setHasTrack(true);
    // Restore PAUSED at the saved position — nothing auto-plays on boot
    // (spec §10, Matthew's explicit call). One tap on play resumes.
    const qint64 pos = cfg->pluginValue(kPluginId, QStringLiteral("last_position_ms")).toLongLong();
    engine_->restorePaused(queue_->currentTrack(), pos);
}

} // namespace plugins
} // namespace oap
```

Note: check `INotificationService`'s actual notify method signature before building (`src/core/services/INotificationService.hpp`) — if it differs from `notify(title, body)`, adapt the single call site in the error handler to match (the semantics stay: title "Media Player", body with the count).

- [ ] **Step 3: Placeholder view + CMake registration**

`qml/applications/media_player/MediaPlayerView.qml` (placeholder — Task 9 replaces it):

```qml
import QtQuick

Item {
    NormalText {
        anchors.centerIn: parent
        text: "Media Player — view lands in Task 9"
        color: ThemeService.onSurface
    }
}
```

`src/CMakeLists.txt` — three places:

1. Source list (after `plugins/media_player/MediaArtProvider.cpp`):

```cmake
    plugins/media_player/MediaPlayerPlugin.cpp
```

2. QML source properties (after the `BtAudioView.qml` `set_source_files_properties` block):

```cmake
set_source_files_properties(../qml/applications/media_player/MediaPlayerView.qml PROPERTIES
    QT_QML_SOURCE_TYPENAME "MediaPlayerView"
    QT_RESOURCE_ALIAS "MediaPlayerView.qml"
)
```

3. QML file list (after the `../qml/applications/bt_audio/BtAudioView.qml` line):

```cmake
        ../qml/applications/media_player/MediaPlayerView.qml
```

- [ ] **Step 4: main.cpp integration (5 insertion points)**

Add the include next to the other plugin includes at the top of `src/main.cpp`:

```cpp
#include "plugins/media_player/MediaPlayerPlugin.hpp"
#include "plugins/media_player/MediaArtProvider.hpp"
```

**(a) Registration** — directly after the `btAudioPlugin` registration (anchor: `pluginManager.registerStaticPlugin(btAudioPlugin);`, ~line 519):

```cpp
    auto mediaPlayerPlugin = new oap::plugins::MediaPlayerPlugin(&app);
    auto* mediaArtProvider = new oap::plugins::MediaArtProvider();
    mediaPlayerPlugin->setArtProvider(mediaArtProvider);  // non-owning; engine owns it (see addImageProvider below)
    pluginManager.registerStaticPlugin(mediaPlayerPlugin);
```

**(b) Provider wiring** — after the BT wiring block (anchor: the closing brace of `if (btAudioPlugin->connectionState() == 1) { ... }`, ~line 615), add:

```cpp
    // Wire MediaStatusService to the local media player plugin
    QObject::connect(mediaPlayerPlugin, &oap::plugins::MediaPlayerPlugin::metadataChanged,
                     mediaStatusService, [mediaStatusService, mediaPlayerPlugin]() {
        mediaStatusService->updateMediaPlayerMetadata(mediaPlayerPlugin->trackTitle(),
                                                      mediaPlayerPlugin->trackArtist(),
                                                      mediaPlayerPlugin->trackAlbum());
        mediaStatusService->updateMediaPlayerArt(mediaPlayerPlugin->artUrl());
    });
    QObject::connect(mediaPlayerPlugin, &oap::plugins::MediaPlayerPlugin::playbackStateChanged,
                     mediaStatusService, [mediaStatusService, mediaPlayerPlugin]() {
        mediaStatusService->updateMediaPlayerPlaybackState(mediaPlayerPlugin->playbackState());
    });
    QObject::connect(mediaPlayerPlugin, &oap::plugins::MediaPlayerPlugin::progressChanged,
                     mediaStatusService, [mediaStatusService](qint64 pos, qint64 dur) {
        mediaStatusService->updateMediaPlayerProgress(pos, dur);
    });
    QObject::connect(mediaPlayerPlugin, &oap::plugins::MediaPlayerPlugin::hasTrackChanged,
                     mediaStatusService, [mediaStatusService, mediaPlayerPlugin]() {
        mediaStatusService->setMediaPlayerConnected(mediaPlayerPlugin->hasTrack());
    });
    if (mediaPlayerPlugin->hasTrack()) {   // queue restored at initialize()
        mediaStatusService->setMediaPlayerConnected(true);
        mediaStatusService->updateMediaPlayerMetadata(mediaPlayerPlugin->trackTitle(),
                                                      mediaPlayerPlugin->trackArtist(),
                                                      mediaPlayerPlugin->trackAlbum());
        mediaStatusService->updateMediaPlayerPlaybackState(mediaPlayerPlugin->playbackState());
    }

    // BT progress into the widened surface (cheap win — BtAudioPlugin already
    // tracks position/duration from AVRCP)
    QObject::connect(btAudioPlugin, &oap::plugins::BtAudioPlugin::positionChanged,
                     mediaStatusService, [mediaStatusService, btAudioPlugin]() {
        mediaStatusService->updateBtProgress(btAudioPlugin->trackPosition(),
                                             btAudioPlugin->trackDuration());
    });
```

**(c) Callback routing** — REPLACE the three lambdas in the `mediaStatusService->setPlaybackCallbacks(...)` block (~lines 620–641) with (each gains `mediaPlayerPlugin` in the capture and a `MediaPlayer` branch):

```cpp
        mediaStatusService->setPlaybackCallbacks(
            [mediaStatusService, orch, btAudioPlugin, mediaPlayerPlugin]() {
                if (mediaStatusService->source() == "AndroidAuto" && orch)
                    orch->sendButtonPress(85);
                else if (mediaStatusService->source() == "Bluetooth" && btAudioPlugin) {
                    if (btAudioPlugin->playbackState() == 1) btAudioPlugin->pause();
                    else btAudioPlugin->play();
                }
                else if (mediaStatusService->source() == "MediaPlayer" && mediaPlayerPlugin)
                    mediaPlayerPlugin->playPause();
            },
            [mediaStatusService, orch, btAudioPlugin, mediaPlayerPlugin]() {
                if (mediaStatusService->source() == "AndroidAuto" && orch)
                    orch->sendButtonPress(87);
                else if (mediaStatusService->source() == "Bluetooth" && btAudioPlugin)
                    btAudioPlugin->next();
                else if (mediaStatusService->source() == "MediaPlayer" && mediaPlayerPlugin)
                    mediaPlayerPlugin->next();
            },
            [mediaStatusService, orch, btAudioPlugin, mediaPlayerPlugin]() {
                if (mediaStatusService->source() == "AndroidAuto" && orch)
                    orch->sendButtonPress(88);
                else if (mediaStatusService->source() == "Bluetooth" && btAudioPlugin)
                    btAudioPlugin->previous();
                else if (mediaStatusService->source() == "MediaPlayer" && mediaPlayerPlugin)
                    mediaPlayerPlugin->previous();
            }
        );
```

**(d) Pause-others policy** — directly after the callback block's closing `}`:

```cpp
    // One audible music source at a time (spec §6): starting one pauses the
    // others. AA-side: pausing the PHONE's media on local play-start is Task
    // 11's investigation; here we only pause local when AA reports playing.
    QObject::connect(mediaPlayerPlugin, &oap::plugins::MediaPlayerPlugin::playbackStarted,
                     btAudioPlugin, [btAudioPlugin]() {
        if (btAudioPlugin->playbackState() == 1) btAudioPlugin->pause();
    });
    QObject::connect(btAudioPlugin, &oap::plugins::BtAudioPlugin::playbackStateChanged,
                     mediaPlayerPlugin, [mediaPlayerPlugin, btAudioPlugin]() {
        if (btAudioPlugin->playbackState() == 1) mediaPlayerPlugin->pauseIfPlaying();
    });
    if (auto* orchForPolicy = aaPlugin->orchestrator()) {
        if (auto* mshForPolicy = orchForPolicy->mediaStatusHandler()) {
            QObject::connect(mshForPolicy, &oaa::hu::MediaStatusChannelHandler::playbackStateChanged,
                             mediaPlayerPlugin, [mediaPlayerPlugin](int state, const QString&) {
                if (state == 2) mediaPlayerPlugin->pauseIfPlaying();  // AA raw 2 = playing
            }, Qt::QueuedConnection);
        }
    }
```

**(e) Image provider** — next to the existing provider registration (anchor: `engine.addImageProvider(QStringLiteral("navicon"), maneuverIconProvider);`, ~line 1125):

```cpp
    engine.addImageProvider(QStringLiteral("mediaart"), mediaArtProvider);  // engine takes ownership
```

- [ ] **Step 5: Build everything, run the full suite**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: clean build, all tests pass (the new plugin compiles into `openauto-core`; nav strip gains a "Media Player" entry at runtime).

- [ ] **Step 6: Commit**

```bash
git add src/plugins/media_player/MediaPlayerPlugin.hpp src/plugins/media_player/MediaPlayerPlugin.cpp \
        qml/applications/media_player/ src/CMakeLists.txt src/main.cpp
git commit -m "feat(media-player): MediaPlayerPlugin — registration, wiring, pause-others policy"
```

---

### Task 9: MediaPlayerView.qml — folder browse + now-playing bar

Replaces the Task 8 placeholder. Touch-first: 64px list rows, 56px transport buttons, tap-to-seek strip. Uses the same context-property idiom as `BtAudioView.qml` and the shared controls (`NormalText`, `MaterialIcon`, `ThemeService`, `UiMetrics`).

**Files:**
- Rewrite: `qml/applications/media_player/MediaPlayerView.qml`

**Interfaces:**
- Consumes: `MediaPlayerPlugin` context property (Task 8 surface), `FolderModel` roles (`name`, `path`, `isDir`) and invokables (`enter`, `up`, `refreshSources` via plugin).
- Produces: the stage-1 player UI. Layout leaves the header row shaped so stage 2 can add Artists/Albums/Tracks tabs beside the breadcrumb.

- [ ] **Step 1: Write the full view**

Replace `qml/applications/media_player/MediaPlayerView.qml` with:

```qml
import QtQuick

// Local media player — stage 1: Folders browse + persistent now-playing bar.
// Bound to the MediaPlayerPlugin context property (set in onActivated).
// Stage 2 adds Artists/Albums/Tracks tabs in the header row.
Item {
    id: mediaPlayerView

    readonly property var plugin: typeof MediaPlayerPlugin !== "undefined" ? MediaPlayerPlugin : null
    readonly property var folders: plugin ? plugin.folderModel : null

    function fmtTime(ms) {
        if (ms <= 0) return "0:00"
        var s = Math.floor(ms / 1000)
        var m = Math.floor(s / 60)
        s = s % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    // ---- Header: back + breadcrumb + refresh ----
    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 56

        Item {
            id: backButton
            width: 56; height: parent.height
            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue5c4"  // arrow_back
                size: 28
                color: ThemeService.onSurface
                opacity: folders && !folders.atTopLevel ? 1.0 : 0.3
            }
            MouseArea { anchors.fill: parent; onClicked: if (folders) folders.up() }
        }

        NormalText {
            anchors.left: backButton.right
            anchors.right: refreshButton.left
            anchors.verticalCenter: parent.verticalCenter
            text: folders ? folders.breadcrumb : ""
            font.pixelSize: 22
            font.weight: Font.Medium
            color: ThemeService.onSurface
            elide: Text.ElideLeft
        }

        Item {
            id: refreshButton
            anchors.right: parent.right
            width: 56; height: parent.height
            MaterialIcon {
                anchors.centerIn: parent
                icon: "\ue5d5"  // refresh
                size: 26
                color: ThemeService.onSurfaceVariant
            }
            MouseArea { anchors.fill: parent; onClicked: if (plugin) plugin.refreshSources() }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: ThemeService.onSurfaceVariant
            opacity: 0.15
        }
    }

    // ---- Browse list ----
    ListView {
        id: browseList
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: nowPlayingBar.top
        clip: true
        model: mediaPlayerView.folders

        delegate: Item {
            width: browseList.width
            height: 64

            MaterialIcon {
                id: rowIcon
                anchors.left: parent.left
                anchors.leftMargin: UiMetrics.spacing
                anchors.verticalCenter: parent.verticalCenter
                icon: model.isDir ? "\ue2c7" : "\ue405"  // folder / music_note
                size: 30
                color: model.isDir ? ThemeService.primary : ThemeService.onSurfaceVariant
            }

            NormalText {
                anchors.left: rowIcon.right
                anchors.leftMargin: UiMetrics.spacing
                anchors.right: parent.right
                anchors.rightMargin: UiMetrics.spacing
                anchors.verticalCenter: parent.verticalCenter
                text: model.name
                font.pixelSize: 20
                color: ThemeService.onSurface
                elide: Text.ElideRight
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: ThemeService.onSurfaceVariant
                opacity: 0.10
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (model.isDir) mediaPlayerView.folders.enter(model.path)
                    else if (mediaPlayerView.plugin) mediaPlayerView.plugin.playFileFromFolder(model.path)
                }
            }
        }

        NormalText {
            visible: browseList.count === 0
            anchors.centerIn: parent
            text: folders && folders.atTopLevel
                  ? "No music sources found.\nAdd files to ~/Music or plug in a USB drive."
                  : "No playable files here."
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 18
            color: ThemeService.onSurfaceVariant
        }
    }

    // ---- Now-playing bar ----
    Rectangle {
        id: nowPlayingBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: plugin && plugin.hasTrack ? 112 : 0
        visible: height > 0
        color: ThemeService.surface

        // Seek strip: 6px visual bar with a 24px touch strip over it.
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 6
            color: ThemeService.onSurfaceVariant
            opacity: 0.25
        }
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            height: 6
            width: plugin && plugin.trackDuration > 0
                   ? parent.width * Math.min(1, plugin.trackPosition / plugin.trackDuration)
                   : 0
            color: ThemeService.primary
        }
        MouseArea {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 24
            onClicked: function(mouse) {
                if (mediaPlayerView.plugin && mediaPlayerView.plugin.trackDuration > 0)
                    mediaPlayerView.plugin.seekTo(Math.round(mouse.x / width * mediaPlayerView.plugin.trackDuration))
            }
        }

        // Cover art
        Image {
            id: barArt
            anchors.left: parent.left
            anchors.leftMargin: UiMetrics.spacing
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 3   // visually below the seek strip
            width: 80; height: 80
            source: plugin ? plugin.artUrl : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            visible: status === Image.Ready
        }
        MaterialIcon {
            anchors.centerIn: barArt
            visible: !barArt.visible
            icon: "\ue405"  // music_note placeholder
            size: 44
            color: ThemeService.onSurfaceVariant
            opacity: 0.4
        }

        // Title / artist / time
        Column {
            anchors.left: barArt.right
            anchors.leftMargin: UiMetrics.spacing
            anchors.right: transportRow.left
            anchors.rightMargin: UiMetrics.spacing
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 3
            spacing: 2

            NormalText {
                width: parent.width
                text: plugin ? plugin.trackTitle : ""
                font.pixelSize: 22
                font.weight: Font.Bold
                color: ThemeService.onSurface
                elide: Text.ElideRight
            }
            NormalText {
                width: parent.width
                text: plugin ? plugin.trackArtist : ""
                visible: text.length > 0
                font.pixelSize: 17
                color: ThemeService.onSurfaceVariant
                elide: Text.ElideRight
            }
            NormalText {
                text: plugin ? fmtTime(plugin.trackPosition) + " / " + fmtTime(plugin.trackDuration) : ""
                font.pixelSize: 14
                color: ThemeService.onSurfaceVariant
            }
        }

        // Transport + modes
        Row {
            id: transportRow
            anchors.right: parent.right
            anchors.rightMargin: UiMetrics.spacing
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 3
            spacing: UiMetrics.spacing * 0.75

            Item {
                width: 48; height: 56
                MaterialIcon {
                    anchors.centerIn: parent
                    icon: "\ue043"  // shuffle
                    size: 26
                    color: plugin && plugin.shuffle ? ThemeService.primary : ThemeService.onSurfaceVariant
                }
                MouseArea { anchors.fill: parent; onClicked: if (plugin) plugin.toggleShuffle() }
            }

            Item {
                width: 56; height: 56
                MaterialIcon {
                    anchors.centerIn: parent
                    icon: "\ue045"  // skip_previous
                    size: 34
                    color: ThemeService.onSurface
                }
                MouseArea { anchors.fill: parent; onClicked: if (plugin) plugin.previous() }
            }

            Item {
                width: 64; height: 64
                anchors.verticalCenter: parent.verticalCenter
                MaterialIcon {
                    anchors.centerIn: parent
                    icon: plugin && plugin.isPlaying ? "\ue034" : "\ue037"  // pause / play_arrow
                    size: 44
                    color: ThemeService.primary
                }
                MouseArea { anchors.fill: parent; onClicked: if (plugin) plugin.playPause() }
            }

            Item {
                width: 56; height: 56
                MaterialIcon {
                    anchors.centerIn: parent
                    icon: "\ue044"  // skip_next
                    size: 34
                    color: ThemeService.onSurface
                }
                MouseArea { anchors.fill: parent; onClicked: if (plugin) plugin.next() }
            }

            Item {
                width: 48; height: 56
                MaterialIcon {
                    anchors.centerIn: parent
                    // repeat / repeat_one; dimmed when off
                    icon: plugin && plugin.repeatMode === 2 ? "\ue041" : "\ue040"
                    size: 26
                    color: plugin && plugin.repeatMode !== 0 ? ThemeService.primary : ThemeService.onSurfaceVariant
                }
                MouseArea { anchors.fill: parent; onClicked: if (plugin) plugin.cycleRepeat() }
            }
        }
    }
}
```

(If any Material codepoint renders as a hollow box on the bench, swap it for one already proven in this codebase — e.g. reuse `\ue045`/`\ue044`/`\ue034`/`\ue037` from NowPlayingWidget, which are known-good — rather than hunting font tables.)

- [ ] **Step 2: Build + QML smoke**

Run: `cd build && cmake .. && make -j$(nproc) && timeout 15 ./src/openauto-prodigy 2>&1 | grep -iE "qml|MediaPlayer" | head -20`
Expected: build succeeds; no `MediaPlayerView.qml` errors in the output (the app itself may exit early on WSL for unrelated reasons — only QML errors matter here). Then `ctest --output-on-failure` still green.

- [ ] **Step 3: Commit**

```bash
git add qml/applications/media_player/MediaPlayerView.qml
git commit -m "feat(media-player): folder-browse view with now-playing bar"
```

---

### Task 10: NowPlayingWidget upgrade — art, progress, source badge, isPlaying fix

Surgical edits to `qml/widgets/NowPlayingWidget.qml`. Also fixes the pre-existing bug where the play/pause icon used `playbackState === 1`, which is *stopped* for AA (AA playing = 2) — the widget now uses the normalized `isPlaying` from Task 4.

**Files:**
- Modify: `qml/widgets/NowPlayingWidget.qml`

**Interfaces:**
- Consumes: Task 4 provider surface via `widgetContext.mediaStatus` (`isPlaying`, `artUrl`, `hasPosition`, `position`, `duration`).

- [ ] **Step 1: Fix isPlaying (exact replacement)**

Old:

```qml
    property bool isPlaying: widgetContext && widgetContext.mediaStatus
                             ? widgetContext.mediaStatus.playbackState === 1 : false
```

New:

```qml
    property bool isPlaying: widgetContext && widgetContext.mediaStatus
                             ? widgetContext.mediaStatus.isPlaying === true : false
```

- [ ] **Step 2: Add the new provider properties (insert directly after the `artist` property binding)**

After:

```qml
    property string artist: widgetContext && widgetContext.mediaStatus
                            ? (widgetContext.mediaStatus.artist || "") : ""
```

insert:

```qml
    property string artUrl: widgetContext && widgetContext.mediaStatus
                            ? (widgetContext.mediaStatus.artUrl || "") : ""
    property bool hasPosition: widgetContext && widgetContext.mediaStatus
                               ? widgetContext.mediaStatus.hasPosition === true : false
    property real trackPosition: widgetContext && widgetContext.mediaStatus
                                 ? widgetContext.mediaStatus.position : -1
    property real trackDuration: widgetContext && widgetContext.mediaStatus
                                 ? widgetContext.mediaStatus.duration : 0
```

- [ ] **Step 3: Add the local-source icon (extend the codepoint block)**

Old:

```qml
    // Source icon codepoints
    readonly property string btIcon: "\uf032"       // media_bluetooth_on
    readonly property string aaIcon: "\ue859"       // android
```

New:

```qml
    // Source icon codepoints
    readonly property string btIcon: "\uf032"       // media_bluetooth_on
    readonly property string aaIcon: "\ue859"       // android
    readonly property string localIcon: "\ue030"    // library_music (local media player)
```

- [ ] **Step 4: Source badge + progress bar (insert before the final long-press MouseArea)**

Directly before:

```qml
    // Long-press for context menu (edit mode)
    MouseArea {
```

insert:

```qml
    // Source badge (top-right): which source owns the display right now
    MaterialIcon {
        visible: nowPlayingWidget.hasMedia && nowPlayingWidget.mediaSource !== ""
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: UiMetrics.spacing * 0.5
        icon: nowPlayingWidget.mediaSource === "Bluetooth" ? nowPlayingWidget.btIcon
            : nowPlayingWidget.mediaSource === "AndroidAuto" ? nowPlayingWidget.aaIcon
            : nowPlayingWidget.localIcon
        size: Math.max(14, nowPlayingWidget.height * 0.10)
        color: ThemeService.onSurfaceVariant
        opacity: 0.7
    }

    // Track progress along the bottom edge (only when the source reports it).
    // Container is an Item, NOT a Rectangle: child opacity multiplies under a
    // translucent parent, so track and fill must be siblings.
    Item {
        visible: nowPlayingWidget.hasMedia && nowPlayingWidget.hasPosition
                 && nowPlayingWidget.trackDuration > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 3

        Rectangle {  // track
            anchors.fill: parent
            color: ThemeService.onSurfaceVariant
            opacity: 0.25
        }
        Rectangle {  // fill
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * Math.min(1, nowPlayingWidget.trackPosition / nowPlayingWidget.trackDuration)
            color: ThemeService.primary
            opacity: 0.85
        }
    }
```

- [ ] **Step 5: Cover art in the tall layout (exact replacement)**

Old:

```qml
        // Top region: title + artist — vertically centered (hidden at 1x1)
        Column {
            visible: nowPlayingWidget.colSpan >= 2 || nowPlayingWidget.rowSpan >= 2
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: controlRegion.top
            anchors.bottomMargin: UiMetrics.spacing
```

New:

```qml
        // Cover art (tall layout, 3+ cols, when the source provides it)
        Image {
            id: tallArt
            visible: nowPlayingWidget.colSpan >= 3 && nowPlayingWidget.artUrl !== ""
                     && status === Image.Ready
            source: nowPlayingWidget.colSpan >= 3 ? nowPlayingWidget.artUrl : ""
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: controlRegion.top
            anchors.bottomMargin: UiMetrics.spacing
            width: visible ? height : 0
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }

        // Top region: title + artist — vertically centered (hidden at 1x1)
        Column {
            visible: nowPlayingWidget.colSpan >= 2 || nowPlayingWidget.rowSpan >= 2
            anchors.left: tallArt.visible ? tallArt.right : parent.left
            anchors.leftMargin: tallArt.visible ? UiMetrics.spacing : 0
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: controlRegion.top
            anchors.bottomMargin: UiMetrics.spacing
```

- [ ] **Step 6: Cover art in the single-row layout (exact replacement)**

Old:

```qml
        // Metadata — fills available width
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
```

New:

```qml
        // Cover art thumbnail (wide row layouts, when available)
        Image {
            source: nowPlayingWidget.isWide ? nowPlayingWidget.artUrl : ""
            visible: nowPlayingWidget.isWide && nowPlayingWidget.artUrl !== ""
                     && status === Image.Ready
            Layout.preferredWidth: visible ? nowPlayingWidget.height * 0.8 : 0
            Layout.preferredHeight: Layout.preferredWidth
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }

        // Metadata — fills available width
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
```

- [ ] **Step 7: Build + smoke, commit**

Run: `cd build && cmake .. && make -j$(nproc) && timeout 15 ./src/openauto-prodigy 2>&1 | grep -iE "NowPlaying|qml.*error" | head`
Expected: no NowPlayingWidget QML errors in the output. Then `ctest --output-on-failure` still green.

```bash
git add qml/widgets/NowPlayingWidget.qml
git commit -m "feat(widgets): NowPlayingWidget art + progress + source badge; fix AA isPlaying"
```

---

### Task 11: AA audio-focus push investigation (timeboxed, ~1–2h)

Spec §6 plan-verify item (b): can the HU tell an AA-playing phone to pause its media when local playback starts? This is an investigation with two documented outcomes — not open-ended.

**Files:**
- Read-only: `libs/prodigy-oaa-protocol/` (submodule — NEVER modify), `src/core/aa/`
- Possibly modify: `src/core/aa/AndroidAutoOrchestrator.hpp/.cpp`, `src/main.cpp` (pause-others block)
- Always modify: `docs/session-handoffs.md` (findings), `docs/wishlist.md` (if deferred)

- [ ] **Step 1: Map the existing audio-focus plumbing**

```bash
grep -rn "AudioFocus" libs/prodigy-oaa-protocol/include/ | head -30
grep -rn "AudioFocus\|audioFocus" src/core/aa/ | head -30
```

Answer three questions:
1. Where does the HU handle the phone's `AudioFocusRequest` (grant-all? policy?)
2. Is there an existing send path for an HU-initiated focus **notification/state change** (e.g. `AUDIO_FOCUS_STATE_LOSS`, `LOSS_TRANSIENT`, `GAIN`) on the control/media channel?
3. What message + enum names does the proto define for focus states?

- [ ] **Step 2: Decision gate**

Implement ONLY if BOTH hold:
- The send is a **small addition** (≤ ~30 lines) reusing existing channel/messenger plumbing — e.g. a `void notifyAudioFocusLoss()` / `notifyAudioFocusGain()` pair on the orchestrator.
- It can be bench-tested same-day (Pi + one phone playing AA media → local play → phone media pauses; local stop → focus GAIN → phone can resume; AA session must not wedge).

Otherwise: record findings in `docs/session-handoffs.md`, add a wishlist entry ("AA focus push-to-phone for local playback coexistence — investigated 2026-07-XX, blocked by <finding>"), and move on. Shipping stage 1 without it is explicitly acceptable (spec §6: "annoying, not broken").

- [ ] **Step 3 (only if implementing): wire into the pause-others block**

In `src/main.cpp`, extend the Task 8(d) `playbackStarted` handler. Scope note: Task 8(d) declares `orchForPolicy` inside its own `if` AFTER this connect — hoist `auto* orchForPolicy = aaPlugin->orchestrator();` above the `playbackStarted` connect so the lambda can capture it:

```cpp
    QObject::connect(mediaPlayerPlugin, &oap::plugins::MediaPlayerPlugin::playbackStarted,
                     btAudioPlugin, [btAudioPlugin, orchForPolicy]() {
        if (btAudioPlugin->playbackState() == 1) btAudioPlugin->pause();
        if (orchForPolicy && orchForPolicy->isAaConnected())
            orchForPolicy->notifyAudioFocusLoss();   // phone pauses its media session
    });
```

plus the matching `notifyAudioFocusGain()` on local stop/pause if bench behavior warrants it. Keep the exact semantics that bench-test cleanly; document what the phone actually did per model.

- [ ] **Step 4: Record + commit**

```bash
git add -A docs/ src/  # whatever this task actually touched
git commit -m "feat(aa)|docs(aa): audio-focus push investigation — <verdict>"
```

---

### Task 12: Integration verification — full suite, cross-build, Pi deploy, bench checklist

**Files:**
- Modify: `docs/session-handoffs.md` (bench results), `docs/roadmap-current.md` (stage-1 status)

Reminder: the Pi (192.168.1.149) is a fresh Trixie install with NO build dir (fresh clone) — create target dirs before rsync. QML is loaded from disk on the Pi, so rsync `qml/` alongside the binary (mid-arc; the commits go over git after review).

- [ ] **Step 1: Full local suite**

Run: `cd build && cmake .. && make -j$(nproc) && ctest --output-on-failure`
Expected: 100% pass (was 88 tests; now 92 with the four new ones).

- [ ] **Step 2: Cross-build**

Run: `./cross-build.sh`
Expected: `build-pi/src/openauto-prodigy` produced (~4–6 min, app-only fast mode).

- [ ] **Step 3: Deploy to the bench Pi**

```bash
ssh matt@192.168.1.149 'mkdir -p ~/openauto-prodigy/build/src ~/Music'
rsync -av build-pi/src/openauto-prodigy matt@192.168.1.149:~/openauto-prodigy/build/src/
rsync -av qml/ matt@192.168.1.149:~/openauto-prodigy/qml/
# Seed bench audio: real music beats sine tones for the audible checks.
# Minimum viable: the test fixtures.
rsync -av tests/data/media/ matt@192.168.1.149:'~/Music/fixtures/'
ssh matt@192.168.1.149 'sudo systemctl restart openauto-prodigy.service'
ssh matt@192.168.1.149 'journalctl -u openauto-prodigy.service -n 50 --no-pager'
```

Expected: service starts; journal shows the Media Player plugin registered; no QML errors.

- [ ] **Step 4: Bench checklist (spec §12 stage 1)**

Record each row's result in `docs/session-handoffs.md`. Items marked **[M]** need Matthew present (audible judgment / phone in hand).

| # | Check | How |
|---|-------|-----|
| 1 | Media Player appears in nav; Folders browse works by touch | Navigate sources → dirs → files on the touchscreen |
| 2 | Local file plays with audio out | Tap a file in ~/Music |
| 3 | **[M]** EQ preset audibly changes local playback | Settings → EQ, swap presets mid-song |
| 4 | Master volume applies to local playback | Adjust master volume while playing |
| 5 | Now-playing bar: art, title, progress advance, seek works | Observe + tap seek strip |
| 6 | Dashboard NowPlayingWidget: art, progress, source badge, play state correct | Switch to dashboard while playing |
| 7 | Widget transport controls drive local playback | Tap prev/play/next on the widget |
| 8 | **[M]** BT pauses when local starts; local pauses when BT starts | Phone A2DP playing → start local; then reverse |
| 9 | **[M]** AA nav prompt ducks local audio (not pause) | AA session + local music + navigation prompt |
| 10 | API v1 media stream reports LOCAL_MEDIA + position fields | `websocat ws://192.168.1.149:9811` (or the api-client test tool) while playing |
| 11 | State restores paused after service restart | Play, `sudo systemctl restart openauto-prodigy.service`, verify paused-at-position, one tap resumes |
| 12 | Unplayable-file policy | Drop a `garbage.mp3` (e.g. `echo junk > ~/Music/garbage.mp3`) in a folder, play through it: skips once; three garbage files in a row: stops with toast |

Note phone re-pairing may be needed first (fresh install) — budget 5 min for the standard pairing flow before rows 8–9.

- [ ] **Step 5: Record results + update docs + final commit**

- `docs/session-handoffs.md`: bench outcomes, any deviations from plan, Task 11 verdict.
- `docs/roadmap-current.md`: update the media-player Now item — stage 1 shipped, stage 2 (library + automount) pending planning.

```bash
git add docs/session-handoffs.md docs/roadmap-current.md
git commit -m "docs: media player stage 1 bench results + roadmap status"
```

Do NOT push. Next step after this task: superpowers:requesting-code-review, then push on pass, then plan stage 2.

---

## Post-plan notes for the executor

- **Task order:** 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10 → 11 → 12. Tasks 2/3 are independent of each other; 4 must precede 5; 6 needs Task 1's verdict; 8 needs 2, 3, 6, 7; 10 needs 4 (and 8 for end-to-end observation).
- **Task 1 NO-GO is a hard stop** — report to Matthew, don't improvise a fallback beyond the two documented in Task 1 Step 4.
- The `INotificationService::notify` call in Task 8 and the `websocat` usage in Task 12 are the two spots where the plan trusts an interface it didn't pin down — check the actual signature/tooling in-repo and adapt locally (semantics as stated).
- Spec §13.5 closure: no task uses QML Multimedia (`MediaPlayer`/`VideoOutput` QML types) — all Qt Multimedia use is C++-side, so `qml6-module-qtmultimedia` is NOT needed and the installer is untouched. Keep it that way; if you find yourself importing QtMultimedia in QML, stop and reconsider.
- Deviations get recorded in `docs/session-handoffs.md` as you go, per this repo's AGENTS.md loop.




