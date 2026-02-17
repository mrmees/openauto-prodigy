#include "VideoDecoder.hpp"

#include <boost/log/trivial.hpp>
#include <cstring>

namespace oap {
namespace aa {

// AnnexB start code prepended to each NAL unit from aasdk
static const uint8_t kStartCode[] = {0x00, 0x00, 0x00, 0x01};

VideoDecoder::VideoDecoder(QObject* parent)
    : QObject(parent)
{
    codec_ = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec_) {
        BOOST_LOG_TRIVIAL(error) << "[VideoDecoder] H.264 decoder not found";
        return;
    }

    codecCtx_ = avcodec_alloc_context3(codec_);
    if (!codecCtx_) {
        BOOST_LOG_TRIVIAL(error) << "[VideoDecoder] Failed to allocate codec context";
        return;
    }

    // Low-latency settings for real-time streaming
    codecCtx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codecCtx_->flags2 |= AV_CODEC_FLAG2_FAST;
    codecCtx_->thread_count = 2;

    if (avcodec_open2(codecCtx_, codec_, nullptr) < 0) {
        BOOST_LOG_TRIVIAL(error) << "[VideoDecoder] Failed to open codec";
        avcodec_free_context(&codecCtx_);
        return;
    }

    parser_ = av_parser_init(AV_CODEC_ID_H264);
    if (!parser_) {
        BOOST_LOG_TRIVIAL(error) << "[VideoDecoder] Failed to init parser";
        avcodec_free_context(&codecCtx_);
        return;
    }

    packet_ = av_packet_alloc();
    frame_ = av_frame_alloc();

    BOOST_LOG_TRIVIAL(info) << "[VideoDecoder] Initialized (H.264 software decode)";
}

VideoDecoder::~VideoDecoder()
{
    cleanup();
}

void VideoDecoder::cleanup()
{
    if (frame_) { av_frame_free(&frame_); frame_ = nullptr; }
    if (packet_) { av_packet_free(&packet_); packet_ = nullptr; }
    if (parser_) { av_parser_close(parser_); parser_ = nullptr; }
    if (codecCtx_) { avcodec_free_context(&codecCtx_); codecCtx_ = nullptr; }
}

void VideoDecoder::setVideoSink(QVideoSink* sink)
{
    if (videoSink_ != sink) {
        videoSink_ = sink;
        emit videoSinkChanged();
        BOOST_LOG_TRIVIAL(info) << "[VideoDecoder] Video sink "
                                << (sink ? "connected" : "disconnected");
    }
}

void VideoDecoder::decodeFrame(QByteArray h264Data)
{
    if (!codecCtx_ || !parser_ || !packet_ || !frame_) return;

    // Prepend AnnexB start code — aasdk delivers raw NAL units without it
    QByteArray annexBData;
    annexBData.reserve(4 + h264Data.size());
    annexBData.append(reinterpret_cast<const char*>(kStartCode), 4);
    annexBData.append(h264Data);

    const uint8_t* data = reinterpret_cast<const uint8_t*>(annexBData.constData());
    int dataSize = annexBData.size();

    while (dataSize > 0) {
        int consumed = av_parser_parse2(
            parser_, codecCtx_,
            &packet_->data, &packet_->size,
            data, dataSize,
            AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

        if (consumed < 0) {
            BOOST_LOG_TRIVIAL(error) << "[VideoDecoder] Parse error";
            break;
        }

        data += consumed;
        dataSize -= consumed;

        if (packet_->size == 0) continue;

        // Send packet to decoder
        int ret = avcodec_send_packet(codecCtx_, packet_);
        if (ret < 0) {
            if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                BOOST_LOG_TRIVIAL(warning) << "[VideoDecoder] Send packet error: " << ret;
            }
            continue;
        }

        // Receive decoded frames
        while (true) {
            ret = avcodec_receive_frame(codecCtx_, frame_);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) {
                BOOST_LOG_TRIVIAL(warning) << "[VideoDecoder] Receive frame error: " << ret;
                break;
            }

            // Got a decoded frame — convert to QVideoFrame
            if (videoSink_ && frame_->format == AV_PIX_FMT_YUV420P) {
                QVideoFrameFormat format(
                    QSize(frame_->width, frame_->height),
                    QVideoFrameFormat::Format_YUV420P);

                QVideoFrame videoFrame(format);
                if (videoFrame.map(QVideoFrame::WriteOnly)) {
                    // Copy Y plane
                    const int yStride = videoFrame.bytesPerLine(0);
                    const uint8_t* ySrc = frame_->data[0];
                    uint8_t* yDst = videoFrame.bits(0);
                    for (int y = 0; y < frame_->height; ++y) {
                        std::memcpy(yDst + y * yStride, ySrc + y * frame_->linesize[0],
                                    std::min(yStride, frame_->linesize[0]));
                    }

                    // Copy U plane
                    const int uStride = videoFrame.bytesPerLine(1);
                    const uint8_t* uSrc = frame_->data[1];
                    uint8_t* uDst = videoFrame.bits(1);
                    const int chromaHeight = frame_->height / 2;
                    for (int y = 0; y < chromaHeight; ++y) {
                        std::memcpy(uDst + y * uStride, uSrc + y * frame_->linesize[1],
                                    std::min(uStride, frame_->linesize[1]));
                    }

                    // Copy V plane
                    const int vStride = videoFrame.bytesPerLine(2);
                    const uint8_t* vSrc = frame_->data[2];
                    uint8_t* vDst = videoFrame.bits(2);
                    for (int y = 0; y < chromaHeight; ++y) {
                        std::memcpy(vDst + y * vStride, vSrc + y * frame_->linesize[2],
                                    std::min(vStride, frame_->linesize[2]));
                    }

                    videoFrame.unmap();
                    videoSink_->setVideoFrame(videoFrame);
                }

                ++frameCount_;
                if (frameCount_ == 1) {
                    BOOST_LOG_TRIVIAL(info) << "[VideoDecoder] First frame decoded: "
                                            << frame_->width << "x" << frame_->height;
                } else if (frameCount_ % 300 == 0) {
                    BOOST_LOG_TRIVIAL(info) << "[VideoDecoder] Frames decoded: " << frameCount_;
                }
            }

            av_frame_unref(frame_);
        }
    }
}

} // namespace aa
} // namespace oap
