#pragma once

#include <oaa/Channel/IChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>

namespace oaa {
namespace hu {

class MediaStatusChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    explicit MediaStatusChannelHandler(QObject* parent = nullptr);

    uint8_t channelId() const override { return oaa::ChannelId::MediaStatus; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload) override;

    enum PlaybackState {
        Stopped = 1,
        Playing = 2,
        Paused  = 3
    };

signals:
    /// Emitted when playback state changes
    void playbackStateChanged(int state, const QString& appName);

    /// Emitted when track metadata changes (album art as raw PNG bytes)
    void metadataChanged(const QString& title, const QString& artist,
                          const QString& album, const QByteArray& albumArt);

private:
    void handlePlaybackStatus(const QByteArray& payload);
    void handlePlaybackMetadata(const QByteArray& payload);
};

} // namespace hu
} // namespace oaa
