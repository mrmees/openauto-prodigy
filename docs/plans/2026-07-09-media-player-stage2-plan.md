# Media Player Stage 2 (Library + USB Automount) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

Status: ACTIVE
Date: 2026-07-09
Design: `docs/plans/2026-07-08-media-player-design.md` §§4/7/8/9/12 + the
2026-07-09 prior-art amendment (§8 heuristics; credits requirements).
Grounded: abb9542 (dev). Stage-1 seams re-verified against this commit —
the design's `MediaSourceWatcher` does NOT exist as a class (it is the
inline loop in `MediaPlayerPlugin::refreshSources()`,
`MediaPlayerPlugin.cpp:231-265`), and the playback-policy state lives in
MediaPlayerPlugin (NOT PlaybackEngine).
Scope decision (Matthew, 2026-07-09): wishlist item "Extract playback-policy
state machine" is PROMOTED into this plan (Task 1).

**Goal:** Library tabs (Artists/Albums/Tracks) backed by a libavformat
scanner with incremental cache, plus udisks2 USB hot-plug/automount with
safe-eject and yank-mid-playback recovery.

**Architecture:** New pure-logic `PlaybackPolicy` extracted from
MediaPlayerPlugin's lambdas (Task 1) so yank recovery lands on tested code.
`MediaTagReader` (libavformat) reads tags/art; `MediaLibrary` builds the
Artist→Album→Track index and exposes three `QAbstractListModel`s;
`MediaScanner` runs cold/incremental scans on a worker thread with a
QDataStream per-volume cache; `UsbMediaWatcher` mirrors the BlueZ
ObjectManager pattern against udisks2. All new sources compile into
`openauto-core` like the stage-1 files.

**Tech Stack:** Qt 6.8 (Core/Multimedia/DBus already linked), libavformat
(NEW pkg-config module — same FFmpeg family already used), QML, udisks2
(Pi runtime service), QtTest.

## Global Constraints

- Repo: `/mnt/e/claude/personal/openautopro/openauto-prodigy`. Build ONLY in
  `~/builds/openauto-prodigy` (ext4). NEVER create or use an in-repo `build/`
  dir (9p mount).
- `ctest` does NOT compile the app target. Before claiming green, also run
  `cmake --build ~/builds/openauto-prodigy --target openauto-prodigy`.
- One task = one commit (subject given per task) ending with the trailer:
  `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`. No pushes, no
  git tags (milestone tags only on Matthew's explicit call — AGENTS.md
  § Versioning).
- Test names MUST be prefixed `test_media_*` — bare names collide with
  targets in the hands-off `libs/prodigy-oaa-protocol` submodule (stage-1
  lesson: `test_version` collided).
- New C++ sources are added to `openauto-core` in `src/CMakeLists.txt`
  beside the existing media_player entries (lines 83-87). New QML files
  follow the `MediaPlayerView.qml` registration pattern
  (`src/CMakeLists.txt:375` + `:493`).
- Zero new BUILD dependencies beyond libavformat (same FFmpeg source
  package family as the already-required libavcodec/libavutil). udisks2 is
  a Pi RUNTIME dependency only (install.sh), never a build/link dep.
- Prior-art credits (design §8 amendment): scanner/library sources carry
  the header comment `// Library heuristics informed by Yarock (GPL-3.0,
  github.com/sebaro/Yarock) and Strawberry (GPL-3.0); no code copied.` If a
  worker DOES copy code, that file instead carries per-file attribution
  with the upstream path and license.
- QML ships in-binary: UI changes reach the Pi only via cross-build +
  binary rsync (Task 8).
- Behavior invariants that MUST survive (stage-1 bench-hardened): nothing
  auto-plays at boot (restore = paused); restored-track failure stays
  stopped (no boot auto-skip); 3 consecutive unplayables → stop + toast;
  save-on-every-edge gated by restoring/shuttingDown; `previous()` >3s
  restarts track.
- Workers read root `AGENTS.md` + the nested `AGENTS.md` nearest their
  files (`src/AGENTS.md`, `qml/AGENTS.md`) before editing. Scope bounded to
  each task's files (wishlist-then-promote).

---

### Task 1: Extract `PlaybackPolicy` (pure logic, zero behavior change)

**Tier:** opus

**Files:**
- Create: `src/plugins/media_player/PlaybackPolicy.hpp`
- Create: `src/plugins/media_player/PlaybackPolicy.cpp`
- Modify: `src/plugins/media_player/MediaPlayerPlugin.hpp:103-126`
- Modify: `src/plugins/media_player/MediaPlayerPlugin.cpp` (lambdas at
  48-93; `shutdown()` 103-110; `startTrack` 143-146; `handleUnplayable`
  151-173; user actions 175-229; `restoreState` 279-303)
- Modify: `src/CMakeLists.txt:83-87` (add PlaybackPolicy.cpp)
- Create: `tests/test_media_playback_policy.cpp`
- Modify: `tests/CMakeLists.txt` (register after line 44 block)

**Interfaces:**
- Consumes: nothing new — pure refactor of the four fields
  `consecutiveErrors_`, `lastProgressMs_`, `restoring_`, `shuttingDown_`
  and their mutation sites.
- Produces (Tasks 5/6 rely on these exact names):
  `oap::plugins::PlaybackPolicy` with:
  `enum class TrackEndVerdict { Advance, Unplayable }`,
  `enum class UnplayableVerdict { SkipNext, StopAndNotify, StayStopped }`,
  methods `TrackEndVerdict onTrackFinished()`,
  `UnplayableVerdict onUnplayableEdge()`, `void onProgress(qint64 posMs)`,
  `void onTrackStarted()`, `void onUserAction()`, `void onNewQueue()`,
  `void onRestoreBegan()`, `void onShutdownBegan()`,
  `bool saveAllowed() const`, `bool restoring() const`,
  `int consecutiveErrors() const`.

Semantics contract (verbatim from the stage-1 code being replaced):

| Old site | New call |
|---|---|
| `trackFinished` lambda `lastProgressMs_ < 500` check (cpp:49) | `onTrackFinished()` returns `Unplayable` when watermark < 500 ms, else `Advance` |
| `errorOccurred` → `handleUnplayable` (cpp:58) | `onUnplayableEdge()` |
| `handleUnplayable` restoring branch (cpp:152-156) | `onUnplayableEdge()` returns `StayStopped` AND clears restoring (first edge only) |
| 3rd consecutive error stops + toast, counter resets (cpp:157-168) | `onUnplayableEdge()` returns `StopAndNotify` on the 3rd, resets counter |
| otherwise skip (cpp:170-172) | returns `SkipNext` |
| `progressChanged` watermark + `pos>500` error reset, restoring NOT cleared (cpp:80-93) | `onProgress(pos)` |
| `startTrack` resets watermark (cpp:144) | `onTrackStarted()` |
| `playFileFromFolder` clears restoring + errors (cpp:176,180) | `onUserAction()` + `onNewQueue()` |
| `playPause/next/previous` clear restoring (cpp:187,197,204) | `onUserAction()` |
| `saveState` gate `!restoring_ && !shuttingDown_` (cpp:71) | `saveAllowed()` |
| `shutdown()` sets shuttingDown_ (cpp:104) | `onShutdownBegan()` |
| `restoreState()` sets restoring_ (cpp:301) | `onRestoreBegan()` |

- [ ] **Step 1: Write the failing test**

Create `tests/test_media_playback_policy.cpp`:

```cpp
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
        // counter reset after the stop:
        QCOMPARE(p.onUnplayableEdge(), PlaybackPolicy::UnplayableVerdict::SkipNext);
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
```

Register in `tests/CMakeLists.txt` after the `test_playback_engine` block
(ends line 48):

```cmake
oap_add_test(test_media_playback_policy SOURCES test_media_playback_policy.cpp)
```

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cmake -S /mnt/e/claude/personal/openautopro/openauto-prodigy -B ~/builds/openauto-prodigy
cmake --build ~/builds/openauto-prodigy --target test_media_playback_policy -j"$(nproc)"
```
Expected: compile FAILURE — `PlaybackPolicy.hpp: No such file or directory`.

- [ ] **Step 3: Implement PlaybackPolicy**

Create `src/plugins/media_player/PlaybackPolicy.hpp`:

```cpp
#pragma once

#include <QtGlobal>

namespace oap {
namespace plugins {

/// Pure playback-policy state machine extracted from MediaPlayerPlugin's
/// signal lambdas (wishlist promotion 2026-07-09). Holds NO Qt-object
/// state; every stage-1 bench invariant is locked by
/// tests/test_media_playback_policy.cpp. Spec: design §10/§11.
class PlaybackPolicy {
public:
    enum class TrackEndVerdict { Advance, Unplayable };
    enum class UnplayableVerdict { SkipNext, StopAndNotify, StayStopped };

    /// Track reached its end. Below the audibility watermark the "end" was
    /// FFmpeg misdetecting garbage as audio (bench row 12) — unplayable.
    TrackEndVerdict onTrackFinished() const {
        return (lastProgressMs_ < kMinAudibleMs) ? TrackEndVerdict::Unplayable
                                                 : TrackEndVerdict::Advance;
    }

    /// An unplayable edge (decode error, or no-audio track end).
    UnplayableVerdict onUnplayableEdge();

    void onProgress(qint64 posMs);   ///< watermark + strike clearing; never clears restoring
    void onTrackStarted() { lastProgressMs_ = 0; }
    void onUserAction()   { restoring_ = false; }
    void onNewQueue()     { consecutiveErrors_ = 0; }
    void onRestoreBegan() { restoring_ = true; }
    void onShutdownBegan(){ shuttingDown_ = true; }

    bool saveAllowed() const { return !restoring_ && !shuttingDown_; }
    bool restoring() const { return restoring_; }
    int consecutiveErrors() const { return consecutiveErrors_; }

private:
    static constexpr int kMaxConsecutiveErrors = 3;
    static constexpr qint64 kMinAudibleMs = 500;

    int consecutiveErrors_ = 0;
    qint64 lastProgressMs_ = 0;
    bool restoring_ = false;
    bool shuttingDown_ = false;
};

} // namespace plugins
} // namespace oap
```

Create `src/plugins/media_player/PlaybackPolicy.cpp`:

```cpp
#include "PlaybackPolicy.hpp"

namespace oap {
namespace plugins {

PlaybackPolicy::UnplayableVerdict PlaybackPolicy::onUnplayableEdge() {
    if (restoring_) {
        // No auto-skip at boot — nothing may auto-play (design §10).
        restoring_ = false;
        return UnplayableVerdict::StayStopped;
    }
    ++consecutiveErrors_;
    if (consecutiveErrors_ >= kMaxConsecutiveErrors) {
        // A dead USB stick must not machine-gun skips (design §11).
        consecutiveErrors_ = 0;
        return UnplayableVerdict::StopAndNotify;
    }
    return UnplayableVerdict::SkipNext;
}

void PlaybackPolicy::onProgress(qint64 posMs) {
    lastProgressMs_ = qMax(lastProgressMs_, posMs);
    if (posMs > kMinAudibleMs)
        consecutiveErrors_ = 0;  // decode demonstrably working
    // restoring_ deliberately NOT cleared here — restore-seek echoes the
    // position before any decode (bench 2026-07-09 row 11 addendum).
}

} // namespace plugins
} // namespace oap
```

Add to `src/CMakeLists.txt` in the media_player block (after line 85's
`PlaybackEngine.cpp`):

```cmake
    plugins/media_player/PlaybackPolicy.cpp
