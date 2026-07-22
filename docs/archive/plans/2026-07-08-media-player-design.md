# Local Media Player Plugin — Design

Status: COMPLETED 2026-07-21

**Completion:** Stages 1 and 2 shipped and completed their Pi bench matrix by
2026-07-10; `ALPHA-26-07-10-01` is the declared stage-2 milestone. Further
media-player ideas remain in `docs/wishlist.md` and require promotion into a
new design.

**Date:** 2026-07-08 · **Status:** approved (brainstorm 2026-07-08, Matthew)
**Amended 2026-07-09** (prior-art brainstorm, Matthew): scanner heuristics +
credits folded into §8, Elisa UI reference in §7. Survey outcome: no existing
project fits libavformat+QML+zero-deps (ecosystem standard is TagLib+SQLite);
adopt heuristics as credited reference, not code — Yarock and VLC medialibrary
adoption explicitly rejected (deps, Widgets→QML mismatch, weight).
**Roadmap item:** Done — "Local media player plugin" (`docs/roadmap-current.md`)
**Supersedes:** the F1 light-plan sketch (`docs/plans/2026-07-05-phase-f-light-plans.md` §F1) — this is the full design the sketch called for.
**Grounding:** integration seams verified against develop @ `192b0fa` (MediaStatusService, IMediaStatusProvider, NowPlayingWidget, ActionRegistry, ApiSerializers, AudioService, plugin/CMake/main.cpp registration pattern).

## 1. Outcome

A native media player plugin (Qt Multimedia) that plays local files from USB
sticks and `~/Music`, with a scanned library (Artists/Albums/Tracks + cover
art) and a folder browser, integrated with `MediaStatusService`, the
now-playing widget, the EQ/volume/ducking audio stack, and External API v1.
Closes the biggest remaining daily-driver parity gap: prodigy currently only
plays BT audio.

## 2. Settled constraints (not revisited here)

- Native QML + Qt Multimedia. Core media playback never depends on WebEngine
  (design-philosophy §8).
- Wireless-only project posture is unaffected; USB here means storage, not AA.
- `proto/api/` is additive-only. The `open-android-auto` submodule is
  read-only.

## 3. Decisions made in this brainstorm

| Question | Decision |
|----------|----------|
| Media sources (v1) | USB storage + `~/Music`. NAS sync-at-home deferred (wishlist candidate); NAS live streaming rejected (car has no LAN — the Pi *is* the AP). |
| Library UX | Scanned library (Artists / Albums / Tracks, cover-art grid) **plus** folder-browser fallback for untagged files. |
| Audio path | QMediaPlayer for transport + Qt 6.8 `QAudioBufferOutput` PCM tap → `AudioService::writeAudio()`. Full EQ/volume/ducking/focus integration. Verified `QAudioBufferOutput` exists in bench Qt 6.8.2. |
| Metadata reach | Widen the shared surface: `IMediaStatusProvider` gains position/duration/artwork; proto uses its reserved slots. All sources benefit. |
| Coexistence policy | **Playing-wins** arbitration (see §6). Manual start only — no autoplay on boot; saved state restores paused. |
| Dashboard widget | Upgrade the single unified `NowPlayingWidget` (art, progress, source badge). No second widget. |
| Build slicing | Two stages inside one arc. Stage 1 = playable + integrated (folder browse). Stage 2 = library + USB automount. Each independently Pi-verifiable. |

## 4. Architecture

New static plugin `MediaPlayerPlugin` (`src/plugins/media_player/`, id
`org.openauto.media-player`), mirroring the bt_audio pattern: registered in
`main.cpp` beside the other static plugins, QML view under
`qml/applications/media_player/`, sources added to the `src/CMakeLists.txt`
lists (compile, QML resource properties, qml file list), config under the
existing `plugin_config.org.openauto.media-player` YAML namespace.

Three units inside the plugin, each independently testable:

- **PlaybackEngine** — owns QMediaPlayer, the PCM tap, and the AudioService
  stream. Interface: set queue, play/pause/next/previous/seek; emits
  state/position/metadata signals. No UI or library knowledge.
