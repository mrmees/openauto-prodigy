#pragma once

#include <QObject>
#include <QByteArray>
#include <QVideoSink>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicPointer>
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>

#include "PerfStats.hpp"
#include "VideoFramePool.hpp"

// FFmpeg C headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

namespace oap { class YamlConfig; }

namespace oap {
namespace aa {

class VideoDecoder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVideoSink* videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)

public:
    explicit VideoDecoder(QObject* parent = nullptr);
    ~VideoDecoder() override;

    QVideoSink* videoSink() const { return videoSink_.loadRelaxed(); }
    void setVideoSink(QVideoSink* sink);
    void setYamlConfig(oap::YamlConfig* config) { yamlConfig_ = config; }

    /// Returns the latest decoded frame if available, otherwise invalid QVideoFrame
    QVideoFrame takeLatestFrame();

signals:
    void videoSinkChanged();

public slots:
    void decodeFrame(std::shared_ptr<const QByteArray> h264Data, qint64 enqueueTimeNs = 0);

private:
    // Decode worker thread
    class DecodeWorker : public QThread {
    public:
        explicit DecodeWorker(VideoDecoder* decoder) : decoder_(decoder) {}
        void run() override;
        void enqueue(std::shared_ptr<const QByteArray> data, qint64 enqueueTimeNs);
        void requestStop();
        void setCodecIsH265(bool h265) { QMutexLocker lock(&mutex_); codecIsH265_ = h265; }
        uint32_t droppedFrames() const { QMutexLocker lock(&mutex_); return droppedFrames_; }
    private:
        VideoDecoder* decoder_;
        mutable QMutex mutex_;
        QWaitCondition condition_;
        struct WorkItem { std::shared_ptr<const QByteArray> data; qint64 enqueueTimeNs; bool isKeyframe; };
        std::queue<WorkItem> queue_;
        bool stopRequested_ = false;
        static constexpr int MAX_QUEUE_SIZE = 2;
        bool awaitingKeyframe_ = false;
        bool needsFlush_ = false;
        uint32_t droppedFrames_ = 0;
        bool codecIsH265_ = false;  // Set by decoder after codec detection
    };

    DecodeWorker* worker_ = nullptr;
    void processFrame(const QByteArray& h264Data, qint64 enqueueTimeNs);

    void cleanup();

    QAtomicPointer<QVideoSink> videoSink_;
    // Shared across threads: set to false in setVideoSink(nullptr) so any
    // already-queued invokeMethod lambdas skip the setVideoFrame() call.
    std::shared_ptr<std::atomic<bool>> sinkValid_ = std::make_shared<std::atomic<bool>>(false);

    // Latest-frame-wins slot — decode thread writes, display timer reads
    std::mutex latestFrameMutex_;
    QVideoFrame latestFrame_;
    std::atomic<bool> hasLatestFrame_{false};

    // FFmpeg state
    const AVCodec* codec_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    AVCodecParserContext* parser_ = nullptr;
    AVPacket* packet_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVCodecID activeCodecId_ = AV_CODEC_ID_H264;
    bool codecDetected_ = false;
    bool usingHardware_ = false;
    bool firstFrameDecoded_ = false;
    oap::YamlConfig* yamlConfig_ = nullptr;

    bool initCodec(AVCodecID codecId);
    bool tryOpenCodec(const AVCodec* codec, AVCodecID codecId);
    void cleanupCodec();
    AVCodecID detectCodec(const QByteArray& data) const;
    static bool isHardwareDecoder(const AVCodec* codec);

    uint64_t frameCount_ = 0;

    // Performance instrumentation
    PerfStats::Metric metricQueue_;    // signal emit → decode start
    PerfStats::Metric metricDecode_;   // decode start → avcodec_receive_frame
    PerfStats::Metric metricCopy_;     // receive_frame → copy done
    PerfStats::Metric metricTotal_;    // signal emit → setVideoFrame
    PerfStats::TimePoint lastLogTime_ = PerfStats::Clock::now();
    uint64_t framesSinceLog_ = 0;
    static constexpr double LOG_INTERVAL_SEC = 5.0;

    // Frame pool — owns the cached format and allocates QVideoFrames
    std::unique_ptr<VideoFramePool> framePool_;
};

} // namespace aa
} // namespace oap
