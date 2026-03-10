#pragma once

#include <QAbstractVideoBuffer>
#include <QVideoFrame>
#include <QVideoFrameFormat>

extern "C" {
#include <libavutil/frame.h>
}

namespace oap {
namespace aa {

/// Wraps a DRM_PRIME dmabuf fd for zero-copy rendering via Qt's RHI.
/// The AVFrame is kept alive (ref-counted) so the dmabuf fd stays valid
/// until Qt is done rendering the frame.
///
/// Qt 6.8 redesigned QAbstractVideoBuffer: no constructor args, format() is
/// pure virtual, and QVideoFrame takes std::unique_ptr<QAbstractVideoBuffer>.
///
/// NOTE: Requires Qt 6.8+ (QAbstractVideoBuffer redesign: no constructor args,
/// format() is pure virtual, QVideoFrame takes std::unique_ptr).
class DmaBufVideoBuffer : public QAbstractVideoBuffer
{
public:
    /// Takes ownership of a ref to the AVFrame (caller must av_frame_ref or clone).
    explicit DmaBufVideoBuffer(AVFrame* frame, int width, int height);
    ~DmaBufVideoBuffer() override;

    /// Returns the video format for this buffer (required by Qt 6.8 API).
    QVideoFrameFormat format() const override;

    MapData map(QVideoFrame::MapMode mode) override;
    void unmap() override;

private:
    AVFrame* frame_;              // ref-counted, kept alive for dmabuf fd lifetime
    AVFrame* swFrame_ = nullptr;  // CPU fallback (lazy, for map() support)
    bool mapped_ = false;
    int width_;
    int height_;
};

} // namespace aa
} // namespace oap