- **PlayQueue** — queue semantics: tapping a track in any container (folder,
  album, track list) makes that container the queue, starting at the tapped
  track. Shuffle + repeat (off/all/one) live here. Pure logic.
- **MediaSourceWatcher** — enumerates playable roots (`~/Music` + configured
  extra dirs + mounted USB volumes). Stage 1: `QStorageInfo` snapshot at
  activation. Stage 2: udisks2 D-Bus hot-plug watcher (modeled on the BlueZ
  ObjectManager watcher in `BtAudioPlugin.cpp`).

Stage 2 adds:

- **MediaLibrary** — libavformat tag scanner (worker thread), in-memory
  Artist→Album→Track index, on-disk incremental cache, art extraction cache.
  Exposes QAbstractListModels for the library tabs.

## 5. Audio pipeline

```
QMediaPlayer (file URL)
  └─ QAudioBufferOutput(QAudioFormat{48kHz, S16, stereo})   // Qt resamples
       └─ audioBufferReceived(QAudioBuffer)
            └─ AudioService::writeAudio(stream, pcm, size)   // same as AA media
```

Stream creation mirrors `AndroidAutoOrchestrator.cpp:326-367` exactly:
`createStream("Local Media", priority 50, 48000, 2, "auto", bufferMs)` and
`stream->eqEngine = eqService->engineForStream(StreamId::Media)`. EQ, master
volume, ducking, and audio focus therefore apply with no new audio plumbing.
No `QAudioOutput` device sink is attached — PCM goes only through
AudioService.

