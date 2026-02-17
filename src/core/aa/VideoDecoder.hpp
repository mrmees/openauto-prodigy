#pragma once

#include <QObject>
#include <QByteArray>
#include <QVideoSink>
#include <QVideoFrame>
#include <QVideoFrameFormat>

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
    void decodeFrame(QByteArray h264Data);

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
};

} // namespace aa
} // namespace oap
