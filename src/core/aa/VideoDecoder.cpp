#include "VideoDecoder.hpp"

#include <QMetaObject>
#include <boost/log/trivial.hpp>
#include <cstring>
#include <iomanip>

namespace oap {
namespace aa {

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

    worker_ = new DecodeWorker(this);
    worker_->start();
    BOOST_LOG_TRIVIAL(info) << "[VideoDecoder] Decode worker thread started";
}

VideoDecoder::~VideoDecoder()
{
    cleanup();
}

void VideoDecoder::cleanup()
{
    if (worker_) {
        worker_->requestStop();
        worker_->wait();
        delete worker_;
        worker_ = nullptr;
    }

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

void VideoDecoder::decodeFrame(QByteArray h264Data, qint64 enqueueTimeNs)
{
    if (worker_)
        worker_->enqueue(std::move(h264Data), enqueueTimeNs);
}

void VideoDecoder::processFrame(const QByteArray& h264Data, qint64 enqueueTimeNs)
{
    if (!codecCtx_ || !parser_ || !packet_ || !frame_) return;

    auto t_decodeStart = PerfStats::Clock::now();

    // Android Auto sends H.264 in AnnexB format (start codes already present)
    const uint8_t* data = reinterpret_cast<const uint8_t*>(h264Data.constData());
    int dataSize = h264Data.size();

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

            auto t_decodeDone = PerfStats::Clock::now();

            // Got a decoded frame — convert to QVideoFrame
            if (videoSink_ && frame_->format == AV_PIX_FMT_YUV420P) {
                // Cache format on first frame or resolution change
                if (!cachedFormat_ ||
                    cachedFormat_->frameWidth() != frame_->width ||
                    cachedFormat_->frameHeight() != frame_->height) {
                    cachedFormat_ = QVideoFrameFormat(
                        QSize(frame_->width, frame_->height),
                        QVideoFrameFormat::Format_YUV420P);
                    frameBuffers_[0] = QVideoFrame();
                    frameBuffers_[1] = QVideoFrame();
                }

                // Double-buffered: alternate between two pre-allocated frames
                int bufIdx = currentBuffer_;
                currentBuffer_ = 1 - currentBuffer_;

                if (!frameBuffers_[bufIdx].isValid()) {
                    frameBuffers_[bufIdx] = QVideoFrame(*cachedFormat_);
                }

                QVideoFrame& videoFrame = frameBuffers_[bufIdx];
                if (videoFrame.map(QVideoFrame::WriteOnly)) {
                    const int h = frame_->height;
                    const int chromaH = h / 2;

                    // Y plane — bulk copy if strides match
                    const int yDstStride = videoFrame.bytesPerLine(0);
                    const int ySrcStride = frame_->linesize[0];
                    if (yDstStride == ySrcStride) {
                        std::memcpy(videoFrame.bits(0), frame_->data[0], ySrcStride * h);
                    } else {
                        const int yRowBytes = std::min(yDstStride, ySrcStride);
                        for (int y = 0; y < h; ++y)
                            std::memcpy(videoFrame.bits(0) + y * yDstStride,
                                        frame_->data[0] + y * ySrcStride, yRowBytes);
                    }

                    // U plane
                    const int uDstStride = videoFrame.bytesPerLine(1);
                    const int uSrcStride = frame_->linesize[1];
                    if (uDstStride == uSrcStride) {
                        std::memcpy(videoFrame.bits(1), frame_->data[1], uSrcStride * chromaH);
                    } else {
                        const int uRowBytes = std::min(uDstStride, uSrcStride);
                        for (int y = 0; y < chromaH; ++y)
                            std::memcpy(videoFrame.bits(1) + y * uDstStride,
                                        frame_->data[1] + y * uSrcStride, uRowBytes);
                    }

                    // V plane
                    const int vDstStride = videoFrame.bytesPerLine(2);
                    const int vSrcStride = frame_->linesize[2];
                    if (vDstStride == vSrcStride) {
                        std::memcpy(videoFrame.bits(2), frame_->data[2], vSrcStride * chromaH);
                    } else {
                        const int vRowBytes = std::min(vDstStride, vSrcStride);
                        for (int y = 0; y < chromaH; ++y)
                            std::memcpy(videoFrame.bits(2) + y * vDstStride,
                                        frame_->data[2] + y * vSrcStride, vRowBytes);
                    }

                    videoFrame.unmap();

                    auto t_copyDone = PerfStats::Clock::now();

                    // Marshal setVideoFrame back to Qt main thread
                    QVideoFrame frameCopy = videoFrame;
                    QMetaObject::invokeMethod(videoSink_, [this, frameCopy]() {
                        videoSink_->setVideoFrame(frameCopy);
                    }, Qt::QueuedConnection);

                    auto t_display = PerfStats::Clock::now();

                    // Record timing metrics
                    if (enqueueTimeNs > 0) {
                        auto t_enqueue = PerfStats::TimePoint(std::chrono::nanoseconds(enqueueTimeNs));
                        metricQueue_.record(PerfStats::msElapsed(t_enqueue, t_decodeStart));
                        metricTotal_.record(PerfStats::msElapsed(t_enqueue, t_display));
                    }
                    metricDecode_.record(PerfStats::msElapsed(t_decodeStart, t_decodeDone));
                    metricCopy_.record(PerfStats::msElapsed(t_decodeDone, t_copyDone));
                }

                ++frameCount_;
                ++framesSinceLog_;

                if (frameCount_ == 1) {
                    BOOST_LOG_TRIVIAL(info) << "[VideoDecoder] First frame decoded: "
                                            << frame_->width << "x" << frame_->height;
                }

                // Rolling stats every LOG_INTERVAL_SEC seconds
                auto now = PerfStats::Clock::now();
                double elapsed = PerfStats::msElapsed(lastLogTime_, now) / 1000.0;
                if (elapsed >= LOG_INTERVAL_SEC) {
                    double fps = framesSinceLog_ / elapsed;
                    BOOST_LOG_TRIVIAL(info)
                        << "[Perf] Video: queue=" << std::fixed << std::setprecision(1)
                        << metricQueue_.avg() << "ms"
                        << " decode=" << metricDecode_.avg() << "ms"
                        << " copy=" << metricCopy_.avg() << "ms"
                        << " total=" << metricTotal_.avg() << "ms"
                        << " (p99\u2248" << metricTotal_.max << "ms)"
                        << " | " << std::setprecision(1) << fps << " fps";

                    metricQueue_.reset();
                    metricDecode_.reset();
                    metricCopy_.reset();
                    metricTotal_.reset();
                    framesSinceLog_ = 0;
                    lastLogTime_ = now;
                }
            }

            av_frame_unref(frame_);
        }
    }
}

void VideoDecoder::DecodeWorker::enqueue(QByteArray data, qint64 enqueueTimeNs)
{
    QMutexLocker locker(&mutex_);
    queue_.push({std::move(data), enqueueTimeNs});
    condition_.wakeOne();
}

void VideoDecoder::DecodeWorker::requestStop()
{
    QMutexLocker locker(&mutex_);
    stopRequested_ = true;
    condition_.wakeOne();
}

void VideoDecoder::DecodeWorker::run()
{
    while (true) {
        WorkItem item;
        {
            QMutexLocker locker(&mutex_);
            while (queue_.empty() && !stopRequested_)
                condition_.wait(&mutex_);
            if (stopRequested_ && queue_.empty())
                return;
            item = std::move(queue_.front());
            queue_.pop();
        }
        decoder_->processFrame(item.data, item.enqueueTimeNs);
    }
}

} // namespace aa
} // namespace oap
