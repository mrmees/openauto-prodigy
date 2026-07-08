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
