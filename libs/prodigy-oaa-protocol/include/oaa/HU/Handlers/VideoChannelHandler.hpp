#pragma once

#include <memory>
#include <atomic>
#include <QMap>
#include <QString>
#include <oaa/Channel/IAVChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>
#include <oaa/video/VideoFocusModeEnum.pb.h>

namespace oaa {
namespace hu {

class VideoChannelHandler : public oaa::IAVChannelHandler {
    Q_OBJECT
public:
    explicit VideoChannelHandler(QObject* parent = nullptr);
    VideoChannelHandler(uint8_t channelId,
                        oaa::proto::enums::VideoFocusMode::Enum setupFocusMode,
                        QObject* parent = nullptr);

    uint8_t channelId() const override { return channelId_; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload, int dataOffset = 0) override;

    // IAVChannelHandler
    void onMediaData(const QByteArray& data, uint64_t timestamp) override;
    bool canAcceptMedia() const override { return channelOpen_ && streaming_; }

    // Video focus control — called by orchestrator
    void requestVideoFocus(bool focused);

    /// Set how many video configs were advertised (for setup response)
    void setNumVideoConfigs(uint32_t n) { numVideoConfigs_ = n; }
    uint64_t receivedFrameCount() const { return receivedFrameCount_.load(); }
    uint64_t ackCount() const { return ackCount_.load(); }

signals:
    void setupRequested(int codec);
    void handlerError(const QString& message);
    void videoFrameData(std::shared_ptr<const QByteArray> data, qint64 enqueueTimeNs);
    void streamStarted(int32_t session, uint32_t configIndex);
    void streamStartDetailsReceived(int32_t sessionId, uint32_t configIndex,
                                    int sessionType, bool hasMediaConfig,
                                    QString mediaConfigSummary);
    void mediaOptionsReceived(const QString& boundedSummary);
    void streamStopped();
    void videoFocusChanged(int focusMode, bool unrequested);
    void uiConfigTokensReceived(const QMap<QString, uint32_t>& dayTokens,
                                 const QMap<QString, uint32_t>& nightTokens);

private:
    void handleSetupRequest(const QByteArray& payload);
    void handleStartIndication(const QByteArray& payload);
    void handleMediaOptions(const QByteArray& payload);
    void handleStopIndication();
    void handleVideoFocusRequest(const QByteArray& payload);
    void handleVideoFocusIndication(const QByteArray& payload);
    void sendAck();

    uint8_t channelId_ = oaa::ChannelId::Video;
    oaa::proto::enums::VideoFocusMode::Enum setupFocusMode_
        = oaa::proto::enums::VideoFocusMode::PROJECTED;
    int32_t session_ = -1;
    uint32_t numVideoConfigs_ = 1;
    bool channelOpen_ = false;
    bool streaming_ = false;
    std::atomic<uint64_t> receivedFrameCount_{0};
    std::atomic<uint64_t> ackCount_{0};
};

} // namespace hu
} // namespace oaa
