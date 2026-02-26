#include "VideoDecoder.hpp"
#include "../../core/YamlConfig.hpp"

#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
#include "DmaBufVideoBuffer.hpp"
#endif

#include <QMetaObject>
#include <QDebug>
#include <cstring>
#include <iomanip>

namespace oap {
namespace aa {

VideoDecoder::VideoDecoder(QObject* parent)
    : QObject(parent)
{
    packet_ = av_packet_alloc();
    frame_ = av_frame_alloc();

    if (!initCodec(AV_CODEC_ID_H264)) {
        qCritical() << "[VideoDecoder] Failed to initialize H.264 decoder";
        return;
    }

    worker_ = new DecodeWorker(this);
    worker_->start();
    qInfo() << "[VideoDecoder] Decode worker thread started";
}

bool VideoDecoder::isHardwareDecoder(const AVCodec* codec)
{
    if (!codec) return false;
    // AV_CODEC_CAP_HARDWARE is the official flag, but v4l2m2m often doesn't set it
    // (reports as "wrapper" instead). Use name-based detection as reliable fallback.
#ifdef AV_CODEC_CAP_HARDWARE
    if (codec->capabilities & AV_CODEC_CAP_HARDWARE)
        return true;
#endif
    const char* name = codec->name;
    return (strstr(name, "v4l2m2m") || strstr(name, "vaapi") ||
            strstr(name, "cuda") || strstr(name, "vdpau") ||
            strstr(name, "videotoolbox") || strstr(name, "qsv"));
}

bool VideoDecoder::tryOpenCodec(const AVCodec* codec, AVCodecID codecId)
{
    codecCtx_ = avcodec_alloc_context3(codec);
    if (!codecCtx_) return false;

    // Low-latency settings for real-time streaming
    codecCtx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codecCtx_->flags2 |= AV_CODEC_FLAG2_FAST;
    codecCtx_->thread_count = 1;  // Single-threaded for immediate output (no frame reordering delay)

    if (avcodec_open2(codecCtx_, codec, nullptr) < 0) {
        avcodec_free_context(&codecCtx_);
        return false;
    }

    parser_ = av_parser_init(codecId);
    if (!parser_) {
        avcodec_free_context(&codecCtx_);
        return false;
    }

    codec_ = codec;
    activeCodecId_ = codecId;
    return true;
}

bool VideoDecoder::initCodec(AVCodecID codecId)
{
    cleanupCodec();
    firstFrameDecoded_ = false;

    // Map codec IDs to display names, config keys, and hw decoder probe lists
    struct CodecInfo {
        const char* displayName;
        const char* configKey;
        const char* hwDecoders[4]; // null-terminated
    };
    static const QMap<AVCodecID, CodecInfo> codecInfoMap = {
        { AV_CODEC_ID_H264, { "H.264", "h264", {"h264_v4l2m2m", "h264_vaapi", nullptr} } },
        { AV_CODEC_ID_H265, { "H.265", "h265", {"hevc_v4l2m2m", "hevc_vaapi", nullptr} } },
        { AV_CODEC_ID_VP9,  { "VP9",   "vp9",  {"vp9_v4l2m2m", "vp9_vaapi", nullptr} } },
        { AV_CODEC_ID_AV1,  { "AV1",   "av1",  {"av1_v4l2m2m", "av1_vaapi", nullptr} } },
    };

    auto it = codecInfoMap.find(codecId);
    const char* codecName = it != codecInfoMap.end() ? it->displayName : "unknown";
    QString codecKey = it != codecInfoMap.end() ? it->configKey : "h264";
    QString decoderPref = yamlConfig_ ? yamlConfig_->videoDecoder(codecKey) : "auto";

    const AVCodec* selectedCodec = nullptr;

    // 1. Try user-specified decoder
    if (decoderPref != "auto") {
        selectedCodec = avcodec_find_decoder_by_name(decoderPref.toUtf8().constData());
        if (selectedCodec)
            qInfo() << "[VideoDecoder] Trying configured decoder:" << decoderPref;
        else
            qWarning() << "[VideoDecoder] Configured decoder" << decoderPref
                       << "not found, falling back to auto";
    }

    // 2. Auto mode: try known hw decoders in order
    if (!selectedCodec && it != codecInfoMap.end()) {
        for (int i = 0; it->hwDecoders[i]; ++i) {
            const AVCodec* hwCodec = avcodec_find_decoder_by_name(it->hwDecoders[i]);
            if (hwCodec) {
                qInfo() << "[VideoDecoder] Auto-detected hw decoder:" << it->hwDecoders[i];
                selectedCodec = hwCodec;
                break;
            }
        }
    }

    // 3. Fall back to default software decoder
    if (!selectedCodec) {
        selectedCodec = avcodec_find_decoder(codecId);
        if (!selectedCodec) {
            qCritical() << "[VideoDecoder] No" << codecName << "decoder found";
            return false;
        }
    }

    // Try to open the selected decoder
    if (!tryOpenCodec(selectedCodec, codecId)) {
        // If it was a non-software decoder, fall back to software
        const AVCodec* swCodec = avcodec_find_decoder(codecId);
        if (selectedCodec != swCodec) {
            qWarning() << "[VideoDecoder]" << selectedCodec->name
                       << "failed to open, falling back to software";
            if (!swCodec || !tryOpenCodec(swCodec, codecId)) {
                qCritical() << "[VideoDecoder] Software fallback also failed for" << codecName;
                return false;
            }
        } else {
            qCritical() << "[VideoDecoder] Failed to open" << codecName << "software decoder";
            return false;
        }
    }

    usingHardware_ = isHardwareDecoder(codec_);
    qInfo() << "[VideoDecoder] Using" << codec_->name
            << (usingHardware_ ? "(hardware)" : "(software)");
    return true;
}

void VideoDecoder::cleanupCodec()
{
    if (parser_) { av_parser_close(parser_); parser_ = nullptr; }
    if (codecCtx_) { avcodec_free_context(&codecCtx_); codecCtx_ = nullptr; }
    codec_ = nullptr;
}

AVCodecID VideoDecoder::detectCodec(const QByteArray& data) const
{
    // Find first AnnexB start code and check NAL unit type
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data.constData());
    int len = data.size();

