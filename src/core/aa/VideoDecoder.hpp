#pragma once

#include <QObject>
#include <QByteArray>
#include <QVideoSink>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <optional>

#include "PerfStats.hpp"

// FFmpeg C headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

namespace oap {
namespace aa {

class VideoDecoder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVideoSink* videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)

public:
    explicit VideoDecoder(QObject* parent = nullptr);
    ~VideoDecoder() override;

    QVideoSink* videoSink() const { return videoSink_; }
    void setVideoSink(QVideoSink* sink);

signals:
    void videoSinkChanged();

public slots:
    void decodeFrame(QByteArray h264Data, qint64 enqueueTimeNs = 0);

private:
    void cleanup();

    QVideoSink* videoSink_ = nullptr;

    // FFmpeg state
    const AVCodec* codec_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    AVCodecParserContext* parser_ = nullptr;
    AVPacket* packet_ = nullptr;
    AVFrame* frame_ = nullptr;

    uint64_t frameCount_ = 0;

    // Performance instrumentation
    PerfStats::Metric metricQueue_;    // signal emit → decode start
    PerfStats::Metric metricDecode_;   // decode start → avcodec_receive_frame
    PerfStats::Metric metricCopy_;     // receive_frame → copy done
    PerfStats::Metric metricTotal_;    // signal emit → setVideoFrame
    PerfStats::TimePoint lastLogTime_ = PerfStats::Clock::now();
    uint64_t framesSinceLog_ = 0;
    static constexpr double LOG_INTERVAL_SEC = 5.0;

    // Cached video frame format + double-buffered frames
    std::optional<QVideoFrameFormat> cachedFormat_;
    QVideoFrame frameBuffers_[2];
    int currentBuffer_ = 0;
};

} // namespace aa
} // namespace oap