```

- [ ] **Step 4: Run the policy test to verify it passes**

```bash
cmake --build ~/builds/openauto-prodigy --target test_media_playback_policy -j"$(nproc)"
cd ~/builds/openauto-prodigy && ctest -R test_media_playback_policy --output-on-failure
```
Expected: PASS (9/9 slots).

- [ ] **Step 5: Refactor MediaPlayerPlugin to delegate**

In `MediaPlayerPlugin.hpp`: add `#include "PlaybackPolicy.hpp"` (after the
IPlugin include), replace the four members at lines 117-125
(`consecutiveErrors_`, `lastProgressMs_` + comment, `restoring_`,
`shuttingDown_` + comment) with:

```cpp
    PlaybackPolicy policy_;  // extracted state machine; invariants locked
                             // by tests/test_media_playback_policy.cpp
```

In `MediaPlayerPlugin.cpp`, replace each mutation site per the semantics
table above. The exact replacements:

`trackFinished` lambda (was cpp:48-56):

```cpp
    connect(engine_, &PlaybackEngine::trackFinished, this, [this]() {
        if (policy_.onTrackFinished() == PlaybackPolicy::TrackEndVerdict::Unplayable) {
            handleUnplayable(QStringLiteral("track ended with no audio (misdetected format?)"));
            return;
        }
        if (queue_->advance(false))
            startTrack(queue_->currentTrack());
        // else: end of queue, repeat off — remain stopped on the last track.
    });
```

`playbackStateChanged` lambda save gate (was cpp:71):

```cpp
        if (policy_.saveAllowed()) saveState();
```

`progressChanged` lambda body (was cpp:82-90 — keep both emits):

```cpp
        policy_.onProgress(pos);
```

`shutdown()` (was cpp:104): `shuttingDown_ = true;` → `policy_.onShutdownBegan();`

`startTrack()` (was cpp:144): `lastProgressMs_ = 0;` → `policy_.onTrackStarted();`

`handleUnplayable()` (was cpp:151-173) becomes a verdict switch:

```cpp
void MediaPlayerPlugin::handleUnplayable(const QString& reason) {
    switch (policy_.onUnplayableEdge()) {
    case PlaybackPolicy::UnplayableVerdict::StayStopped:
        qCWarning(lcMediaPlayerPlugin) << "restored track failed to load; staying stopped:" << reason;
        return;  // no auto-skip at boot — nothing may auto-play (spec §10)
    case PlaybackPolicy::UnplayableVerdict::StopAndNotify:
        engine_->stop();
        if (hostContext_ && hostContext_->notificationService())
            hostContext_->notificationService()->post({
                {"kind", "toast"},
                {"message", QStringLiteral("Media Player: Playback stopped: too many unplayable files in a row")},
                {"sourcePluginId", kPluginId}
            });
        return;
    case PlaybackPolicy::UnplayableVerdict::SkipNext:
        qCWarning(lcMediaPlayerPlugin) << "skipping unplayable file:" << reason;
        if (queue_->advance(true))          // manual semantics: never re-loop one broken file
            startTrack(queue_->currentTrack());
        return;
    }
}
```

(The toast text drops the `%1` count — the policy resets its counter before
returning StopAndNotify, so the count is always 3; the message no longer
prints a number. `kMaxConsecutiveErrors` at cpp:22 is DELETED — the policy
owns it now.)

User actions: replace `restoring_ = false;` at cpp:176, 187, 197, 204 with
`policy_.onUserAction();` and `consecutiveErrors_ = 0;` at cpp:180 with
`policy_.onNewQueue();`.

`restoreState()` (was cpp:301): `restoring_ = true;` → `policy_.onRestoreBegan();`

- [ ] **Step 6: Full suite + app target (behavior-freeze check)**

```bash
cmake --build ~/builds/openauto-prodigy -j"$(nproc)"
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j"$(nproc)"
cd ~/builds/openauto-prodigy && ctest --output-on-failure
```
Expected: all green — the pre-existing suite passing unchanged IS the
zero-behavior-change evidence.

- [ ] **Step 7: Commit**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git add src/plugins/media_player/PlaybackPolicy.hpp src/plugins/media_player/PlaybackPolicy.cpp src/plugins/media_player/MediaPlayerPlugin.hpp src/plugins/media_player/MediaPlayerPlugin.cpp src/CMakeLists.txt tests/test_media_playback_policy.cpp tests/CMakeLists.txt
git commit -m "refactor: extract PlaybackPolicy state machine from MediaPlayerPlugin"
```

---

### Task 2: libavformat wiring + `MediaTagReader` + fixtures

**Tier:** opus

**Files:**
- Modify: `CMakeLists.txt:27` (pkg_check_modules)
- Modify: `docker/Dockerfile.cross-pi4:32-33`, `install.sh:814`,
  `docs/development.md:27` (add `libavformat-dev`)
- Create: `src/plugins/media_player/MediaTagReader.hpp`
- Create: `src/plugins/media_player/MediaTagReader.cpp`
- Modify: `src/CMakeLists.txt:83-87` (add MediaTagReader.cpp)
- Create: `tests/data/media/library/` fixtures (ffmpeg-generated, committed)
- Create: `tests/test_media_tag_reader.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `LIBAV_INCLUDE_DIRS`/`LIBAV_LIBRARIES` already applied to
  openauto-core at `src/CMakeLists.txt:108/126` — adding the module to the
  pkg_check line is sufficient.
- Produces (Tasks 3/4 rely on these exact names):

```cpp
struct MediaTrackInfo {
    QString title, artist, albumArtist, album, genre;
    int year = 0, trackNo = 0, discNo = 0;
    qint64 durationMs = 0;
    bool hasEmbeddedArt = false;
    bool valid = false;          // false = file unreadable as audio
};
namespace MediaTagReader {
    MediaTrackInfo read(const QString& path);
    QByteArray embeddedArt(const QString& path);  // JPEG/PNG bytes or empty
}
```

- [ ] **Step 1: Build wiring (four sites)**

`CMakeLists.txt:27`:
```cmake
pkg_check_modules(LIBAV REQUIRED libavformat libavcodec libavutil)
```
`docker/Dockerfile.cross-pi4` line 32 area — add beside the existing two:
```
    libavformat-dev:arm64 \
```
`install.sh:814`:
```
        libavformat-dev libavcodec-dev libavutil-dev
```
`docs/development.md:27` — add `libavformat-dev` to the same apt line.

- [ ] **Step 2: Generate committed fixtures**

`ffmpeg` CLI is required on the build host (`sudo apt install -y ffmpeg` if
missing — allowed). From the repo root:

```bash
F=tests/data/media/library && mkdir -p "$F/AlbumA" "$F/Comp"
# Two-track album with albumartist + embedded art (500ms tones)
ffmpeg -y -f lavfi -i "sine=frequency=440:duration=0.5" -f lavfi -i "color=c=red:s=64x64:d=1" -map 0:a -map 1:v -c:v mjpeg -disposition:v attached_pic -metadata title="Song One" -metadata artist="The Band" -metadata album_artist="The Band" -metadata album="First Album" -metadata track="1/2" -metadata disc="1" -metadata date="2001" -metadata genre="Rock" "$F/AlbumA/01-song-one.mp3"
ffmpeg -y -f lavfi -i "sine=frequency=550:duration=0.5" -metadata title="Song Two" -metadata artist="The Band" -metadata album_artist="The Band" -metadata album="First Album" -metadata track="2/2" "$F/AlbumA/02-song-two.mp3"
# Compilation: same album, differing artists, NO albumartist
ffmpeg -y -f lavfi -i "sine=frequency=660:duration=0.5" -metadata title="Comp One" -metadata artist="Artist X" -metadata album="Hits Comp" -metadata track="1" "$F/Comp/comp-one.flac"
ffmpeg -y -f lavfi -i "sine=frequency=770:duration=0.5" -metadata title="Comp Two" -metadata artist="Artist Y" -metadata album="Hits Comp" -metadata track="2" "$F/Comp/comp-two.flac"
# Tagless file (fallback exercise) + a non-audio decoy
ffmpeg -y -f lavfi -i "sine=frequency=880:duration=0.5" -map_metadata -1 -fflags +bitexact "$F/no-tags-here.ogg"
echo "not audio" > "$F/notes.txt"
```

Verify each file is <25 KB (`du -h "$F"`), then ensure tests copy them:
`tests/CMakeLists.txt:48`'s `file(COPY data/media DESTINATION ...)` already
copies the whole `data/media` tree — confirm `library/` lands in the build
dir after reconfigure.

- [ ] **Step 3: Write the failing test**

Create `tests/test_media_tag_reader.cpp`:

```cpp
#include <QtTest>
#include "plugins/media_player/MediaTagReader.hpp"

using namespace oap::plugins;

class TestMediaTagReader : public QObject {
    Q_OBJECT
    const QString root = QStringLiteral(TEST_DATA_DIR "/media/library");
private slots:
    void readsFullTags() {
        const auto t = MediaTagReader::read(root + "/AlbumA/01-song-one.mp3");
        QVERIFY(t.valid);
        QCOMPARE(t.title, QStringLiteral("Song One"));
        QCOMPARE(t.artist, QStringLiteral("The Band"));
        QCOMPARE(t.albumArtist, QStringLiteral("The Band"));
        QCOMPARE(t.album, QStringLiteral("First Album"));
        QCOMPARE(t.genre, QStringLiteral("Rock"));
        QCOMPARE(t.year, 2001);
        QCOMPARE(t.trackNo, 1);      // parsed from "1/2"
        QCOMPARE(t.discNo, 1);
        QVERIFY(t.durationMs > 300 && t.durationMs < 1500);
        QVERIFY(t.hasEmbeddedArt);
    }
    void parsesBareTrackNumber() {
        const auto t = MediaTagReader::read(root + "/Comp/comp-one.flac");
        QVERIFY(t.valid);
        QCOMPARE(t.trackNo, 1);
        QVERIFY(t.albumArtist.isEmpty());
    }
    void taglessFileFallsBackToFilenameStem() {
        const auto t = MediaTagReader::read(root + "/no-tags-here.ogg");
        QVERIFY(t.valid);
        QCOMPARE(t.title, QStringLiteral("no-tags-here"));  // §8 amendment #2
        QVERIFY(t.artist.isEmpty());
        QVERIFY(!t.hasEmbeddedArt);
    }
    void nonAudioIsInvalid() {
        QVERIFY(!MediaTagReader::read(root + "/notes.txt").valid);
        QVERIFY(!MediaTagReader::read(root + "/does-not-exist.mp3").valid);
    }
    void extractsEmbeddedArt() {
        const QByteArray art = MediaTagReader::embeddedArt(root + "/AlbumA/01-song-one.mp3");
        QVERIFY(!art.isEmpty());
        QVERIFY(!QImage::fromData(art).isNull());
        QVERIFY(MediaTagReader::embeddedArt(root + "/AlbumA/02-song-two.mp3").isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestMediaTagReader)
#include "test_media_tag_reader.moc"
```