    for (int i = 0; i < len - 4; ++i) {
        if (p[i] == 0 && p[i+1] == 0 && p[i+2] == 0 && p[i+3] == 1) {
            uint8_t nalByte = p[i+4];
            // H.264: NAL type = byte & 0x1F (SPS=7, PPS=8, IDR=5)
            // H.265: NAL type = (byte >> 1) & 0x3F (VPS=32, SPS=33, PPS=34)
            int h264Type = nalByte & 0x1F;
            int h265Type = (nalByte >> 1) & 0x3F;

            if (h265Type >= 32 && h265Type <= 34) {
                return AV_CODEC_ID_H265;
            }
            if (h264Type == 7 || h264Type == 8 || h264Type == 5 || h264Type == 1) {
                return AV_CODEC_ID_H264;
            }
            // Ambiguous — check forbidden_zero_bit for H.265
            // H.265 NAL header: forbidden(1) + type(6) + layer_id(6) + tid(3) = 2 bytes
            // H.264 NAL header: forbidden(1) + nal_ref_idc(2) + type(5) = 1 byte
            // H.265 forbidden bit is always 0, same as H.264
            // If type > 23 in H.264 space, it's likely H.265
            if (h264Type > 23) {
                return AV_CODEC_ID_H265;
            }
        }
    }
    return AV_CODEC_ID_H264; // default
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
    cleanupCodec();
}

void VideoDecoder::setVideoSink(QVideoSink* sink)
{
    // Atomically swap so the decode worker thread always sees a consistent pointer.
    // fetchAndStoreRelease pairs with loadAcquire in processFrame().
    QVideoSink* old = videoSink_.fetchAndStoreRelease(sink);
    if (old != sink) {
        // Invalidate the liveness guard BEFORE updating the pointer.
        // Any lambda already in the main thread's event queue that captured
        // the old sinkValid_ shared_ptr will see false and skip setVideoFrame(),
        // so the old QVideoSink can be safely deleted immediately after this call.
        sinkValid_->store(false);
        if (sink) {
            // Fresh guard for the new sink — lambdas dispatched after this point
            // get a new shared_ptr that is true until the next setVideoSink call.
            sinkValid_ = std::make_shared<std::atomic<bool>>(true);
        }
        emit videoSinkChanged();
        qInfo() << "[VideoDecoder] Video sink "
                                << (sink ? "connected" : "disconnected");
    }
}

void VideoDecoder::decodeFrame(std::shared_ptr<const QByteArray> h264Data, qint64 enqueueTimeNs)
{
    if (worker_)
        worker_->enqueue(std::move(h264Data), enqueueTimeNs);
}

void VideoDecoder::processFrame(const QByteArray& h264Data, qint64 enqueueTimeNs)
{
    if (!codecCtx_ || !parser_ || !packet_ || !frame_) return;

    // Auto-detect codec from first packet's NAL headers (run exactly once)
    if (!codecDetected_) {
        codecDetected_ = true;
        QByteArray prefix = h264Data.left(16);
        qInfo() << "[VideoDecoder] First packet:" << prefix.size() << "bytes, hex:"
                << prefix.toHex(' ');
        AVCodecID detected = detectCodec(h264Data);
        if (detected != activeCodecId_) {
            const char* name = (detected == AV_CODEC_ID_H265) ? "H.265" : "H.264";
            qInfo() << "[VideoDecoder] Phone is sending" << name << "— switching decoder";
            if (!initCodec(detected)) {
                qCritical() << "[VideoDecoder] Failed to switch to" << name;
                return;
            }
        } else {
            qInfo() << "[VideoDecoder] Phone is sending"
                    << ((detected == AV_CODEC_ID_H265) ? "H.265" : "H.264")
                    << "(matches current decoder)";
        }
        // Update the decode worker's codec hint for keyframe detection
        if (worker_)
            worker_->setCodecIsH265(activeCodecId_ == AV_CODEC_ID_H265);
    }

    auto t_decodeStart = PerfStats::Clock::now();

    // Android Auto sends video in AnnexB format (start codes already present)
    const uint8_t* data = reinterpret_cast<const uint8_t*>(h264Data.constData());
    int dataSize = h264Data.size();

    while (dataSize > 0) {
        int consumed = av_parser_parse2(
            parser_, codecCtx_,
            &packet_->data, &packet_->size,
            data, dataSize,
            AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

        if (consumed < 0) {
            qCritical() << "[VideoDecoder] Parse error";
            break;
        }

        data += consumed;
        dataSize -= consumed;

        if (packet_->size == 0)
            continue;

        // Send packet to decoder
        int ret = avcodec_send_packet(codecCtx_, packet_);
        if (ret < 0) {
            qWarning() << "[VideoDecoder] Send packet error: " << ret;
            continue;
        }

        // Receive decoded frames
        while (true) {
            ret = avcodec_receive_frame(codecCtx_, frame_);

            // First-frame hw fallback: if hw decoder can't produce a frame,
            // reinitialize with software and re-send the packet
            if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF
                && usingHardware_ && !firstFrameDecoded_) {
                qWarning() << "[VideoDecoder] HW decoder failed on first frame (err="
                           << ret << "), falling back to software";
                const AVCodec* swCodec = avcodec_find_decoder(activeCodecId_);
                AVCodecID savedId = activeCodecId_;
                cleanupCodec();
                if (swCodec && tryOpenCodec(swCodec, savedId)) {
                    usingHardware_ = false;
                    qInfo() << "[VideoDecoder] Switched to" << swCodec->name << "(software)";
                    // Re-send the current packet to the new decoder
                    avcodec_send_packet(codecCtx_, packet_);
                    ret = avcodec_receive_frame(codecCtx_, frame_);
                } else {
                    qCritical() << "[VideoDecoder] Software fallback failed during first-frame recovery";
                    break;
                }
            }

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0) {
                qWarning() << "[VideoDecoder] Receive frame error: " << ret;
                break;
            }

            firstFrameDecoded_ = true;

            auto t_decodeDone = PerfStats::Clock::now();

            // Got a decoded frame — convert to QVideoFrame
            // Accept both YUV420P (limited range) and YUVJ420P (full/JPEG range) —
            // same pixel layout, different color range convention
            // Load the sink pointer once atomically. The lambda captures this local
            // value so it doesn't re-read videoSink_ when it fires on the main thread
            // (by which time the sink may have been nulled by setVideoSink(nullptr)).
            // Qt's QueuedConnection dispatch also uses `sink` as the context object,
            // so if QVideoSink is destroyed before the lambda runs, Qt discards the event.
            QVideoSink* sink = videoSink_.loadAcquire();
#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
            if (sink && frame_->format == AV_PIX_FMT_DRM_PRIME) {
                // Zero-copy hardware path: wrap DRM_PRIME frame in QAbstractVideoBuffer.
                // Qt 6.8 takes ownership via unique_ptr; format() is provided by the buffer.
                auto buffer = std::make_unique<DmaBufVideoBuffer>(
                    frame_, frame_->width, frame_->height);
                QVideoFrame videoFrame(std::move(buffer));

                auto t_copyDone = PerfStats::Clock::now();

                auto guard = sinkValid_;
                QMetaObject::invokeMethod(sink, [sink, videoFrame, guard]() {
                    if (guard->load())
                        sink->setVideoFrame(videoFrame);
                }, Qt::QueuedConnection);

                auto t_display = PerfStats::Clock::now();

                // Record timing metrics
                if (enqueueTimeNs > 0) {
                    auto t_enqueue = PerfStats::TimePoint(std::chrono::nanoseconds(enqueueTimeNs));
                    metricQueue_.record(PerfStats::msElapsed(t_enqueue, t_decodeStart));
                    metricTotal_.record(PerfStats::msElapsed(t_enqueue, t_display));
                }
                metricDecode_.record(PerfStats::msElapsed(t_decodeStart, t_decodeDone));
                metricCopy_.record(0.0);  // zero-copy
            }
            else
#endif
            if (sink && (frame_->format == AV_PIX_FMT_YUV420P ||
                               frame_->format == AV_PIX_FMT_YUVJ420P)) {
                // Create or reset frame pool on first frame or resolution change
                if (!framePool_ ||
                    framePool_->format().frameWidth() != frame_->width ||
                    framePool_->format().frameHeight() != frame_->height) {
                    QVideoFrameFormat fmt(
                        QSize(frame_->width, frame_->height),
                        QVideoFrameFormat::Format_YUV420P);
                    if (framePool_) {
                        framePool_->reset(fmt);
                    } else {
                        framePool_ = std::make_unique<VideoFramePool>(fmt, 5);
                    }
                }

                // Fresh frame each decode — QVideoFrame is ref-counted so reusing
                // buffers races with Qt's render thread holding a read mapping.
                // The pool encapsulates format caching and allocation tracking.
                QVideoFrame videoFrame = framePool_->acquire();
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

                    // Marshal setVideoFrame back to Qt main thread.
                    // Capture `sink` and `guard` by value. If setVideoSink(nullptr)
                    // is called before this lambda runs, guard will be false and we
                    // skip the call — the QVideoSink may already be deleted by then.
                    auto guard = sinkValid_;
                    QMetaObject::invokeMethod(sink, [sink, videoFrame, guard]() {
                        if (guard->load())
                            sink->setVideoFrame(videoFrame);
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
                    qInfo() << "[VideoDecoder] First frame decoded: "
                                            << frame_->width << "x" << frame_->height;
                }

                // Rolling stats every LOG_INTERVAL_SEC seconds
                auto now = PerfStats::Clock::now();
                double elapsed = PerfStats::msElapsed(lastLogTime_, now) / 1000.0;
                if (elapsed >= LOG_INTERVAL_SEC) {
                    double fps = framesSinceLog_ / elapsed;
                    qInfo() << "[Perf] Video: queue="
                        << QString::number(metricQueue_.avg(), 'f', 1) << "ms"
                        << "decode=" << QString::number(metricDecode_.avg(), 'f', 1) << "ms"
                        << "copy=" << QString::number(metricCopy_.avg(), 'f', 1) << "ms"
                        << "total=" << QString::number(metricTotal_.avg(), 'f', 1) << "ms"
                        << "(p99\u2248" << QString::number(metricTotal_.max, 'f', 1) << "ms)"
                        << "|" << QString::number(fps, 'f', 1) << "fps";

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

// Detect keyframes in H.264 or H.265 AnnexB bitstreams.
// When codecIsH265 is unknown (first frame), checks both — this is safe because
// the first frame from AA is always SPS+PPS+IDR which is unambiguous in both codecs.
static bool isKeyframe(const QByteArray& data, bool codecIsH265)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data.constData());
    int len = data.size();
    for (int i = 0; i + 3 <= len; ++i) {
        if (p[i] == 0 && p[i+1] == 0) {
            int nalStart = -1;
            if (p[i+2] == 1) {
                nalStart = i + 3;
            } else if (p[i+2] == 0 && i + 3 < len && p[i+3] == 1) {
                nalStart = i + 4;
            }
            if (nalStart >= 0 && nalStart < len) {
                uint8_t firstByte = p[nalStart];
                if (codecIsH265) {
                    // H.265: forbidden_zero(1) + type(6) + layer_id(6) + tid(3)
                    // IDR_W_RADL=19, IDR_N_LP=20, VPS=32, SPS=33, PPS=34
                    uint8_t h265Type = (firstByte >> 1) & 0x3F;
                    if (h265Type == 19 || h265Type == 20 || h265Type == 32 || h265Type == 33 || h265Type == 34)
                        return true;
                } else {
                    // H.264: forbidden_zero(1) + nal_ref_idc(2) + type(5)
                    // IDR=5, SPS=7, PPS=8
                    uint8_t h264Type = firstByte & 0x1F;
                    if (h264Type == 5 || h264Type == 7 || h264Type == 8)
                        return true;
                }
                // Found a NAL but it's not a keyframe type — continue scanning for more NALs
            }
        }
    }
    return false;
}

void VideoDecoder::DecodeWorker::enqueue(std::shared_ptr<const QByteArray> data, qint64 enqueueTimeNs)
{
    QMutexLocker locker(&mutex_);

    bool keyframe = isKeyframe(*data, codecIsH265_);

    // If awaiting keyframe after a forced drop, discard non-keyframes
    if (awaitingKeyframe_) {
        if (!keyframe) {
            ++droppedFrames_;
            return;  // Don't wake — nothing to process
        }
        // Got our keyframe — resume normal decode
        awaitingKeyframe_ = false;
        needsFlush_ = true;
        qDebug() << "[VideoDecoder] Keyframe received, resuming decode (dropped" << droppedFrames_ << "frames)";
    }

    // Bound queue size
    while (static_cast<int>(queue_.size()) >= MAX_QUEUE_SIZE) {
        auto& front = queue_.front();
        if (front.isKeyframe && queue_.size() == 1) {
            // Only a keyframe left and we need to drop it — enter awaiting mode
            awaitingKeyframe_ = true;
            needsFlush_ = true;
            qWarning() << "[VideoDecoder] Dropped keyframe, awaiting next IDR";
        }
        queue_.pop();
        ++droppedFrames_;
    }

    queue_.push({std::move(data), enqueueTimeNs, keyframe});
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
        bool flush = false;
        {
            QMutexLocker locker(&mutex_);
            while (queue_.empty() && !stopRequested_)
                condition_.wait(&mutex_);
            if (stopRequested_ && queue_.empty())
                return;
            item = std::move(queue_.front());
            queue_.pop();
            flush = needsFlush_;
            needsFlush_ = false;
        }
        if (flush && decoder_->codecCtx_)
            avcodec_flush_buffers(decoder_->codecCtx_);
        decoder_->processFrame(*item.data, item.enqueueTimeNs);
    }
}

} // namespace aa
} // namespace oap
