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
    void onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset = 0) override;

    enum PlaybackState {
        Stopped = 1,
        Playing = 2,
        Paused  = 3
    };

    /// Send a playback command (PAUSE=1, RESUME=2) to the phone
    void sendPlaybackCommand(int command);

    /// Toggle playback based on current state (sends PAUSE if playing, RESUME otherwise)
    void togglePlayback();

signals:
    /// Emitted when playback state changes
    void playbackStateChanged(int state, const QString& appName);

    /// Emitted when track metadata changes (album art as raw PNG bytes)
    void metadataChanged(const QString& title, const QString& artist,
                          const QString& album, const QByteArray& albumArt);

    /// Emitted after a playback command is sent (for orchestrator logging)
    void playbackCommandSent(int command);

private:
    void handlePlaybackStatus(const QByteArray& payload);
    void handlePlaybackMetadata(const QByteArray& payload);

    int currentPlaybackState_ = 0;  // default: unknown (not playing)
};

} // namespace hu
} // namespace oaa