Register in `tests/CMakeLists.txt` (mirrors `test_playback_engine`'s DEFS):

```cmake
oap_add_test(test_media_tag_reader
    SOURCES test_media_tag_reader.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
```

- [ ] **Step 4: Run test to verify it fails**

```bash
cmake -S /mnt/e/claude/personal/openautopro/openauto-prodigy -B ~/builds/openauto-prodigy
cmake --build ~/builds/openauto-prodigy --target test_media_tag_reader -j"$(nproc)"
```
Expected: compile FAILURE — `MediaTagReader.hpp: No such file or directory`.

- [ ] **Step 5: Implement MediaTagReader**

Create `src/plugins/media_player/MediaTagReader.hpp`:

```cpp
#pragma once

#include <QByteArray>
#include <QString>

namespace oap {
namespace plugins {

/// One track's tags as read from the container. Library heuristics
/// informed by Yarock (GPL-3.0, github.com/sebaro/Yarock) and Strawberry
/// (GPL-3.0); no code copied.
struct MediaTrackInfo {
    QString title, artist, albumArtist, album, genre;
    int year = 0, trackNo = 0, discNo = 0;
    qint64 durationMs = 0;
    bool hasEmbeddedArt = false;
    bool valid = false;
};

/// libavformat tag/art reader — header-only demux, no decode (except the
/// attached_pic stream, which ships pre-encoded). Zero new deps: avformat
/// joins the avcodec/avutil modules the video path already links.
namespace MediaTagReader {
MediaTrackInfo read(const QString& path);
QByteArray embeddedArt(const QString& path);
}

} // namespace plugins
} // namespace oap
```

Create `src/plugins/media_player/MediaTagReader.cpp`:

```cpp
#include "MediaTagReader.hpp"

#include <QFileInfo>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
}

namespace oap {
namespace plugins {
namespace MediaTagReader {

namespace {

QString tag(AVDictionary* meta, const char* key) {
    const AVDictionaryEntry* e = av_dict_get(meta, key, nullptr, AV_DICT_IGNORE_SUFFIX);
    return e ? QString::fromUtf8(e->value).trimmed() : QString();
}

// Accepts "3" and "3/12" (§8 amendment #2).
int leadingInt(const QString& s) {
    const int slash = s.indexOf(QLatin1Char('/'));
    bool ok = false;
    const int v = (slash < 0 ? s : s.left(slash)).toInt(&ok);
    return ok ? v : 0;
}

struct FormatCtx {  // RAII for avformat_open_input
    AVFormatContext* ctx = nullptr;
    ~FormatCtx() { if (ctx) avformat_close_input(&ctx); }
};

const AVStream* attachedPic(const AVFormatContext* ctx) {
    for (unsigned i = 0; i < ctx->nb_streams; ++i)
        if (ctx->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC)
            return ctx->streams[i];
    return nullptr;
}

} // namespace

MediaTrackInfo read(const QString& path) {
    MediaTrackInfo info;
    FormatCtx f;
    if (avformat_open_input(&f.ctx, path.toUtf8().constData(), nullptr, nullptr) != 0)
        return info;
    if (avformat_find_stream_info(f.ctx, nullptr) < 0)
        return info;
    // A "valid" track must contain at least one real audio stream.
    bool hasAudio = false;
    for (unsigned i = 0; i < f.ctx->nb_streams; ++i)
        if (f.ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            hasAudio = true;
    if (!hasAudio)
        return info;

    AVDictionary* meta = f.ctx->metadata;
    // Some containers (ogg/flac) hang tags off the stream, not the format.
    if (!av_dict_count(meta) && f.ctx->nb_streams > 0 && f.ctx->streams[0]->metadata)
        meta = f.ctx->streams[0]->metadata;

    info.title       = tag(meta, "title");
    info.artist      = tag(meta, "artist");
    info.albumArtist = tag(meta, "album_artist");
    info.album       = tag(meta, "album");
    info.genre       = tag(meta, "genre");
    info.year        = leadingInt(tag(meta, "date").left(4));
    info.trackNo     = leadingInt(tag(meta, "track"));
    info.discNo      = leadingInt(tag(meta, "disc"));
    if (f.ctx->duration > 0)
        info.durationMs = f.ctx->duration / (AV_TIME_BASE / 1000);
    if (info.title.isEmpty())
        info.title = QFileInfo(path).completeBaseName();  // §8 amendment #2
    info.hasEmbeddedArt = attachedPic(f.ctx) != nullptr;
    info.valid = true;
    return info;
}

QByteArray embeddedArt(const QString& path) {
    FormatCtx f;
    if (avformat_open_input(&f.ctx, path.toUtf8().constData(), nullptr, nullptr) != 0)
        return {};
    if (avformat_find_stream_info(f.ctx, nullptr) < 0)
        return {};
    const AVStream* pic = attachedPic(f.ctx);
    if (!pic || pic->attached_pic.size <= 0)
        return {};
    return QByteArray(reinterpret_cast<const char*>(pic->attached_pic.data),
                      pic->attached_pic.size);
}

} // namespace MediaTagReader
} // namespace plugins
} // namespace oap
```

Add to `src/CMakeLists.txt` media_player block:

```cmake
    plugins/media_player/MediaTagReader.cpp
```

- [ ] **Step 6: Run test to verify it passes, then full suite + app target**

```bash
cmake --build ~/builds/openauto-prodigy --target test_media_tag_reader -j"$(nproc)"
cd ~/builds/openauto-prodigy && ctest -R test_media_tag_reader --output-on-failure
cmake --build ~/builds/openauto-prodigy -j"$(nproc)"
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j"$(nproc)"
ctest --output-on-failure
```
Expected: all green. If a fixture assertion fails on tag casing/containers,
fix the READER (or regenerate the fixture with explicit metadata), never
loosen the assertion semantics.

- [ ] **Step 7: Commit**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git add CMakeLists.txt docker/Dockerfile.cross-pi4 install.sh docs/development.md src/plugins/media_player/MediaTagReader.hpp src/plugins/media_player/MediaTagReader.cpp src/CMakeLists.txt tests/test_media_tag_reader.cpp tests/CMakeLists.txt tests/data/media/library
git commit -m "feat: libavformat tag reader + committed library fixtures"
```

---

### Task 3: `MediaLibrary` index + the three library models

**Tier:** opus

**Files:**
- Create: `src/plugins/media_player/MediaLibrary.hpp`
- Create: `src/plugins/media_player/MediaLibrary.cpp`
- Modify: `src/CMakeLists.txt:83-87`
- Create: `tests/test_media_library.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `MediaTrackInfo` (Task 2, exact struct above).
- Produces (Tasks 4/5/6 rely on these exact names):

```cpp
struct MediaTrackRecord {          // one scanned file
    QString path, volumeKey;       // volumeKey: Task 4's per-volume cache key
    MediaTrackInfo info;
    QString artFile;               // Task 4 fills: cached album-art path or ""
};
class MediaLibrary : public QObject {
    // models (QAbstractListModel*, parented to the library):
    QObject* artistsModel(); QObject* albumsModel(); QObject* tracksModel();
    // data feed:
    void setTracks(QVector<MediaTrackRecord> all);   // full rebuild
    void removeVolume(const QString& volumeKey);     // yank support (Task 6)
    // drill-down (Q_INVOKABLE, QML):
    QVariantList albumsForArtist(const QString& artistKey);
    QStringList  trackPathsForAlbum(const QString& albumKey);
    QStringList  allTrackPathsSorted();
    int trackCount() const;
signals:
    void libraryChanged();
};
```

Model roles (all three models): `name` (display), `key` (stable string
key), `subtitle` (albums: artist; artists: "N albums"; tracks: artist),
`artUrl` (albums + tracks; `file://` to cached art or ""), `path` (tracks
only).

Heuristics (design §8 amendment — the reason this task exists):
- Album key = `albumArtistEffective + "\x1f" + album` where
  `albumArtistEffective = albumArtist ?: artist`, both lower-cased for
  keying (display keeps original case, first-seen wins).
- **Various Artists:** tracks sharing a non-empty album name whose
  albumArtist is empty and whose track artists DIFFER collapse into ONE
  album keyed `"\x1fVA\x1f" + album`, displayed under artist
  **"Various Artists"** with `compilation = true`.
- Missing artist → "Unknown Artist" bucket; missing album → "Unknown
  Album" (per-artist, NOT one global bucket).
- Track sort key inside an album: `discNo*1000 + trackNo`, ties by title
  (case-insensitive), then path. Artists and albums sort case-insensitive
  alphabetically; "Various Artists" sorts like any other artist.

- [ ] **Step 1: Write the failing test**

Create `tests/test_media_library.cpp` (pure in-memory — no fixtures):

```cpp
#include <QtTest>
#include <QAbstractListModel>
#include "plugins/media_player/MediaLibrary.hpp"

using namespace oap::plugins;

namespace {
MediaTrackRecord rec(const QString& path, const QString& title, const QString& artist,
                     const QString& albumArtist, const QString& album,
                     int track = 0, int disc = 0, const QString& vol = QStringLiteral("v1")) {
    MediaTrackRecord r;
    r.path = path; r.volumeKey = vol;
    r.info.title = title; r.info.artist = artist; r.info.albumArtist = albumArtist;
    r.info.album = album; r.info.trackNo = track; r.info.discNo = disc; r.info.valid = true;
    return r;
}
QStringList names(QObject* model) {
    auto* m = qobject_cast<QAbstractListModel*>(model);
    QStringList out;
    for (int i = 0; i < m->rowCount(); ++i)
        out << m->data(m->index(i, 0), m->roleNames().key("name")).toString();
    return out;
}
} // namespace

class TestMediaLibrary : public QObject {
    Q_OBJECT
private slots:
    void albumArtistGroupsAlbum() {
        MediaLibrary lib;
        lib.setTracks({ rec("/a/1.mp3", "One", "Band feat. Guest", "Band", "LP", 1),
                        rec("/a/2.mp3", "Two", "Band",             "Band", "LP", 2) });
        QCOMPARE(names(lib.albumsModel()), QStringList{QStringLiteral("LP")});
        QCOMPARE(names(lib.artistsModel()), QStringList{QStringLiteral("Band")});
    }
    void variousArtistsCollapse() {
        // Same album, differing artists, NO albumartist -> ONE VA album.
        MediaLibrary lib;
        lib.setTracks({ rec("/c/1.flac", "C1", "Artist X", "", "Hits Comp", 1),
                        rec("/c/2.flac", "C2", "Artist Y", "", "Hits Comp", 2) });
        QCOMPARE(names(lib.albumsModel()), QStringList{QStringLiteral("Hits Comp")});
        QCOMPARE(names(lib.artistsModel()), QStringList{QStringLiteral("Various Artists")});
    }
    void sameArtistNoAlbumArtistDoesNotCollapseToVA() {
        MediaLibrary lib;
        lib.setTracks({ rec("/b/1.mp3", "B1", "Solo", "", "Solo LP", 1),
                        rec("/b/2.mp3", "B2", "Solo", "", "Solo LP", 2) });
        QCOMPARE(names(lib.artistsModel()), QStringList{QStringLiteral("Solo")});
    }
    void sameAlbumNameDifferentDirsStaySeparate() {
        // Two artists each shipping "Greatest Hits" in their own folder,
        // neither tagged with albumartist — must NOT merge into VA.
        MediaLibrary lib;
        lib.setTracks({ rec("/x/greatest/1.mp3", "One", "Artist A", "", "Greatest Hits", 1),
                        rec("/y/greatest/1.mp3", "Uno", "Artist B", "", "Greatest Hits", 1) });
        QCOMPARE(qobject_cast<QAbstractListModel*>(lib.albumsModel())->rowCount(), 2);
        QCOMPARE(names(lib.artistsModel()),
                 (QStringList{QStringLiteral("Artist A"), QStringLiteral("Artist B")}));
    }
    void unknownBuckets() {
        MediaLibrary lib;
        lib.setTracks({ rec("/u/x.mp3", "x", "", "", "") });
        QCOMPARE(names(lib.artistsModel()), QStringList{QStringLiteral("Unknown Artist")});
        QCOMPARE(names(lib.albumsModel()), QStringList{QStringLiteral("Unknown Album")});
    }
    void discFoldedTrackOrder() {
        MediaLibrary lib;
        lib.setTracks({ rec("/d/d2t1.mp3", "D2T1", "B", "B", "Double", 1, 2),
                        rec("/d/d1t2.mp3", "D1T2", "B", "B", "Double", 2, 1),
                        rec("/d/d1t1.mp3", "D1T1", "B", "B", "Double", 1, 1) });
        const QString key = lib.albumsModelKeyAt(0);
        QCOMPARE(lib.trackPathsForAlbum(key),
                 (QStringList{QStringLiteral("/d/d1t1.mp3"), QStringLiteral("/d/d1t2.mp3"),
                              QStringLiteral("/d/d2t1.mp3")}));
    }
    void removeVolumeDropsItsTracks() {
        MediaLibrary lib;
        lib.setTracks({ rec("/usb/1.mp3", "U1", "A", "A", "UsbAlbum", 1, 0, "usbvol"),
                        rec("/home/1.mp3", "H1", "B", "B", "HomeAlbum", 1, 0, "homevol") });
        QCOMPARE(lib.trackCount(), 2);
        lib.removeVolume(QStringLiteral("usbvol"));
        QCOMPARE(lib.trackCount(), 1);
        QCOMPARE(names(lib.albumsModel()), QStringList{QStringLiteral("HomeAlbum")});
    }
    void drillDownAlbumsForArtist() {
        MediaLibrary lib;
        lib.setTracks({ rec("/a/1.mp3", "1", "Band", "Band", "LP1", 1),
                        rec("/a/2.mp3", "2", "Band", "Band", "LP2", 1),
                        rec("/z/1.mp3", "z", "Other", "Other", "ZLP", 1) });
        const QString bandKey = lib.artistsModelKeyAt(0);  // "Band" (alpha before Other)
        QCOMPARE(lib.albumsForArtist(bandKey).size(), 2);
    }
};

QTEST_APPLESS_MAIN(TestMediaLibrary)
#include "test_media_library.moc"
```

(`albumsModelKeyAt(int)` / `artistsModelKeyAt(int)` are Q_INVOKABLE test/QML
helpers returning the `key` role at a row — part of the produced interface.)

Register:

```cmake
oap_add_test(test_media_library SOURCES test_media_library.cpp)
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake -S /mnt/e/claude/personal/openautopro/openauto-prodigy -B ~/builds/openauto-prodigy
cmake --build ~/builds/openauto-prodigy --target test_media_library -j"$(nproc)"
```
Expected: compile FAILURE — `MediaLibrary.hpp: No such file or directory`.

- [ ] **Step 3: Implement MediaLibrary**

Create `src/plugins/media_player/MediaLibrary.hpp`:

```cpp
#pragma once

#include "MediaTagReader.hpp"
#include <QAbstractListModel>
#include <QHash>
#include <QObject>
#include <QVariantList>
#include <QVector>

namespace oap {
namespace plugins {

/// One scanned file (produced by MediaScanner, Task 4).
struct MediaTrackRecord {
    QString path;
    QString volumeKey;   // per-volume cache identity (UUID or path hash)
    MediaTrackInfo info;
    QString artFile;     // cached album-art absolute path, or empty
};

class LibraryListModel;  // internal generic name/key/subtitle/artUrl/path model

/// In-memory Artist -> Album -> Track index + the three QML list models.
/// Library heuristics informed by Yarock (GPL-3.0, github.com/sebaro/Yarock)
/// and Strawberry (GPL-3.0); no code copied. Design §8 amendment 2026-07-09.
class MediaLibrary : public QObject {
    Q_OBJECT
public:
    explicit MediaLibrary(QObject* parent = nullptr);
    ~MediaLibrary() override;

    QObject* artistsModel() const;
    QObject* albumsModel() const;
    QObject* tracksModel() const;

    void setTracks(QVector<MediaTrackRecord> all);
    void removeVolume(const QString& volumeKey);
    int trackCount() const { return tracks_.size(); }

    Q_INVOKABLE QVariantList albumsForArtist(const QString& artistKey) const;
    Q_INVOKABLE QStringList trackPathsForAlbum(const QString& albumKey) const;
    Q_INVOKABLE QStringList allTrackPathsSorted() const;
    Q_INVOKABLE QString artistsModelKeyAt(int row) const;
    Q_INVOKABLE QString albumsModelKeyAt(int row) const;

signals:
    void libraryChanged();

private:
    void rebuild();

    QVector<MediaTrackRecord> tracks_;
    LibraryListModel* artists_;
    LibraryListModel* albums_;
    LibraryListModel* trackList_;
    // albumKey -> ordered track indices into tracks_; artistKey -> albumKeys
    QHash<QString, QVector<int>> albumTracks_;
    QHash<QString, QStringList> artistAlbums_;
};

} // namespace plugins
} // namespace oap
```

Create `src/plugins/media_player/MediaLibrary.cpp` implementing:

```cpp
#include "MediaLibrary.hpp"

#include <QUrl>
#include <algorithm>

namespace oap {
namespace plugins {

namespace {
const QString kVaKeyPrefix = QStringLiteral("\x1fVA\x1f");
const QString kVaDisplay = QStringLiteral("Various Artists");
const QString kUnknownArtist = QStringLiteral("Unknown Artist");
const QString kUnknownAlbum = QStringLiteral("Unknown Album");

qint64 trackSortKey(const MediaTrackInfo& t) {   // §8: disc*1000 + track
    return qint64(t.discNo) * 1000 + t.trackNo;
}
} // namespace

/// Generic list model: rows of {name, key, subtitle, artUrl, path}.
class LibraryListModel : public QAbstractListModel {
public:
    enum Roles { NameRole = Qt::UserRole + 1, KeyRole, SubtitleRole, ArtUrlRole, PathRole };
    struct Row { QString name, key, subtitle, artUrl, path; };

    explicit LibraryListModel(QObject* parent) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex& = {}) const override { return rows_.size(); }
    QVariant data(const QModelIndex& idx, int role) const override {
        if (!idx.isValid() || idx.row() >= rows_.size()) return {};
        const Row& r = rows_.at(idx.row());
        switch (role) {
        case NameRole: return r.name;
        case KeyRole: return r.key;
        case SubtitleRole: return r.subtitle;
        case ArtUrlRole: return r.artUrl;
        case PathRole: return r.path;
        }
        return {};
    }
    QHash<int, QByteArray> roleNames() const override {
        return { {NameRole, "name"}, {KeyRole, "key"}, {SubtitleRole, "subtitle"},
                 {ArtUrlRole, "artUrl"}, {PathRole, "path"} };
    }
    void reset(QVector<Row> rows) {
        beginResetModel();
        rows_ = std::move(rows);
        endResetModel();
    }
    QString keyAt(int row) const { return rows_.value(row).key; }

private:
    QVector<Row> rows_;
};

MediaLibrary::MediaLibrary(QObject* parent)
    : QObject(parent),
      artists_(new LibraryListModel(this)),
      albums_(new LibraryListModel(this)),
      trackList_(new LibraryListModel(this)) {}

MediaLibrary::~MediaLibrary() = default;

QObject* MediaLibrary::artistsModel() const { return artists_; }
QObject* MediaLibrary::albumsModel() const { return albums_; }
QObject* MediaLibrary::tracksModel() const { return trackList_; }
QString MediaLibrary::artistsModelKeyAt(int row) const { return artists_->keyAt(row); }
QString MediaLibrary::albumsModelKeyAt(int row) const { return albums_->keyAt(row); }

void MediaLibrary::setTracks(QVector<MediaTrackRecord> all) {
    tracks_ = std::move(all);
    rebuild();
}

void MediaLibrary::removeVolume(const QString& volumeKey) {
    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                                 [&](const MediaTrackRecord& r) { return r.volumeKey == volumeKey; }),
                  tracks_.end());
    rebuild();
}

void MediaLibrary::rebuild() {
    albumTracks_.clear();
    artistAlbums_.clear();

    struct AlbumAgg {
        QString display, artistDisplay, artUrl;
        QSet<QString> trackArtistsLower;
        QString firstTrackArtist;
        QVector<int> trackIdx;
    };

    // Pass 1: aggregate. Tracks WITH an albumartist key by
    // (albumartist, album) — unambiguous. Tracks WITHOUT one key
    // provisionally by (album, parent directory): a compilation's
    // differing-artist tracks share a folder and must meet in ONE bucket
    // (§8 #1), while two different artists' same-named albums live in
    // different folders and must NOT merge. (Directory scoping is the
    // Strawberry trick; a multi-folder compilation without albumartist
    // degrades to one album per folder — acceptable, tag it properly.)
    QHash<QString, AlbumAgg> keyed;       // final (albumartist) buckets
    QHash<QString, AlbumAgg> provisional; // no-albumartist buckets
    for (int i = 0; i < tracks_.size(); ++i) {
        const MediaTrackInfo& t = tracks_[i].info;
        const QString albumDisp = !t.album.isEmpty() ? t.album : kUnknownAlbum;
        const bool hasAlbumArtist = !t.albumArtist.isEmpty();
        const QString bucketKey = hasAlbumArtist
            ? t.albumArtist.toLower() + QLatin1Char('\x1f') + albumDisp.toLower()
            : QLatin1Char('\x1f') + albumDisp.toLower() + QLatin1Char('\x1f')
                  + QFileInfo(tracks_[i].path).absolutePath().toLower();
        AlbumAgg& a = hasAlbumArtist ? keyed[bucketKey] : provisional[bucketKey];
        if (a.display.isEmpty()) {
            a.display = albumDisp;
            a.artistDisplay = hasAlbumArtist ? t.albumArtist : QString();
            a.firstTrackArtist = t.artist;
        }
        if (!t.artist.isEmpty()) a.trackArtistsLower.insert(t.artist.toLower());
        if (a.artUrl.isEmpty() && !tracks_[i].artFile.isEmpty())
            a.artUrl = QUrl::fromLocalFile(tracks_[i].artFile).toString();
        a.trackIdx.append(i);
    }

    // Pass 2: resolve provisional buckets — >1 distinct track artist =>
    // Various Artists; exactly one => a normal (artist, album) album;
    // zero (all tags empty) => Unknown Artist bucket (§8 #1/#2).
    QHash<QString, AlbumAgg> finalAlbums = keyed;
    for (auto it = provisional.begin(); it != provisional.end(); ++it) {
        AlbumAgg a = it.value();
        if (a.trackArtistsLower.size() > 1) {
            a.artistDisplay = kVaDisplay;
            finalAlbums.insert(kVaKeyPrefix + a.display.toLower()
                                   + QLatin1Char('\x1f') + it.key(), a);
        } else {
            a.artistDisplay = !a.firstTrackArtist.isEmpty() ? a.firstTrackArtist
                                                            : kUnknownArtist;
            finalAlbums.insert(a.artistDisplay.toLower() + QLatin1Char('\x1f')
                                   + a.display.toLower(), a);
        }
    }

    // Order tracks inside each album; build model rows.
    QVector<LibraryListModel::Row> albumRows, artistRows, trackRows;
    QHash<QString, QString> artistKeyToDisplay;

    for (auto it = finalAlbums.begin(); it != finalAlbums.end(); ++it) {
        AlbumAgg& a = it.value();
        std::sort(a.trackIdx.begin(), a.trackIdx.end(), [this](int l, int r) {
            const auto& lt = tracks_[l].info; const auto& rt = tracks_[r].info;
            if (trackSortKey(lt) != trackSortKey(rt)) return trackSortKey(lt) < trackSortKey(rt);
            const int c = lt.title.compare(rt.title, Qt::CaseInsensitive);
            if (c != 0) return c < 0;
            return tracks_[l].path < tracks_[r].path;
        });
        albumTracks_.insert(it.key(), a.trackIdx);
        const QString artistKey = a.artistDisplay.toLower();
        artistKeyToDisplay.insert(artistKey, a.artistDisplay);
        artistAlbums_[artistKey].append(it.key());
        albumRows.append({a.display, it.key(), a.artistDisplay, a.artUrl, {}});
    }

    std::sort(albumRows.begin(), albumRows.end(), [](const auto& l, const auto& r) {
        const int c = l.name.compare(r.name, Qt::CaseInsensitive);
        return c != 0 ? c < 0 : l.subtitle.compare(r.subtitle, Qt::CaseInsensitive) < 0;
    });

    for (auto it = artistAlbums_.begin(); it != artistAlbums_.end(); ++it)
        artistRows.append({artistKeyToDisplay.value(it.key()), it.key(),
                           QStringLiteral("%1 album(s)").arg(it.value().size()), {}, {}});
    std::sort(artistRows.begin(), artistRows.end(), [](const auto& l, const auto& r) {
        return l.name.compare(r.name, Qt::CaseInsensitive) < 0;
    });

    for (int i = 0; i < tracks_.size(); ++i)
        trackRows.append({tracks_[i].info.title, tracks_[i].path, tracks_[i].info.artist,
                          tracks_[i].artFile.isEmpty()
                              ? QString()
                              : QUrl::fromLocalFile(tracks_[i].artFile).toString(),
                          tracks_[i].path});
    std::sort(trackRows.begin(), trackRows.end(), [](const auto& l, const auto& r) {
        const int c = l.name.compare(r.name, Qt::CaseInsensitive);
        return c != 0 ? c < 0 : l.path < r.path;
    });

    artists_->reset(std::move(artistRows));
    albums_->reset(std::move(albumRows));
    trackList_->reset(std::move(trackRows));
    emit libraryChanged();
}

QVariantList MediaLibrary::albumsForArtist(const QString& artistKey) const {
    QVariantList out;
    for (const QString& albumKey : artistAlbums_.value(artistKey)) {
        const QVector<int> idx = albumTracks_.value(albumKey);
        if (idx.isEmpty()) continue;
        const MediaTrackRecord& first = tracks_[idx.first()];
        QVariantMap m;
        m.insert(QStringLiteral("key"), albumKey);
        m.insert(QStringLiteral("name"), !first.info.album.isEmpty() ? first.info.album : kUnknownAlbum);
        m.insert(QStringLiteral("artUrl"), first.artFile.isEmpty()
                     ? QString() : QUrl::fromLocalFile(first.artFile).toString());
        m.insert(QStringLiteral("trackCount"), idx.size());
        out.append(m);
    }
    return out;
}

QStringList MediaLibrary::trackPathsForAlbum(const QString& albumKey) const {
    QStringList out;
    for (int i : albumTracks_.value(albumKey)) out << tracks_[i].path;
    return out;
}

QStringList MediaLibrary::allTrackPathsSorted() const {
    QStringList out;
    for (int i = 0; i < trackList_->rowCount(); ++i)
        out << trackList_->keyAt(i);   // track rows key == path
    return out;
}

} // namespace plugins
} // namespace oap
```

Add to `src/CMakeLists.txt` media_player block:

```cmake
    plugins/media_player/MediaLibrary.cpp
```

- [ ] **Step 4: Run test to verify it passes, then full suite + app target**

```bash
cmake --build ~/builds/openauto-prodigy --target test_media_library -j"$(nproc)"
cd ~/builds/openauto-prodigy && ctest -R test_media_library --output-on-failure
cmake --build ~/builds/openauto-prodigy -j"$(nproc)"
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j"$(nproc)"
ctest --output-on-failure
```
Expected: all green.

- [ ] **Step 5: Commit**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git add src/plugins/media_player/MediaLibrary.hpp src/plugins/media_player/MediaLibrary.cpp src/CMakeLists.txt tests/test_media_library.cpp tests/CMakeLists.txt
git commit -m "feat: MediaLibrary index + artists/albums/tracks models with VA heuristics"
```

---

### Task 4: `MediaScanner` worker + incremental cache + art cache

**Tier:** opus

**Files:**
- Create: `src/plugins/media_player/MediaScanner.hpp`
- Create: `src/plugins/media_player/MediaScanner.cpp`
- Modify: `src/CMakeLists.txt:83-87`
- Create: `tests/test_media_scanner.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `MediaTagReader::read/embeddedArt` (Task 2),
  `MediaTrackRecord` (Task 3), `FolderModel::audioExtensions()`
  (`FolderModel.cpp:11` — the single source of truth for what counts as
  audio).
- Produces (Task 5 relies on these exact names):

```cpp
class MediaScanner : public QObject {
    // cacheDir default: ~/.openauto/cache (overridable for tests)
    explicit MediaScanner(QObject* parent = nullptr);
    void setCacheDir(const QString& dir);
    // Fire-and-forget: scans roots on a worker thread. label/path pairs as
    // FolderModel::setRoots takes; volumeKey derived per root (below).
    void scan(const QVector<QPair<QString,QString>>& roots);
    bool busy() const;
signals:
    void progress(int filesScanned, int filesTotal);
    void finished(QVector<oap::plugins::MediaTrackRecord> records);
};
```

Contract details:
- **volumeKey** per root: `QStorageInfo(rootPath)` — use
  `device()+subvolume()` if non-empty else the root path, SHA1-hex first 16
  chars. Same function feeds the cache filename.
- **Cache file** per volume: `<cacheDir>/medialib/<volumeKey>.bin`,
  QDataStream (`QDataStream::Qt_6_5`), layout: magic `"OAPL"`, version
  `quint16 = 1`, then count + repeated
  `{path, mtimeMsSinceEpoch(qint64), size(qint64), MediaTrackInfo fields
  in declaration order, artFile}`. Unknown magic/version → ignore cache,
  full rescan, overwrite on completion.
- **Incremental rule:** a directory-walk entry whose `(path, mtime, size)`
  matches a cache entry reuses the cached record — `MediaTagReader::read`
  is NOT called for it. Everything else is (re)read. Cache entries whose
  file vanished are dropped. Cache is rewritten after every completed scan.
- **Art cache:** first track of each album (key per Task 3 heuristic) with
  `hasEmbeddedArt` gets `embeddedArt()` extracted to
  `<cacheDir>/art/<sha1hex16 of albumKey>.jpg` (skip write if the file
  already exists); every record of that album gets `artFile` set to it. If
  no track has embedded art, look for `cover.jpg|cover.png|folder.jpg|
  folder.png|front.jpg|front.png` (case-insensitive) in the FIRST track's
  directory (§8 amendment #3) and use that path directly (no copy).
- **Walk hygiene (§8 amendment #4):** `QDirIterator` with
  `QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs | QDir::NoSymLinks` is
  NOT enough — symlinked dirs are wanted, loops are not: track visited
  `QFileInfo::canonicalFilePath()` of each directory in a QSet, skip
  already-seen; skip hidden entries (`QDir::Hidden` excluded by default —
  do not add it).
- **Threading:** the scan body runs via
  `QThread::create`; `finished`/`progress` are emitted with
  `Qt::QueuedConnection` semantics (signal emission from the worker thread
  to the QObject living on the caller's thread is queued automatically).
  `scan()` while busy is a no-op returning immediately (log a warning).
  Destructor requests interruption and `wait()`s — the walk loop checks
  `QThread::currentThread()->isInterruptionRequested()` every file.

- [ ] **Step 1: Write the failing test**

Create `tests/test_media_scanner.cpp`:

```cpp
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "plugins/media_player/MediaScanner.hpp"

using namespace oap::plugins;

class TestMediaScanner : public QObject {
    Q_OBJECT
    QString fixtures() const { return QStringLiteral(TEST_DATA_DIR "/media/library"); }

    QVector<MediaTrackRecord> runScan(MediaScanner& s, const QString& root) {
        QSignalSpy done(&s, &MediaScanner::finished);
        s.scan({{QStringLiteral("root"), root}});
        [&]{ QVERIFY(done.wait(15000)); }();
        return done.takeFirst().at(0).value<QVector<MediaTrackRecord>>();
    }

private slots:
    void coldScanFindsAllAudio() {
        QTemporaryDir cache;
        MediaScanner s; s.setCacheDir(cache.path());
        const auto recs = runScan(s, fixtures());
        // 5 audio fixtures; notes.txt excluded by extension filter.
        QCOMPARE(recs.size(), 5);
        for (const auto& r : recs) QVERIFY(r.info.valid);
    }
    void embeddedArtExtractedOncePerAlbum() {
        QTemporaryDir cache;
        MediaScanner s; s.setCacheDir(cache.path());
        const auto recs = runScan(s, fixtures());
        QString albumAArt;
        for (const auto& r : recs)
            if (r.info.album == QLatin1String("First Album")) {
                QVERIFY2(!r.artFile.isEmpty(), "album with embedded art must get artFile");
                if (albumAArt.isEmpty()) albumAArt = r.artFile;
                QCOMPARE(r.artFile, albumAArt);   // shared per-album, not per-track
            }
        QVERIFY(QFileInfo::exists(albumAArt));
    }
    void incrementalScanSkipsUnchanged() {
        // Copy fixtures to a writable tree so mtimes are ours to control.
        QTemporaryDir tree, cache;
        for (const QFileInfo& fi : QDir(fixtures() + "/AlbumA").entryInfoList(QDir::Files))
            QFile::copy(fi.absoluteFilePath(), tree.path() + "/" + fi.fileName());
        MediaScanner s; s.setCacheDir(cache.path());
        auto first = runScan(s, tree.path());
        QCOMPARE(first.size(), 2);
        // Second scan: cache hit for both -> tag reads == 0.
        auto second = runScan(s, tree.path());
        QCOMPARE(second.size(), 2);
        QCOMPARE(s.lastScanTagReads(), 0);        // probe counter (test hook)
        // Touch one file -> exactly one re-read.
        QFile f(tree.path() + "/01-song-one.mp3");
        f.open(QIODevice::Append); f.write("x"); f.close();
        auto third = runScan(s, tree.path());
        QCOMPARE(third.size(), 2);
        QCOMPARE(s.lastScanTagReads(), 1);
    }
    void vanishedFilesDropFromCache() {
        QTemporaryDir tree, cache;
        for (const QFileInfo& fi : QDir(fixtures() + "/Comp").entryInfoList(QDir::Files))
            QFile::copy(fi.absoluteFilePath(), tree.path() + "/" + fi.fileName());
        MediaScanner s; s.setCacheDir(cache.path());
        QCOMPARE(runScan(s, tree.path()).size(), 2);
        QFile::remove(tree.path() + "/comp-two.flac");
        QCOMPARE(runScan(s, tree.path()).size(), 1);
    }
    void hiddenFilesSkipped() {
        QTemporaryDir tree, cache;
        QFile::copy(fixtures() + "/no-tags-here.ogg", tree.path() + "/.hidden.ogg");
        QFile::copy(fixtures() + "/no-tags-here.ogg", tree.path() + "/visible.ogg");
        MediaScanner s; s.setCacheDir(cache.path());
        QCOMPARE(runScan(s, tree.path()).size(), 1);
    }
};

QTEST_MAIN(TestMediaScanner)
#include "test_media_scanner.moc"
```

(`lastScanTagReads()` is a test-hook counter on MediaScanner — part of the
produced interface, mirrors `PlayQueue::setShuffleSeed` precedent.
`QTEST_MAIN`, not APPLESS: QSignalSpy::wait needs an event loop.
`qRegisterMetaType<QVector<MediaTrackRecord>>()` in the scanner ctor makes
the queued signal + QSignalSpy work.)

Register:

```cmake
oap_add_test(test_media_scanner
    SOURCES test_media_scanner.cpp
    DEFS TEST_DATA_DIR="${CMAKE_CURRENT_BINARY_DIR}/data"
)
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake -S /mnt/e/claude/personal/openautopro/openauto-prodigy -B ~/builds/openauto-prodigy
cmake --build ~/builds/openauto-prodigy --target test_media_scanner -j"$(nproc)"
```
Expected: compile FAILURE — `MediaScanner.hpp: No such file or directory`.

- [ ] **Step 3: Implement MediaScanner**

Create `src/plugins/media_player/MediaScanner.hpp`:

```cpp
#pragma once

#include "MediaLibrary.hpp"
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QVector>

class QThread;

namespace oap {
namespace plugins {

/// Worker-thread library scanner with per-volume incremental cache.
/// Cache: <cacheDir>/medialib/<volumeKey>.bin (QDataStream, versioned).
/// Art:   <cacheDir>/art/<albumHash>.jpg (§8 amendment #3 priority).
/// Library heuristics informed by Yarock (GPL-3.0, github.com/sebaro/Yarock)
/// and Strawberry (GPL-3.0); no code copied.
class MediaScanner : public QObject {
    Q_OBJECT
public:
    explicit MediaScanner(QObject* parent = nullptr);
    ~MediaScanner() override;

    void setCacheDir(const QString& dir);
    void scan(const QVector<QPair<QString, QString>>& roots);
    bool busy() const { return thread_ != nullptr; }
    int lastScanTagReads() const { return lastScanTagReads_; }  // test hook

    static QString volumeKeyFor(const QString& rootPath);

signals:
    void progress(int filesScanned, int filesTotal);
    void finished(QVector<oap::plugins::MediaTrackRecord> records);

private:
    QVector<MediaTrackRecord> runScan(QVector<QPair<QString, QString>> roots);

    QString cacheDir_;
    QThread* thread_ = nullptr;
    int lastScanTagReads_ = 0;
};

} // namespace plugins
} // namespace oap
```

Create `src/plugins/media_player/MediaScanner.cpp` implementing the
contract above. Complete implementation:

```cpp
#include "MediaScanner.hpp"

#include "FolderModel.hpp"
#include "MediaTagReader.hpp"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QThread>

Q_LOGGING_CATEGORY(lcMediaScanner, "oap.media.scanner")

namespace oap {
namespace plugins {

namespace {
constexpr quint32 kCacheMagic = 0x4F41504C;  // "OAPL"
constexpr quint16 kCacheVersion = 1;

struct CacheEntry {
    qint64 mtimeMs = 0, size = 0;
    MediaTrackInfo info;
    QString artFile;
};

QString sha1Hex16(const QString& s) {
    return QString::fromLatin1(
        QCryptographicHash::hash(s.toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
}

QDataStream& operator<<(QDataStream& out, const MediaTrackInfo& t) {
    return out << t.title << t.artist << t.albumArtist << t.album << t.genre
               << qint32(t.year) << qint32(t.trackNo) << qint32(t.discNo)
               << t.durationMs << t.hasEmbeddedArt << t.valid;
}
QDataStream& operator>>(QDataStream& in, MediaTrackInfo& t) {
    qint32 year = 0, trackNo = 0, discNo = 0;
    in >> t.title >> t.artist >> t.albumArtist >> t.album >> t.genre
       >> year >> trackNo >> discNo >> t.durationMs >> t.hasEmbeddedArt >> t.valid;
    t.year = year; t.trackNo = trackNo; t.discNo = discNo;
    return in;
}

QString albumKeyOf(const MediaTrackInfo& t) {   // must mirror MediaLibrary keying
    const QString artist = !t.albumArtist.isEmpty() ? t.albumArtist
                          : !t.artist.isEmpty()     ? t.artist
                                                    : QStringLiteral("Unknown Artist");
    const QString album = !t.album.isEmpty() ? t.album : QStringLiteral("Unknown Album");
    return artist.toLower() + QLatin1Char('\x1f') + album.toLower();
}

QString sidecarArt(const QString& dir) {        // §8 #3: cover|folder|front.{jpg,png}
    static const QStringList names = {
        QStringLiteral("cover.jpg"), QStringLiteral("cover.png"),
        QStringLiteral("folder.jpg"), QStringLiteral("folder.png"),
        QStringLiteral("front.jpg"), QStringLiteral("front.png")};
    const QDir d(dir);
    for (const QFileInfo& fi : d.entryInfoList(QDir::Files)) {
        if (names.contains(fi.fileName().toLower()))
            return fi.absoluteFilePath();
    }
    return {};
}
} // namespace

MediaScanner::MediaScanner(QObject* parent) : QObject(parent) {
    qRegisterMetaType<QVector<oap::plugins::MediaTrackRecord>>();
    cacheDir_ = QDir::homePath() + QStringLiteral("/.openauto/cache");
}

MediaScanner::~MediaScanner() {
    if (thread_) {
        thread_->requestInterruption();
        thread_->wait();
    }
}

void MediaScanner::setCacheDir(const QString& dir) { cacheDir_ = dir; }

QString MediaScanner::volumeKeyFor(const QString& rootPath) {
    const QStorageInfo si(rootPath);
    const QByteArray dev = si.device() + si.subvolume();
    return sha1Hex16(dev.isEmpty() ? rootPath : QString::fromUtf8(dev));
}

void MediaScanner::scan(const QVector<QPair<QString, QString>>& roots) {
    if (thread_) {
        qCWarning(lcMediaScanner) << "scan requested while busy — ignored";
        return;
    }
    thread_ = QThread::create([this, roots]() {
        QVector<MediaTrackRecord> records = runScan(roots);
        emit finished(records);   // queued to the scanner's owner thread
    });
    connect(thread_, &QThread::finished, this, [this]() {
        thread_->deleteLater();
        thread_ = nullptr;
    });
    thread_->start();
}

QVector<MediaTrackRecord> MediaScanner::runScan(QVector<QPair<QString, QString>> roots) {
    int tagReads = 0;
    QVector<MediaTrackRecord> all;
    QDir().mkpath(cacheDir_ + QStringLiteral("/medialib"));
    QDir().mkpath(cacheDir_ + QStringLiteral("/art"));

    for (const auto& root : roots) {
        const QString rootPath = root.second;
        const QString volKey = volumeKeyFor(rootPath);
        const QString cacheFile =
            cacheDir_ + QStringLiteral("/medialib/") + volKey + QStringLiteral(".bin");

        // Load cache (tolerate absence/corruption -> full scan).
        QHash<QString, CacheEntry> cache;
        {
            QFile f(cacheFile);
            if (f.open(QIODevice::ReadOnly)) {
                QDataStream in(&f);
                in.setVersion(QDataStream::Qt_6_5);
                quint32 magic = 0; quint16 ver = 0; qint32 count = 0;
                in >> magic >> ver >> count;
                if (magic == kCacheMagic && ver == kCacheVersion) {
                    for (qint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i) {
                        QString path; CacheEntry e;
                        in >> path >> e.mtimeMs >> e.size >> e.info >> e.artFile;
                        cache.insert(path, e);
                    }
                }
            }
        }

        // Walk: files by extension; visited-dir set kills symlink loops (§8 #4).
        const QStringList exts = FolderModel::audioExtensions();
        QStringList files;
        QSet<QString> visitedDirs;
        QVector<QString> pending{rootPath};
        while (!pending.isEmpty()) {
            if (QThread::currentThread()->isInterruptionRequested()) return {};
            const QString dirPath = pending.takeLast();
            const QString canonical = QFileInfo(dirPath).canonicalFilePath();
            if (canonical.isEmpty() || visitedDirs.contains(canonical)) continue;
            visitedDirs.insert(canonical);
            const QDir dir(dirPath);
            for (const QFileInfo& fi :
                 dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                   QDir::Name | QDir::IgnoreCase)) {
                if (fi.isDir()) pending.append(fi.absoluteFilePath());
                else if (exts.contains(fi.suffix().toLower()))
                    files.append(fi.absoluteFilePath());
            }
        }

        // Read (cache-aware).
        QVector<MediaTrackRecord> vol;
        int scanned = 0;
        for (const QString& path : files) {
            if (QThread::currentThread()->isInterruptionRequested()) return {};
            const QFileInfo fi(path);
            const qint64 mtime = fi.lastModified().toMSecsSinceEpoch();
            const qint64 size = fi.size();
            MediaTrackRecord rec;
            rec.path = path;
            rec.volumeKey = volKey;
            const auto it = cache.constFind(path);
            if (it != cache.constEnd() && it->mtimeMs == mtime && it->size == size) {
                rec.info = it->info;
                rec.artFile = it->artFile;
            } else {
                rec.info = MediaTagReader::read(path);
                ++tagReads;
            }
            if (rec.info.valid) vol.append(rec);
            emit progress(++scanned, files.size());
        }

        // Art pass (§8 #3): once per album.
        QHash<QString, QString> albumArt;   // albumKey -> artFile
        for (MediaTrackRecord& rec : vol) {
            const QString key = albumKeyOf(rec.info);
            if (!albumArt.contains(key)) {
                QString art;
                if (rec.info.hasEmbeddedArt) {
                    const QString artPath =
                        cacheDir_ + QStringLiteral("/art/") + sha1Hex16(key) + QStringLiteral(".jpg");
                    if (QFileInfo::exists(artPath)) {
                        art = artPath;
                    } else {
                        const QByteArray bytes = MediaTagReader::embeddedArt(rec.path);
                        if (!bytes.isEmpty()) {
                            QFile out(artPath);
                            if (out.open(QIODevice::WriteOnly)) { out.write(bytes); art = artPath; }
                        }
                    }
                }
                if (art.isEmpty())
                    art = sidecarArt(QFileInfo(rec.path).absolutePath());
                albumArt.insert(key, art);
            }
            rec.artFile = albumArt.value(key);
        }

        // Rewrite cache for this volume.
        {
            QFile f(cacheFile);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QDataStream out(&f);
                out.setVersion(QDataStream::Qt_6_5);
                out << kCacheMagic << kCacheVersion << qint32(vol.size());
                for (const MediaTrackRecord& rec : vol) {
                    const QFileInfo fi(rec.path);
                    out << rec.path << fi.lastModified().toMSecsSinceEpoch()
                        << fi.size() << rec.info << rec.artFile;
                }
            }
        }

        all += vol;
    }

    lastScanTagReads_ = tagReads;
    return all;
}

} // namespace plugins
} // namespace oap
```

Add to `src/CMakeLists.txt` media_player block:

```cmake
    plugins/media_player/MediaScanner.cpp
```

Also declare the metatype at the bottom of `MediaLibrary.hpp` (outside
namespaces): `Q_DECLARE_METATYPE(QVector<oap::plugins::MediaTrackRecord>)`.

- [ ] **Step 4: Run test to verify it passes, then full suite + app target**

```bash
cmake --build ~/builds/openauto-prodigy --target test_media_scanner -j"$(nproc)"
cd ~/builds/openauto-prodigy && ctest -R test_media_scanner --output-on-failure
cmake --build ~/builds/openauto-prodigy -j"$(nproc)"
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j"$(nproc)"
ctest --output-on-failure
```
Expected: all green. `lastScanTagReads_` write happens on the worker thread
before `finished` is emitted and is only read after `finished` is received
— if the reviewer flags the cross-thread write, guard it with the same
pattern the test uses (read only after `finished`) and document it.

- [ ] **Step 5: Commit**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git add src/plugins/media_player/MediaScanner.hpp src/plugins/media_player/MediaScanner.cpp src/plugins/media_player/MediaLibrary.hpp src/CMakeLists.txt tests/test_media_scanner.cpp tests/CMakeLists.txt
git commit -m "feat: worker-thread MediaScanner with incremental per-volume cache + art cache"
```

---

### Task 5: Plugin wiring + QML library tabs

**Tier:** opus

**Files:**
- Modify: `src/plugins/media_player/MediaPlayerPlugin.hpp` (properties +
  members + invokables)
- Modify: `src/plugins/media_player/MediaPlayerPlugin.cpp` (`initialize`,
  `refreshSources`, new invokables)
- Modify: `qml/applications/media_player/MediaPlayerView.qml` (tab row +
  tab hosting; current header lines 21-73, list at 76-139)
- Create: `qml/applications/media_player/LibraryAlbumsTab.qml`
- Create: `qml/applications/media_player/LibraryArtistsTab.qml`
- Create: `qml/applications/media_player/LibraryTracksTab.qml`
- Modify: `src/CMakeLists.txt` (register the three QML files beside
  `MediaPlayerView.qml` — pattern at lines 375 and 493)

**Interfaces:**
- Consumes: `MediaLibrary` (Task 3), `MediaScanner` (Task 4),
  `PlaybackPolicy` (Task 1), existing `PlayQueue::setTracks(QStringList,int)`.
- Produces (QML + Task 6 rely on):
  - Plugin Q_PROPERTYs: `QObject* artistsModel`, `QObject* albumsModel`,
    `QObject* tracksModel` (CONSTANT), `bool libraryScanning` (NOTIFY
    `libraryScanningChanged`), `int libraryTrackCount` (NOTIFY
    `libraryChanged`).
  - Plugin Q_INVOKABLEs:
    `playAlbum(QString albumKey, int startIndex)`,
    `playAllTracks(int startIndex)`,
    `albumsForArtist(QString artistKey)` (forwards to MediaLibrary),
    `trackPathsForAlbum(QString albumKey)` (forwards),
    `rescanLibrary()`.
  - Members: `MediaLibrary* library_`, `MediaScanner* scanner_` (both
    created in `initialize`), plus `currentRoots_`
    (`QVector<QPair<QString,QString>>`) captured by `refreshSources()` so
    Task 6 can rescan and purge per-volume.

Wiring contract:
- `initialize()`: create `library_`/`scanner_` after `folderModel_`;
  `connect(scanner_, &MediaScanner::finished, this, ...)` → 
  `library_->setTracks(records)`, emit `libraryScanningChanged` +
  `libraryChanged`.
- `refreshSources()` stores the roots it computed into `currentRoots_`
  (single change: the local `roots` becomes the member assignment before
  `folderModel_->setRoots(currentRoots_)`) and then, when
  `!scanner_->busy()`, kicks `scanner_->scan(currentRoots_)`.
- `playAlbum(key, idx)`: `policy_.onUserAction(); policy_.onNewQueue();`
  then `queue_->setTracks(library_->trackPathsForAlbum(key), idx)`,
  `setHasTrack(true)`, `startTrack(queue_->currentTrack())` — the same
  sequence `playFileFromFolder` uses (cpp:175-184).
- `playAllTracks(idx)`: same with `library_->allTrackPathsSorted()`.

QML contract (all three tabs):
- `MediaPlayerView.qml` gains a 4-button tab row (Artists / Albums /
  Tracks / Folders) in the header area; the existing folder browser
  becomes the Folders tab content unchanged. Selected tab held in
  `property int currentTab: 3` (Folders default — stage-1 behavior
  preserved until the user switches). Use the same Tile/ThemeService
  idioms as the existing header buttons (match the file's own back/refresh
  buttons); tab content switches via a `StackLayout`.
- `LibraryAlbumsTab.qml`: `GridView` over `MediaPlayerPlugin.albumsModel`
  (cells ~180px, art via `model.artUrl` with a `library_music` glyph
  fallback, name + subtitle under the art). Tap album → inline track list
  (a `ListView` overlay panel with back, same pattern as Folders
  breadcrumb) via `MediaPlayerPlugin.trackPathsForAlbum(model.key)`; tap
  track N → `MediaPlayerPlugin.playAlbum(key, N)`.
- `LibraryArtistsTab.qml`: `ListView` over `artistsModel` (name +
  subtitle); tap artist → albums panel fed by
  `MediaPlayerPlugin.albumsForArtist(model.key)` (a `QVariantList` of
  {key,name,artUrl,trackCount} maps) → tap album → tracks as above.
- `LibraryTracksTab.qml`: `ListView` over `tracksModel`; tap row i →
  `MediaPlayerPlugin.playAllTracks(i)`.
- Scanning state: when `MediaPlayerPlugin.libraryScanning`, tabs show a
  small "Scanning…" row at top; when `libraryTrackCount === 0` and not
  scanning, an empty-state label ("No music found — add files to ~/Music
  or plug in a USB drive").

The QML worker writes complete files following the view's existing idioms
(this plan intentionally does not reproduce ~200 lines of QML; the binding
contract above plus the surrounding file define the deliverable — any
ambiguity resolves toward matching the existing Folders list styling).

- [ ] **Step 1: Extend the plugin (header + cpp per the wiring contract)** —
  all property/invokable/member names exactly as the Interfaces block.
- [ ] **Step 2: Build + full suite (no test yet — wiring only)**

```bash
cmake --build ~/builds/openauto-prodigy -j"$(nproc)"
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j"$(nproc)"
cd ~/builds/openauto-prodigy && ctest --output-on-failure
```
Expected: green (Tasks 1-4 suites cover the moving parts; the plugin layer
is thin forwarding).

- [ ] **Step 3: Write the three tab QML files + tab row; register in CMake**

Registration in `src/CMakeLists.txt`: add the three new paths in BOTH the
`set_source_files_properties` block (pattern line 375) and the module
`QML_FILES` list (pattern line 493), URIs matching MediaPlayerView.

- [ ] **Step 4: Offscreen smoke**

```bash
QT_QPA_PLATFORM=offscreen timeout 20 ~/builds/openauto-prodigy/src/openauto-prodigy --headless-check 2>&1 | grep -iE "qml|error" | head -20
```
(If `--headless-check` is not a supported flag, run with
`timeout 10 ... ; true` and grep the log for QML errors — a QML syntax
error aborts module load loudly.) Expected: no QML errors mentioning
MediaPlayerView/Library*Tab.

- [ ] **Step 5: Full suite + app target again, then commit**

```bash
cmake --build ~/builds/openauto-prodigy -j"$(nproc)"
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j"$(nproc)"
cd ~/builds/openauto-prodigy && ctest --output-on-failure
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git add src/plugins/media_player/MediaPlayerPlugin.hpp src/plugins/media_player/MediaPlayerPlugin.cpp qml/applications/media_player src/CMakeLists.txt
git commit -m "feat: library tabs (artists/albums/tracks) wired to MediaLibrary scanner"
```

---

### Task 6: `UsbMediaWatcher` + safe eject + yank recovery

**Tier:** opus

**Files:**
- Create: `src/plugins/media_player/UsbMediaWatcher.hpp`
- Create: `src/plugins/media_player/UsbMediaWatcher.cpp`
- Modify: `src/plugins/media_player/PlayQueue.hpp` / `.cpp` (add
  `removeTracksUnder`)
- Modify: `src/plugins/media_player/MediaPlayerPlugin.hpp` / `.cpp`
  (watcher wiring + eject invokable)
- Modify: `qml/applications/media_player/MediaPlayerView.qml` (eject button
  on removable roots in the Folders top level)
- Modify: `src/CMakeLists.txt:83-87`
- Create: `tests/test_media_usb_policy.cpp` (queue purge + prefix logic)
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: BlueZ watcher pattern (`BtAudioPlugin.cpp:42-138` —
  `QDBusConnection::systemBus()`, `bus.connect(service, "/", 
  "org.freedesktop.DBus.ObjectManager", "InterfacesAdded", this,
  SLOT(...))`, `QDBusServiceWatcher`, `GetManagedObjects` initial scan at
  `BtAudioPlugin.cpp:143-222`); `MediaLibrary::removeVolume` (Task 3);
  `MediaScanner::volumeKeyFor` (Task 4); `PlaybackPolicy` (Task 1).
- Produces:

```cpp
class UsbMediaWatcher : public QObject {
    explicit UsbMediaWatcher(QObject* parent = nullptr);
    void start();   // no-op if session bus lacks udisks2 (dev machines)
    void stop();
    Q_INVOKABLE void ejectMount(const QString& mountPath);  // Unmount + best-effort PowerOff
signals:
    void volumeMounted(const QString& mountPath, const QString& label);
    void volumeRemoved(const QString& mountPath);
    void ejectCompleted(const QString& mountPath, bool ok);
};
// PlayQueue addition:
int removeTracksUnder(const QString& pathPrefix);  // returns #removed;
// keeps current track if it survives; if current removed -> currentIndex
// moves to the next surviving track (or -1/empty).
```

udisks2 mapping (mirror the BlueZ code shape, service
`org.freedesktop.UDisks2`, root `/org/freedesktop/UDisks2`):
- `InterfacesAdded` carrying `org.freedesktop.UDisks2.Filesystem` on a
  block object whose `Drive` is removable → call `Mount()` on the
  Filesystem interface (`QDBusInterface`, options `{}`); on reply, emit
  `volumeMounted(mountPoint, idLabelOrDeviceName)`. Objects already
  mounted (MountPoints non-empty in `GetManagedObjects` initial scan or in
  the added props) emit `volumeMounted` with the existing mount point —
  no Mount call.
- `InterfacesRemoved` naming `org.freedesktop.UDisks2.Filesystem` → emit
  `volumeRemoved(lastKnownMountPath)` (track object-path→mount in a
  QHash).
- `ejectMount`: find the object path for the mount, call `Unmount({})`,
  then `PowerOff({})` on its Drive (failures logged, `ejectCompleted(false)`).
- Property demarshalling: `MountPoints` arrives as `aay`
  (`QList<QByteArray>`, NUL-terminated) — strip the trailing NUL.

Plugin wiring (`MediaPlayerPlugin`):
- `initialize()`: create watcher (after scanner), `start()` it;
  `volumeMounted` → `refreshSources()` (which now rescans — Task 5);
  `volumeRemoved(mount)` → **yank recovery** (design §9):
  1. `library_->removeVolume(MediaScanner::volumeKeyFor(mount))`
  2. `const bool currentGone = queue_->currentTrack().startsWith(mount + "/")`
  3. `queue_->removeTracksUnder(mount + "/")`
  4. if `currentGone`: if queue non-empty → `policy_.onNewQueue();
     startTrack(queue_->currentTrack())` (continue with what remains);
     else → `engine_->stop(); setHasTrack(false)`
  5. `refreshSources()`
- `shutdown()`: `watcher_->stop()`.
- New Q_INVOKABLE `ejectVolume(QString mountPath)` forwarding to the
  watcher; QML Folders top-level rows whose path starts with `/media/`,
  `/run/media/`, or `/mnt/` show an eject icon calling it.

- [ ] **Step 1: Write the failing tests** (`tests/test_media_usb_policy.cpp`)

```cpp
#include <QtTest>
#include "plugins/media_player/PlayQueue.hpp"

using oap::plugins::PlayQueue;

class TestMediaUsbPolicy : public QObject {
    Q_OBJECT
private slots:
    void purgeKeepsCurrentWhenItSurvives() {
        PlayQueue q;
        q.setTracks({"/home/m/a.mp3", "/media/usb/b.mp3", "/home/m/c.mp3"}, 0);
        QCOMPARE(q.removeTracksUnder("/media/usb/"), 1);
        QCOMPARE(q.count(), 2);
        QCOMPARE(q.currentTrack(), QStringLiteral("/home/m/a.mp3"));
    }
    void purgeAdvancesWhenCurrentDies() {
        PlayQueue q;
        q.setTracks({"/media/usb/a.mp3", "/home/m/b.mp3", "/media/usb/c.mp3"}, 0);
        QCOMPARE(q.removeTracksUnder("/media/usb/"), 2);
        QCOMPARE(q.count(), 1);
        QCOMPARE(q.currentTrack(), QStringLiteral("/home/m/b.mp3"));
    }
    void purgeCanEmptyTheQueue() {
        PlayQueue q;
        q.setTracks({"/media/usb/a.mp3", "/media/usb/b.mp3"}, 1);
        QCOMPARE(q.removeTracksUnder("/media/usb/"), 2);
        QCOMPARE(q.count(), 0);
        QVERIFY(q.currentTrack().isEmpty());
    }
    void prefixIsPathBoundary() {
        PlayQueue q;
        q.setTracks({"/media/usb2/x.mp3", "/media/usb/y.mp3"}, 0);
        QCOMPARE(q.removeTracksUnder("/media/usb/"), 1);   // usb2 survives
        QCOMPARE(q.count(), 1);
    }
};

QTEST_APPLESS_MAIN(TestMediaUsbPolicy)
#include "test_media_usb_policy.moc"
```

Register: `oap_add_test(test_media_usb_policy SOURCES test_media_usb_policy.cpp)`

- [ ] **Step 2: Run to verify failure** — compile error:
  `'removeTracksUnder' is not a member of 'oap::plugins::PlayQueue'`.
- [ ] **Step 3: Implement `PlayQueue::removeTracksUnder`** (keep shuffle
  order consistent: rebuild order after removal the same way `setTracks`
  does; preserve `currentIndex_` semantics per the test contract). Run the
  test → PASS.
- [ ] **Step 4: Implement `UsbMediaWatcher`** per the mapping above
  (D-Bus glue: no unit test — bench-verified in Task 8; keep ALL policy in
  the plugin wiring + PlayQueue where it is tested).
- [ ] **Step 5: Wire the plugin + QML eject button; full suite + app target**

```bash
cmake --build ~/builds/openauto-prodigy -j"$(nproc)"
cmake --build ~/builds/openauto-prodigy --target openauto-prodigy -j"$(nproc)"
cd ~/builds/openauto-prodigy && ctest --output-on-failure
```
Expected: green. On the WSL build host udisks2 is absent — `start()` must
log one warning and disable itself (assert manually: app runs, no crash,
warning present).

- [ ] **Step 6: Commit**

```bash
cd /mnt/e/claude/personal/openautopro/openauto-prodigy
git add src/plugins/media_player/UsbMediaWatcher.hpp src/plugins/media_player/UsbMediaWatcher.cpp src/plugins/media_player/PlayQueue.hpp src/plugins/media_player/PlayQueue.cpp src/plugins/media_player/MediaPlayerPlugin.hpp src/plugins/media_player/MediaPlayerPlugin.cpp qml/applications/media_player/MediaPlayerView.qml src/CMakeLists.txt tests/test_media_usb_policy.cpp tests/CMakeLists.txt
git commit -m "feat: udisks2 hot-plug watcher, safe eject, yank-mid-playback recovery"
```

---

### Task 7: Installer + docs

**Tier:** sonnet

**Files:**
- Create: `config/udisks-polkit.rules`
- Modify: `install.sh` (runtime package + polkit rule install, mirroring
  the BlueZ block at 1439-1443; package near the runtime apt installs)
- Modify: `docs/reference/config-schema.md` (plugin_config
  `org.openauto.media-player` keys if the schema documents plugin_config —
  verify; if it does not, skip with a stated note)
- Modify: `docs/wishlist.md` (delete the promoted "Extract playback-policy
  state machine" entry — Task 1 completed it)

**Interfaces:** consumes only names fixed above (service user/group
conventions from the existing BlueZ rule file `config/bluez-agent-polkit.rules`).

- [ ] **Step 1:** `config/udisks-polkit.rules` — mirror the structure of
  `config/bluez-agent-polkit.rules`, granting the service user:
  `org.freedesktop.udisks2.filesystem-mount`,
  `org.freedesktop.udisks2.filesystem-mount-other-seat`,
  `org.freedesktop.udisks2.filesystem-unmount-others`,
  `org.freedesktop.udisks2.power-off-drive`. (Read the BlueZ file first;
  keep the same user/group predicate it uses.)
- [ ] **Step 2:** `install.sh`: add `udisks2` to the runtime apt packages
  (NOT the build-dep line 814 — that one only gains libavformat-dev in
  Task 2); install the rule beside the BlueZ one (pattern 1439-1443,
  target `/etc/polkit-1/rules.d/50-openauto-udisks.rules`).
- [ ] **Step 3:** wishlist deletion + config-schema check;
  `python3 scripts/check-doc-links.py` → exit 0.
- [ ] **Step 4: Commit**

```bash
git add config/udisks-polkit.rules install.sh docs/wishlist.md docs/reference/config-schema.md
git commit -m "feat: udisks2 runtime dep + polkit rule; close promoted wishlist item"
```

---

### Task 8: Verification, Codex gate, landing + bench checklist

**Tier:** main (Fable session — gate adjudication and Pi deploy; do not
dispatch)

**Files:**
- Modify: `docs/session-handoffs.md` (entry; rotate first if >300 lines)
- Modify: `docs/roadmap-current.md` ("Now" item → stage 2 shipped, bench
  pending)
- Modify: `docs/plans/2026-07-08-media-player-design.md` (Status header →
  stage 2 shipped, bench pending)
- Move: `docs/plans/2026-07-09-media-player-stage2-plan.md` →
  `docs/archive/plans/` (status flipped, same commit as the handoff)

- [ ] **Step 1:** Full local verification (reconfigure + full build + app
  target + `ctest --output-on-failure` — all green; `--version` still
  reports the ALPHA scheme string).
- [ ] **Step 2:** `bash scripts/codex-review.sh` — adjudicate every finding
  (confirmed → fix, dismissed → stated reason); substantial fixes → one
  re-run.
- [ ] **Step 3:** Cross-build + deploy + restart
  (`./cross-build.sh` [maybe `sg docker -c`], rsync binary to
  `matt@192.168.1.149:~/openauto-prodigy/build/src/`, restart service,
  journal clean). **Also run the installer's new steps manually on the Pi**
  (udisks2 package + polkit rule) — the Pi was installed before Task 7.
- [ ] **Step 4:** Prepare the bench checklist for Matthew (design §12
  stage-2 rows, USB-stick rows LAST): cold scan timing on a real stick /
  incremental rescan timing / Artists→Albums→Tracks navigation / cover-art
  grid / hot-plug appears as source / safe eject / yank mid-playback
  recovers per §9 / nothing auto-plays at boot (regression row) / Settings
  version row still correct (regression row).
- [ ] **Step 5:** Handoff entry (what/why/gate counts/verification/bench
  checklist + pending rows), roadmap + design status updates, archive this
  plan. NO git tag (milestone tags only on Matthew's call). Push ONLY with
  Matthew's go-ahead.

---

## Execution notes

- Order is strict 1→2→3→4→5→6→7→8 (each consumes the previous; 7 could
  float but keeps the installer aligned with what 6 shipped).
- Workers report synthesized results only (files, test command +
  pass/fail, deviations). Escalation ladder per AGENTS.md: two attempts →
  Codex (workspace-write, prompt file) → Fable.
- Cache/scan performance row (design §12 "seconds incremental, under a
  minute cold") is bench-verified on the Pi with a real stick — no
  synthetic perf test in CI.
