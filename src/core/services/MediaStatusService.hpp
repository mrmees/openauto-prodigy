#pragma once

#include "IMediaStatusService.hpp"
#include <QByteArray>
#include <functional>

namespace oap {

/// Core service owning media status merging from AA and BT sources.
/// Lives independently of the BT audio UI plugin.
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
    QString source() const override;
    QString appName() const override;
    void playPause() override;
    void next() override;
    void previous() override;

    /// Set playback control callbacks (delegated to active source)
    using Callback = std::function<void()>;
    void setPlaybackCallbacks(Callback play, Callback next, Callback prev);

    // AA source management
    void setAaConnected(bool connected);
    void updateAaMetadata(const QString& title, const QString& artist, const QString& album);
    void updateAaPlaybackState(int state, const QString& appName = {});

    // BT source management
    void setBtConnected(bool connected);
    void updateBtMetadata(const QString& title, const QString& artist, const QString& album);
    void updateBtPlaybackState(int state);

private:
    enum ActiveSource { None, Bluetooth, AndroidAuto };

    void setActiveSource(ActiveSource src);
    void emitCurrentMetadata();

    ActiveSource activeSource_ = None;

    // AA cached state
    QString aaTitle_, aaArtist_, aaAlbum_, aaAppName_;
    int aaPlaybackState_ = 0;

    // BT cached state
    QString btTitle_, btArtist_, btAlbum_;
    int btPlaybackState_ = 0;

    bool aaConnected_ = false;
    bool btConnected_ = false;

    Callback playCallback_, nextCallback_, prevCallback_;
};

} // namespace oap
