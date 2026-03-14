#include "MediaStatusService.hpp"

namespace oap {

MediaStatusService::MediaStatusService(QObject* parent)
    : IMediaStatusService(parent)
{
}

QString MediaStatusService::title() const {
    switch (activeSource_) {
    case AndroidAuto: return aaTitle_;
    case Bluetooth: return btTitle_;
    default: return {};
    }
}

QString MediaStatusService::artist() const {
    switch (activeSource_) {
    case AndroidAuto: return aaArtist_;
    case Bluetooth: return btArtist_;
    default: return {};
    }
}

QString MediaStatusService::album() const {
    switch (activeSource_) {
    case AndroidAuto: return aaAlbum_;
    case Bluetooth: return btAlbum_;
    default: return {};
    }
}

int MediaStatusService::playbackState() const {
    switch (activeSource_) {
    case AndroidAuto: return aaPlaybackState_;
    case Bluetooth: return btPlaybackState_;
    default: return 0;
    }
}

QString MediaStatusService::source() const {
    switch (activeSource_) {
    case AndroidAuto: return QStringLiteral("AndroidAuto");
    case Bluetooth: return QStringLiteral("Bluetooth");
    default: return {};
    }
}

QString MediaStatusService::appName() const {
    return aaAppName_;
}

bool MediaStatusService::hasMedia() const {
    return !title().isEmpty() || !artist().isEmpty();
}

void MediaStatusService::playPause() {
    if (playCallback_) playCallback_();
}

void MediaStatusService::next() {
    if (nextCallback_) nextCallback_();
}

void MediaStatusService::previous() {
    if (prevCallback_) prevCallback_();
}

void MediaStatusService::setPlaybackCallbacks(Callback play, Callback next, Callback prev) {
    playCallback_ = std::move(play);
    nextCallback_ = std::move(next);
    prevCallback_ = std::move(prev);
}

void MediaStatusService::setAaConnected(bool connected) {
    aaConnected_ = connected;
    if (connected) {
        aaTitle_.clear(); aaArtist_.clear(); aaAlbum_.clear();
        aaPlaybackState_ = 0; aaAppName_.clear();
        setActiveSource(AndroidAuto);
    } else {
        if (btConnected_) {
            setActiveSource(Bluetooth);
        } else {
            setActiveSource(None);
        }
    }
}

void MediaStatusService::updateAaMetadata(const QString& t, const QString& a, const QString& al) {
    aaTitle_ = t; aaArtist_ = a; aaAlbum_ = al;
    if (activeSource_ == AndroidAuto)
        emit mediaStatusChanged();
}

void MediaStatusService::updateAaPlaybackState(int state, const QString& app) {
    aaPlaybackState_ = state;
    aaAppName_ = app;
    if (activeSource_ == AndroidAuto)
        emit mediaStatusChanged();
}

void MediaStatusService::setBtConnected(bool connected) {
    btConnected_ = connected;
    if (connected) {
        if (activeSource_ == None)
            setActiveSource(Bluetooth);
    } else {
        if (activeSource_ == Bluetooth) {
            btTitle_.clear(); btArtist_.clear(); btAlbum_.clear();
            btPlaybackState_ = 0;
            setActiveSource(None);
        }
    }
}

void MediaStatusService::updateBtMetadata(const QString& t, const QString& a, const QString& al) {
    btTitle_ = t; btArtist_ = a; btAlbum_ = al;
    if (activeSource_ == Bluetooth)
        emit mediaStatusChanged();
}

void MediaStatusService::updateBtPlaybackState(int state) {
    btPlaybackState_ = state;
    if (activeSource_ == Bluetooth)
        emit mediaStatusChanged();
}

void MediaStatusService::setActiveSource(ActiveSource src) {
    if (activeSource_ == src) return;
    activeSource_ = src;
    emitCurrentMetadata();
}

void MediaStatusService::emitCurrentMetadata() {
    emit mediaStatusChanged();
}

} // namespace oap
