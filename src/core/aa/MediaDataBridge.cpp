#include "core/aa/MediaDataBridge.hpp"
#include "core/aa/AndroidAutoOrchestrator.hpp"

#ifdef HAS_BLUETOOTH
#include "plugins/bt_audio/BtAudioPlugin.hpp"
#endif

#include <oaa/HU/Handlers/MediaStatusChannelHandler.hpp>

namespace oap {
namespace aa {

MediaDataBridge::MediaDataBridge(QObject* parent)
    : QObject(parent)
{
}

void MediaDataBridge::connectToAAOrchestrator(AndroidAutoOrchestrator* orch)
{
    if (!orch) return;
    orchestrator_ = orch;

    // Switch source on AA connect/disconnect
    connect(orch, &AndroidAutoOrchestrator::connectionStateChanged, this,
            &MediaDataBridge::onAAConnectionChanged);

    // AA metadata and playback state from MediaStatusChannelHandler
    // Use Qt::QueuedConnection -- handler signals come from ASIO thread
    auto* handler = orch->mediaStatusHandler();
    if (handler) {
        connect(handler, &oaa::hu::MediaStatusChannelHandler::metadataChanged,
                this, &MediaDataBridge::onAAMetadata, Qt::QueuedConnection);
        connect(handler, &oaa::hu::MediaStatusChannelHandler::playbackStateChanged,
                this, &MediaDataBridge::onAAPlaybackState, Qt::QueuedConnection);
    }
}

void MediaDataBridge::connectToBtAudio(oap::plugins::BtAudioPlugin* bt)
{
    if (!bt) return;

#ifdef HAS_BLUETOOTH
    btPlugin_ = bt;

    connect(bt, &oap::plugins::BtAudioPlugin::metadataChanged, this,
            &MediaDataBridge::onBTMetadataChanged);
    connect(bt, &oap::plugins::BtAudioPlugin::playbackStateChanged, this,
            &MediaDataBridge::onBTPlaybackStateChanged);
    connect(bt, &oap::plugins::BtAudioPlugin::connectionStateChanged, this,
            &MediaDataBridge::onBTConnectionStateChanged);

    // Read current BT state immediately
    readBtState();
    if (bt->connectionState() == 1 && source_ == None) {
        setSource(Bluetooth);
    }
#else
    Q_UNUSED(bt)
#endif
}

void MediaDataBridge::playPause()
{
    if (source_ == AndroidAuto && orchestrator_) {
        orchestrator_->sendButtonPress(85);  // MEDIA_PLAY_PAUSE
    }
#ifdef HAS_BLUETOOTH
    else if (source_ == Bluetooth && btPlugin_) {
        if (btPlaybackState_ == 1) {  // Playing
            btPlugin_->pause();
        } else {
            btPlugin_->play();
        }
    }
#endif
}

void MediaDataBridge::next()
{
    if (source_ == AndroidAuto && orchestrator_) {
        orchestrator_->sendButtonPress(87);  // MEDIA_NEXT
    }
#ifdef HAS_BLUETOOTH
    else if (source_ == Bluetooth && btPlugin_) {
        btPlugin_->next();
    }
#endif
}

void MediaDataBridge::previous()
{
    if (source_ == AndroidAuto && orchestrator_) {
        orchestrator_->sendButtonPress(88);  // MEDIA_PREVIOUS
    }
#ifdef HAS_BLUETOOTH
    else if (source_ == Bluetooth && btPlugin_) {
        btPlugin_->previous();
    }
#endif
}

QString MediaDataBridge::title() const
{
    switch (source_) {
    case AndroidAuto: return aaTitle_;
    case Bluetooth: return btTitle_;
    default: return {};
    }
}

QString MediaDataBridge::artist() const
{
    switch (source_) {
    case AndroidAuto: return aaArtist_;
    case Bluetooth: return btArtist_;
    default: return {};
    }
}

QString MediaDataBridge::album() const
{
    switch (source_) {
    case AndroidAuto: return aaAlbum_;
    case Bluetooth: return btAlbum_;
    default: return {};
    }
}

int MediaDataBridge::playbackState() const
{
    switch (source_) {
    case AndroidAuto: return aaPlaybackState_;
    case Bluetooth: return btPlaybackState_;
    default: return 0;
    }
}

int MediaDataBridge::source() const
{
    return static_cast<int>(source_);
}

bool MediaDataBridge::hasMedia() const
{
    return !title().isEmpty() || !artist().isEmpty();
}

QString MediaDataBridge::appName() const
{
    return aaAppName_;
}

void MediaDataBridge::setSource(Source newSource)
{
    if (source_ == newSource) return;
    source_ = newSource;
    emit sourceChanged();
    emitCurrentMetadata();
}

void MediaDataBridge::emitCurrentMetadata()
{
    emit metadataChanged();
    emit playbackStateChanged();
}

void MediaDataBridge::onAAConnectionChanged()
{
    if (!orchestrator_) return;

    if (orchestrator_->isAaConnected()) {
        // AA connected -- switch to AA source, clear AA metadata cache
        aaTitle_.clear();
        aaArtist_.clear();
        aaAlbum_.clear();
        aaPlaybackState_ = 0;
        aaAppName_.clear();
        albumArt_.clear();
        setSource(AndroidAuto);
    } else {
        // AA disconnected -- fall back to BT or None
#ifdef HAS_BLUETOOTH
        if (btPlugin_ && btPlugin_->connectionState() == 1) {
            readBtState();
            setSource(Bluetooth);
        } else {
            setSource(None);
        }
#else
        setSource(None);
#endif
    }
}

void MediaDataBridge::onAAMetadata(const QString& t, const QString& a,
                                    const QString& al, const QByteArray& art)
{
    aaTitle_ = t;
    aaArtist_ = a;
    aaAlbum_ = al;
    albumArt_ = art;

    if (source_ == AndroidAuto) {
        emit metadataChanged();
    }
}

void MediaDataBridge::onAAPlaybackState(int state, const QString& app)
{
    aaPlaybackState_ = state;
    bool appChanged = (aaAppName_ != app);
    aaAppName_ = app;

    if (source_ == AndroidAuto) {
        emit playbackStateChanged();
    }
    if (appChanged) {
        emit appNameChanged();
    }
}

void MediaDataBridge::onBTMetadataChanged()
{
#ifdef HAS_BLUETOOTH
    if (!btPlugin_) return;
    btTitle_ = btPlugin_->trackTitle();
    btArtist_ = btPlugin_->trackArtist();
    btAlbum_ = btPlugin_->trackAlbum();

    if (source_ == Bluetooth) {
        emit metadataChanged();
    }
#endif
}

void MediaDataBridge::onBTPlaybackStateChanged()
{
#ifdef HAS_BLUETOOTH
    if (!btPlugin_) return;
    btPlaybackState_ = btPlugin_->playbackState();

    if (source_ == Bluetooth) {
        emit playbackStateChanged();
    }
#endif
}

void MediaDataBridge::onBTConnectionStateChanged()
{
#ifdef HAS_BLUETOOTH
    if (!btPlugin_) return;

    if (btPlugin_->connectionState() == 0) {
        // BT disconnected
        if (source_ == Bluetooth) {
            btTitle_.clear();
            btArtist_.clear();
            btAlbum_.clear();
            btPlaybackState_ = 0;
            setSource(None);
        }
    } else {
        // BT connected -- if not in AA, switch to BT
        if (source_ == None) {
            readBtState();
            setSource(Bluetooth);
        }
    }
#endif
}

void MediaDataBridge::readBtState()
{
#ifdef HAS_BLUETOOTH
    if (!btPlugin_) return;
    btTitle_ = btPlugin_->trackTitle();
    btArtist_ = btPlugin_->trackArtist();
    btAlbum_ = btPlugin_->trackAlbum();
    btPlaybackState_ = btPlugin_->playbackState();
#endif
}

} // namespace aa
} // namespace oap
