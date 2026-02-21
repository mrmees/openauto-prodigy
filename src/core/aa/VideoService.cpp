#include "VideoService.hpp"
#include "../../core/YamlConfig.hpp"

#include <chrono>
#include <boost/log/trivial.hpp>

#include <aasdk/Messenger/ChannelId.hpp>
#include <aasdk/Channel/Promise.hpp>
#include <aasdk_proto/ChannelOpenResponseMessage.pb.h>
#include <aasdk_proto/AVChannelSetupResponseMessage.pb.h>
#include <aasdk_proto/AVChannelSetupStatusEnum.pb.h>
#include <aasdk_proto/StatusEnum.pb.h>
#include <aasdk_proto/VideoFocusIndicationMessage.pb.h>
#include <aasdk_proto/VideoFocusModeEnum.pb.h>
#include <aasdk_proto/AVStreamTypeEnum.pb.h>
#include <aasdk_proto/VideoResolutionEnum.pb.h>
#include <aasdk_proto/VideoFPSEnum.pb.h>

namespace oap {
namespace aa {

VideoService::VideoService(
    boost::asio::io_service& ioService,
    aasdk::messenger::IMessenger::Pointer messenger,
    std::shared_ptr<oap::Configuration> config,
    VideoDecoder* decoder,
    oap::YamlConfig* yamlConfig)
    : strand_(ioService)
    , channel_(std::make_shared<aasdk::channel::av::VideoServiceChannel>(strand_, std::move(messenger)))
    , config_(std::move(config))
    , yamlConfig_(yamlConfig)
    , decoder_(decoder)
{
    // Connect signal→slot across threads (ASIO thread → Qt main thread)
    if (decoder_) {
        connect(this, &VideoService::videoFrameData,
                decoder_, &VideoDecoder::decodeFrame,
                Qt::DirectConnection);
    }
}

void VideoService::start()
{
    strand_.dispatch([this, self = shared_from_this()]() {
        BOOST_LOG_TRIVIAL(info) << "[VideoService] Started";
        channel_->receive(shared_from_this());
    });
}

void VideoService::stop()
{
    BOOST_LOG_TRIVIAL(info) << "[VideoService] Stopped";
}

void VideoService::fillFeatures(aasdk::proto::messages::ServiceDiscoveryResponse& response)
{
    auto* desc = response.add_channels();
    desc->set_channel_id(static_cast<uint32_t>(aasdk::messenger::ChannelId::VIDEO));

    auto* avChannel = desc->mutable_av_channel();
    avChannel->set_stream_type(aasdk::proto::enums::AVStreamType::VIDEO);
    avChannel->set_available_while_in_call(true);

    // Resolve preferred resolution from YAML config (fall back to legacy config)
    QString res = yamlConfig_ ? yamlConfig_->videoResolution() : QStringLiteral("720p");
    int dpi = yamlConfig_ ? yamlConfig_->videoDpi() : config_->screenDpi();
    int fps = yamlConfig_ ? yamlConfig_->videoFps() : config_->videoFps();
    auto fpsEnum = (fps == 60) ? aasdk::proto::enums::VideoFPS::_60
                               : aasdk::proto::enums::VideoFPS::_30;

    // Primary video config — preferred resolution
    auto* primaryConfig = avChannel->add_video_configs();
    if (res == "1080p")
        primaryConfig->set_video_resolution(aasdk::proto::enums::VideoResolution::_1080p);
    else if (res == "480p")
        primaryConfig->set_video_resolution(aasdk::proto::enums::VideoResolution::_480p);
    else
        primaryConfig->set_video_resolution(aasdk::proto::enums::VideoResolution::_720p);
    primaryConfig->set_video_fps(fpsEnum);
    primaryConfig->set_margin_width(0);
    primaryConfig->set_margin_height(0);
    primaryConfig->set_dpi(dpi);

    // Mandatory 480p fallback — production AA SDKs always include this
    if (res != "480p") {
        auto* fallbackConfig = avChannel->add_video_configs();
        fallbackConfig->set_video_resolution(aasdk::proto::enums::VideoResolution::_480p);
        fallbackConfig->set_video_fps(aasdk::proto::enums::VideoFPS::_30);
        fallbackConfig->set_margin_width(0);
        fallbackConfig->set_margin_height(0);
        fallbackConfig->set_dpi(dpi);
    }

    BOOST_LOG_TRIVIAL(info) << "[VideoService] Advertised video: " << res.toStdString()
                            << " @ " << fps << "fps, " << dpi << "dpi"
                            << (res != "480p" ? " + 480p fallback" : "");
}

void VideoService::onChannelOpenRequest(const aasdk::proto::messages::ChannelOpenRequest& request)
{
    BOOST_LOG_TRIVIAL(info) << "[VideoService] Channel open request";
    aasdk::proto::messages::ChannelOpenResponse response;
    response.set_status(aasdk::proto::enums::Status::OK);

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([]() {}, [](const aasdk::error::Error& e) {
        BOOST_LOG_TRIVIAL(error) << "[VideoService] Send error: " << e.what();
    });
    channel_->sendChannelOpenResponse(response, std::move(promise));
    channel_->receive(shared_from_this());
}

void VideoService::onAVChannelSetupRequest(const aasdk::proto::messages::AVChannelSetupRequest& request)
{
    BOOST_LOG_TRIVIAL(info) << "[VideoService] AV setup request (config index: "
                            << request.config_index() << ")";

    aasdk::proto::messages::AVChannelSetupResponse response;
    response.set_media_status(aasdk::proto::enums::AVChannelSetupStatus::OK);
    response.set_max_unacked(10);
    response.add_configs(0);

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([this, self = shared_from_this()]() {
        // Send video focus indication — tells the phone to start sending video
        aasdk::proto::messages::VideoFocusIndication indication;
        indication.set_focus_mode(aasdk::proto::enums::VideoFocusMode::FOCUSED);
        indication.set_unrequested(false);

        auto p = aasdk::channel::SendPromise::defer(strand_);
        p->then([]() {
            BOOST_LOG_TRIVIAL(info) << "[VideoService] Video focus indication sent";
        }, [](const aasdk::error::Error& e) {
            BOOST_LOG_TRIVIAL(error) << "[VideoService] Focus send error: " << e.what();
        });
        channel_->sendVideoFocusIndication(indication, std::move(p));
    }, [](const aasdk::error::Error& e) {
        BOOST_LOG_TRIVIAL(error) << "[VideoService] Setup send error: " << e.what();
    });
    channel_->sendAVChannelSetupResponse(response, std::move(promise));
    channel_->receive(shared_from_this());
}

void VideoService::onAVChannelStartIndication(const aasdk::proto::messages::AVChannelStartIndication& indication)
{
    BOOST_LOG_TRIVIAL(info) << "[VideoService] AV channel start (session=" << indication.session() << ")";
    session_ = indication.session();
    channel_->receive(shared_from_this());
}

void VideoService::onAVChannelStopIndication(const aasdk::proto::messages::AVChannelStopIndication& indication)
{
    BOOST_LOG_TRIVIAL(info) << "[VideoService] AV channel stop";
    channel_->receive(shared_from_this());
}

void VideoService::onAVMediaWithTimestampIndication(
    aasdk::messenger::Timestamp::ValueType timestamp,
    const aasdk::common::DataConstBuffer& buffer)
{
    // ACK the frame (flow control — phone won't send next frame without this)
    aasdk::proto::messages::AVMediaAckIndication ack;
    ack.set_session(session_);
    ack.set_value(1);

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([]() {}, [](const aasdk::error::Error& e) {});
    channel_->sendAVMediaAckIndication(ack, std::move(promise));

    // Marshal H.264 data to Qt thread for decoding
    // QByteArray performs a deep copy here which is fine — the ASIO buffer
    // is only valid for the duration of this callback
    emit videoFrameData(QByteArray(reinterpret_cast<const char*>(buffer.cdata), buffer.size),
                        std::chrono::steady_clock::now().time_since_epoch().count());

    channel_->receive(shared_from_this());
}

void VideoService::onAVMediaIndication(const aasdk::common::DataConstBuffer& buffer)
{
    // SPS/PPS codec configuration data arrives here (no timestamp)
    // Must forward to decoder or it will never be able to decode frames
    emit videoFrameData(QByteArray(reinterpret_cast<const char*>(buffer.cdata), buffer.size),
                        std::chrono::steady_clock::now().time_since_epoch().count());
    channel_->receive(shared_from_this());
}

void VideoService::onVideoFocusRequest(const aasdk::proto::messages::VideoFocusRequest& request)
{
    bool phoneFocused = (request.focus_mode() == aasdk::proto::enums::VideoFocusMode::FOCUSED);

    // When user has exited to car, reject the phone's FOCUSED requests.
    // The phone will keep trying — always respond UNFOCUSED until the user
    // explicitly re-enters AA (which clears focusSuppressed_ via setVideoFocus).
    if (phoneFocused && focusSuppressed_) {
        BOOST_LOG_TRIVIAL(info) << "[VideoService] Video focus request (mode=FOCUSED) — "
                                   "suppressed (user exited to car)";

        aasdk::proto::messages::VideoFocusIndication indication;
        indication.set_focus_mode(aasdk::proto::enums::VideoFocusMode::UNFOCUSED);
        indication.set_unrequested(false);

        auto promise = aasdk::channel::SendPromise::defer(strand_);
        promise->then([]() {}, [](const aasdk::error::Error& e) {
            BOOST_LOG_TRIVIAL(error) << "[VideoService] VideoFocusIndication send error: " << e.what();
        });
        channel_->sendVideoFocusIndication(indication, std::move(promise));
        channel_->receive(shared_from_this());
        return;
    }

    BOOST_LOG_TRIVIAL(info) << "[VideoService] Video focus request (mode="
                            << request.focus_mode() << ")";

    VideoFocusMode oldMode = currentFocusMode_;
    currentFocusMode_ = phoneFocused ? VideoFocusMode::Projection : VideoFocusMode::Native;

    // Phone requests UNFOCUSED = user hit exit — suppress future FOCUSED requests
    if (!phoneFocused) {
        focusSuppressed_ = true;
    }

    auto protoMode = phoneFocused
        ? aasdk::proto::enums::VideoFocusMode::FOCUSED
        : aasdk::proto::enums::VideoFocusMode::UNFOCUSED;

    aasdk::proto::messages::VideoFocusIndication indication;
    indication.set_focus_mode(protoMode);
    indication.set_unrequested(false);

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([this, self = shared_from_this(), phoneFocused, oldMode]() {
        bool wasFocused = (oldMode == VideoFocusMode::Projection || oldMode == VideoFocusMode::NativeTransient);
        if (phoneFocused != wasFocused) {
            BOOST_LOG_TRIVIAL(info) << "[VideoService] Focus changed: "
                                    << (phoneFocused ? "FOCUSED" : "UNFOCUSED");
            emit videoFocusChanged(phoneFocused);
        }
    }, [](const aasdk::error::Error& e) {
        BOOST_LOG_TRIVIAL(error) << "[VideoService] VideoFocusIndication send error: " << e.what();
    });
    channel_->sendVideoFocusIndication(indication, std::move(promise));
    channel_->receive(shared_from_this());
}

void VideoService::setFocusSuppressed(bool suppressed)
{
    focusSuppressed_ = suppressed;
    BOOST_LOG_TRIVIAL(info) << "[VideoService] Focus suppression "
                            << (suppressed ? "enabled" : "disabled");
}

void VideoService::setVideoFocus(VideoFocusMode mode)
{
    // User-initiated focus change clears suppression
    focusSuppressed_ = false;
    currentFocusMode_ = mode;

    auto protoMode = (mode == VideoFocusMode::Native)
        ? aasdk::proto::enums::VideoFocusMode::UNFOCUSED
        : aasdk::proto::enums::VideoFocusMode::FOCUSED;

    BOOST_LOG_TRIVIAL(info) << "[VideoService] Setting video focus: "
        << (mode == VideoFocusMode::Projection ? "Projection" :
            mode == VideoFocusMode::Native ? "Native" : "NativeTransient");

    aasdk::proto::messages::VideoFocusIndication indication;
    indication.set_focus_mode(protoMode);
    indication.set_unrequested(true);  // HU-initiated, not responding to phone request

    auto promise = aasdk::channel::SendPromise::defer(strand_);
    promise->then([]() {}, [](const aasdk::error::Error& e) {
        BOOST_LOG_TRIVIAL(error) << "[VideoService] setVideoFocus send error: " << e.what();
    });
    channel_->sendVideoFocusIndication(indication, std::move(promise));
}

void VideoService::onChannelError(const aasdk::error::Error& e)
{
    BOOST_LOG_TRIVIAL(error) << "[VideoService] Channel error: " << e.what();
}

} // namespace aa
} // namespace oap
