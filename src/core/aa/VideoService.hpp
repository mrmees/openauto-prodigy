#pragma once

#include <QObject>
#include <QByteArray>
#include <memory>
#include <boost/asio.hpp>

#include "IService.hpp"
#include "VideoDecoder.hpp"
#include "../../core/Configuration.hpp"

#include <aasdk/Channel/AV/VideoServiceChannel.hpp>
#include <aasdk/Channel/AV/IVideoServiceChannelEventHandler.hpp>

namespace oap { class YamlConfig; }

namespace oap {
namespace aa {

// Internal video focus mode — maps to AA proto VideoFocusMode for wire protocol.
// Projection and NativeTransient both map to proto FOCUSED; Native maps to UNFOCUSED.
enum class VideoFocusMode {
    Projection,         // AA active, phone renders → proto FOCUSED
    Native,             // HU's own UI, AA paused → proto UNFOCUSED
    NativeTransient     // Brief interruption (e.g. reverse camera), phone keeps rendering → proto FOCUSED
};

class VideoService
    : public QObject
    , public IService
    , public aasdk::channel::av::IVideoServiceChannelEventHandler
    , public std::enable_shared_from_this<VideoService>
{
    Q_OBJECT

public:
    VideoService(boost::asio::io_service& ioService,
                 aasdk::messenger::IMessenger::Pointer messenger,
                 std::shared_ptr<oap::Configuration> config,
                 VideoDecoder* decoder,
                 oap::YamlConfig* yamlConfig = nullptr);

    // IService
    void start() override;
    void stop() override;
    void fillFeatures(aasdk::proto::messages::ServiceDiscoveryResponse& response) override;

    // Video focus control — callable by other plugins (e.g. PhonePlugin, reverse camera)
    void setVideoFocus(VideoFocusMode mode);

    /// Suppress phone-initiated focus requests (user exited to car).
    /// While suppressed, FOCUSED requests from phone are responded with UNFOCUSED.
    /// Cleared when the user explicitly re-enters AA via setVideoFocus(Projection).
    void setFocusSuppressed(bool suppressed);

    // IVideoServiceChannelEventHandler
    void onChannelOpenRequest(const aasdk::proto::messages::ChannelOpenRequest& request) override;
    void onAVChannelSetupRequest(const aasdk::proto::messages::AVChannelSetupRequest& request) override;
    void onAVChannelStartIndication(const aasdk::proto::messages::AVChannelStartIndication& indication) override;
    void onAVChannelStopIndication(const aasdk::proto::messages::AVChannelStopIndication& indication) override;
    void onAVMediaWithTimestampIndication(aasdk::messenger::Timestamp::ValueType timestamp,
                                          const aasdk::common::DataConstBuffer& buffer) override;
    void onAVMediaIndication(const aasdk::common::DataConstBuffer& buffer) override;
    void onVideoFocusRequest(const aasdk::proto::messages::VideoFocusRequest& request) override;
    void onChannelError(const aasdk::error::Error& e) override;

signals:
    void videoFrameData(QByteArray data, qint64 enqueueTimeNs);
    void videoFocusChanged(bool focused);

private:
    boost::asio::io_service::strand strand_;
    std::shared_ptr<aasdk::channel::av::VideoServiceChannel> channel_;
    std::shared_ptr<oap::Configuration> config_;
    oap::YamlConfig* yamlConfig_ = nullptr;
    VideoDecoder* decoder_;
    int32_t session_ = -1;
    VideoFocusMode currentFocusMode_ = VideoFocusMode::Projection;
    bool focusSuppressed_ = false;
};

} // namespace aa
} // namespace oap
