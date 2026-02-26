#include "DmaBufVideoBuffer.hpp"

#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
}

#include <QDebug>

namespace oap {
namespace aa {

DmaBufVideoBuffer::DmaBufVideoBuffer(AVFrame* frame, int width, int height)
    : QAbstractVideoBuffer()
    , frame_(av_frame_clone(frame))  // clone to hold our own ref
    , width_(width)
    , height_(height)
{
}

DmaBufVideoBuffer::~DmaBufVideoBuffer()
{
    if (swFrame_)
        av_frame_free(&swFrame_);
    if (frame_)
        av_frame_free(&frame_);
}

QVideoFrameFormat DmaBufVideoBuffer::format() const
{
    return QVideoFrameFormat(QSize(width_, height_),
                             QVideoFrameFormat::Format_YUV420P);
}

QAbstractVideoBuffer::MapData DmaBufVideoBuffer::map(QVideoFrame::MapMode mode)
{
    Q_UNUSED(mode);
    MapData data;

    if (mapped_) return data;

    // For DRM_PRIME frames, we need to transfer to CPU for mapping.
    // Qt's RHI *might* use the texture handle directly for rendering,
    // but map() is the CPU-access fallback path.
    if (!swFrame_) {
        swFrame_ = av_frame_alloc();
        if (av_hwframe_transfer_data(swFrame_, frame_, 0) < 0) {
            qWarning() << "[DmaBufVideoBuffer] Failed to transfer hw frame to CPU";
            av_frame_free(&swFrame_);
            swFrame_ = nullptr;
            return data;
        }
    }

    // Expose Y, U, V planes
    data.planeCount = 3;
    for (int i = 0; i < 3; ++i) {
        data.data[i] = swFrame_->data[i];
        data.bytesPerLine[i] = swFrame_->linesize[i];
        // Size calculation: plane height * stride
        int h = (i == 0) ? swFrame_->height : swFrame_->height / 2;
        data.dataSize[i] = swFrame_->linesize[i] * h;
    }

    mapped_ = true;
    return data;
}

void DmaBufVideoBuffer::unmap()
{
    mapped_ = false;
}

} // namespace aa
} // namespace oap

#endif // QT_VERSION >= QT_VERSION_CHECK(6,8,0)