**Spike = plan task #1 (load-bearing bet):** verify that QMediaPlayer paces
`QAudioBufferOutput` delivery in real time when no device output is attached,
and that position/duration/seek still function. Bench-testable on WSL (same
Qt 6.8.2). **Fallback if pacing fails:** QMediaPlayer → its own PipeWire
stream (F1's original option a) — only the stream-integration slice of the
plan changes; UI, library, arbitration, and API work are unaffected.

Free win from QMediaPlayer: `metaData()` exposes embedded cover art
(ffmpeg backend), so the *current track* gets artwork in stage 1 via a
`QQuickImageProvider` (`image://mediaart/current?rev=N`) — no scanner needed.

## 6. Arbitration & shared surface

### MediaStatusService: 2-source switch → 3-source playing-wins

Current logic (hard "AA connected wins", `MediaStatusService.cpp:76-134`) is
replaced:

1. Any source whose playbackState == playing owns the display and the
   `media.*` actions. Most-recently-started-playing breaks ties.
2. If nothing is playing, the last active source keeps the display until it
   disconnects; then fall back by recency.
3. AA being merely *connected* no longer suppresses local/BT display.

Per-source cached state grows to include position/duration/artUrl. The
`media.playPause/next/previous` callbacks in `main.cpp` (~line 620) gain a
MediaPlayer branch keyed on the active source. Existing AA>BT tests get
rewritten as a playing-wins matrix.

Audible-focus policy (distinct from display arbitration):

- Local play → request AudioService focus (Gain); if BT is playing, send
  AVRCP pause (`btAudioPlugin->pause()`).
- BT/AA starts playing → pause local playback.
- AA nav prompts (Speech stream, GainTransientMayDuck) duck local media
  rather than pausing it.
- **Plan-verify items:** (a) whether AudioService focus has loss/duck
  callbacks between streams or needs a small extension; (b) whether the AA
  channel handler can push audio-focus-loss to a phone mid-session to pause
  phone media. If (b) is not feasible in v1, worst case is both sources
  audible until the user pauses one — annoying, not broken.

### IMediaStatusProvider widening

New fields: `position` (ms), `duration` (ms), `hasPosition` (AA reports
none → widget hides progress cleanly), `artUrl` (QML-consumable string; empty
when unavailable). BT already exposes trackPosition/trackDuration on the
plugin — wiring them into the shared surface is a cheap stage-1 win. AA
supplies nothing new.

### External API v1 (additive)

- `proto/api/media.proto`: `MEDIA_SOURCE_LOCAL_MEDIA = 4` (consumes a
  reserved slot); position/duration fields in the slots reserved for exactly
  this (8-15 range).
- `ApiSerializers.cpp:116-159`: source-string branch for `"MediaPlayer"` +
  an explicit playbackState mapping case for the new source (canonical local
  states: 0 stopped / 1 playing / 2 paused — the per-source switch keeps
  other sources' codes untouched).
- Artwork stays **off** the API for now — a local file path is useless to a
  network client. QML-only until a real consumer exists.

### NowPlayingWidget upgrade

Cover-art thumbnail, progress bar (hidden when `!hasPosition`), source badge
(BT/AA/local), title/artist marquee, transport. Layout degrades gracefully at
the 2x1 minimum size (art may drop out — exact breakpoints are implementer's
choice within the existing widget size presets).

## 7. Player UX (1024x600 touch)

`MediaPlayerView`: tab row **Artists / Albums / Tracks / Folders** + content
area + persistent bottom now-playing bar (art thumb, marquee title/artist,
seekable progress, transport, shuffle/repeat toggles). Stage 1 ships Folders
only; library tabs light up in stage 2.

- **Albums:** cover-art GridView → track list.
- **Artists:** list → albums → tracks.
- **Folders:** directories-first alpha sort, breadcrumb back-navigation,
  audio-extension filter (mp3, flac, ogg, opus, m4a, aac, wav).
- Tap a track → container becomes the queue at that track.
- Built from existing controls (Tile, NormalText, ThemeService colors), same
  idiom as other app views. Touch targets sized for gloves-off driving use.
- Visual reference for the Albums grid: Elisa's (KDE) QML album views —
  reference only, no KDE dependencies.

## 8. Library scanner (stage 2)

**libavformat, not TagLib** — FFmpeg is already linked for video decode;
`avformat_open_input` + `av_dict_get` reads tags and embedded art
(`AV_DISPOSITION_ATTACHED_PIC`) with **no new third-party libraries linked**.

*Amendment 2026-07-09/10:* the "nothing added to install.sh,
docs/development.md, or the cross-build Docker image" phrasing above no
longer holds literally. `libavformat-dev`/`libavcodec-dev`/`libavutil-dev`
(2026-07-09, `741fe6b`) were added at all three build sites — but they're
the dev-header half of the FFmpeg package family already linked for video,
not a new external library. Separately, §9's USB handling landed a genuine
new **runtime-only** package, `udisks2`, plus its polkit rule, in
`install.sh` and `docs/development.md` (Task 7, 2026-07-10). The original
intent stands: no new third-party library is *linked into the binary* for
either the tag reader or USB handling.

- Worker-thread scan with progress signal; UI (and the Folders tab) stay
  usable during scans.
- In-memory index; on-disk cache keyed by volume-UUID + path + mtime + size —
  incremental rescans touch only changed files. Cache lives in
  `~/.openauto/cache/`.
- Extracted art → `~/.openauto/cache/art/<hash>.jpg`; `artUrl` points there.
- Scale target: a few-thousand-track USB stick — seconds incremental, under a
  minute cold.

### Prior art & heuristics (amendment 2026-07-09)

Requirements distilled from Yarock's and Strawberry's collection code —
the mistakes hand-rolled scanners make on the first pass:

1. **Album identity:** group by `(albumartist ?: artist, album)`. Same album
   name + differing track-artists + no albumartist → ONE "Various Artists"
   album (compilation flag in the index), never N duplicate albums.
2. **Tag fallbacks:** missing title ← filename stem; missing artist/album ←
   "Unknown Artist"/"Unknown Album" buckets; track number parses both `3` and
   `3/12`; disc number folds into the sort key (`disc*1000 + track`) so
   multi-disc albums order correctly.
3. **Art priority:** embedded `ATTACHED_PIC` → else
   `cover|folder|front.{jpg,png}` in the track's directory → else none.
   Cached per-album, not per-track.
4. **Scan hygiene:** skip hidden files/dirs; symlink-loop guard via
   canonical-path visited set.
5. **Credits:** header comment in the scanner/MediaLibrary sources + this
   note: *library heuristics informed by Yarock (GPL-3.0,
   github.com/sebaro/Yarock) and Strawberry (GPL-3.0); no code copied.* If
   implementation does copy code (GPL3↔GPL3, permitted), that file carries
   per-file attribution instead.

## 9. USB handling (stage 2)

- udisks2 D-Bus watcher: `InterfacesAdded` for filesystem block devices →
  `Mount()` → scan → volume appears as a source. Safe-eject button in the
  sources UI.
- Installer additions: `udisks2` package + a polkit rule for the service user
  (mirrors the existing BlueZ polkit setup in install.sh).
- Yank-without-eject mid-playback: playback error → drop that volume's queue
  entries, mark its library entries unavailable, continue with what remains.
- Stage 1 behavior (no automount yet): `QStorageInfo` sees whatever is
  already mounted; realistically `~/Music` only on a kiosk install.

## 10. Config & persistence

`plugin_config.org.openauto.media-player`:

- `music_dirs` — default `[~/Music]`, extra dirs addable.
- Persisted shuffle/repeat state.
- Saved queue + track + position on shutdown/pause. Restores **paused** —
  one tap resumes; nothing auto-plays on boot (Matthew's explicit call).

## 11. Error handling

- Corrupt/unsupported file: QMediaPlayer error → skip to next, log +
  NotificationService toast. Three consecutive failures → stop (a dead USB
  stick must not machine-gun through 400 skips).
- Missing art: themed placeholder.
- Scanner: skip unreadable files, count them in the log.
- USB yank: §9.
- PCM underrun: buffer sizing follows the AA media stream's `bufferMs`
  approach; spike findings inform the default.

## 12. Testing

Unit (extends the current 88):

- PlayQueue: next/prev/wraparound, shuffle, repeat off/all/one.
- MediaStatusService: playing-wins matrix across 3 sources (replaces the
  AA>BT precedence tests).
- ApiSerializers: `"MediaPlayer"` source + playbackState mapping.
- Scanner (stage 2): tag extraction against tiny ffmpeg-generated mp3/flac
  fixtures in `tests/data/`.
- Folder model: extension filtering, sort order.

On-Pi verify checklists:

- **Stage 1:** play from `~/Music`; EQ preset audibly changes local playback;
  master volume applies; widget shows art + progress; BT playback pauses when
  local starts (and vice versa); AA nav prompt ducks local audio; API v1
  media stream reports `LOCAL_MEDIA`; folder browse works by touch; state
  restores paused after service restart.
- **Stage 2:** cold + incremental scan timing; Artists/Albums/Tracks
  navigation; cover-art grid; USB hot-plug appears as source; safe eject;
  yank mid-playback recovers per §11.

## 13. Open items the plan must resolve

1. **Spike (plan task #1):** QAudioBufferOutput real-time pacing without a
   device sink; seek/position behavior. Go/no-go gates the stream-integration
   tasks.
2. AudioService focus semantics: loss/duck callbacks between streams —
   extend if absent.
3. AA audio-focus push-to-phone mid-session: feasible with the current
   channel handler?
4. Exact NowPlayingWidget responsive breakpoints at small sizes.
5. Whether `qml6-module-qtmultimedia` is needed anywhere (expected: no — all
   Qt Multimedia use is C++-side; verify nothing QML imports QtMultimedia).

## 14. Explicitly out of scope

- NAS sync-at-home (wishlist candidate; revisit after this arc).
- NAS live streaming (rejected — no LAN in the car).
- M3U/playlist file support and playlist editing.
- BT/AA cover art (AVRCP OBEX art, AA art) — interface field stays empty for
  those sources.
- Artwork over the external API.
- Video file playback (audio only).
