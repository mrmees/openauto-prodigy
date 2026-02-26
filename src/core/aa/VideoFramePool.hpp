#pragma once

#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <mutex>
#include <queue>
#include <memory>

#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
#include <QAbstractVideoBuffer>
#endif

namespace oap {
namespace aa {

/**
 * VideoFramePool — recycling pool for software-decode QVideoFrame buffers.
 *
 * On Qt 6.8+: Uses RecycledVideoBuffer (custom QAbstractVideoBuffer) with
 * pooled raw memory. When Qt's render thread releases a frame, the buffer's
 * destructor returns the memory to the pool's free list. Eliminates per-frame
 * heap allocation in steady state.
 *
 * On Qt < 6.8: Falls back to QVideoFrame(format) allocation (no recycling).
 *
 * Thread safety: acquire/acquireRecycled called from decode worker thread.
 * returnBuffer called from any thread (Qt render thread via destructor).
 * Protected by mutex.
 */
class VideoFramePool {
public:
    explicit VideoFramePool(const QVideoFrameFormat& fmt, int poolSize = 5)
        : format_(fmt), poolSize_(poolSize)
    {
        bufferSize_ = computeBufferSize(fmt);
    }

    /// Returns a new QVideoFrame (Qt < 6.8 path, no recycling)
    QVideoFrame acquire()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++totalAllocated_;
        return QVideoFrame(format_);
    }

#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
    /// Returns a QVideoFrame backed by a recycled buffer (Qt 6.8+ only).
    /// The buffer memory is returned to the pool when Qt releases the frame.
    QVideoFrame acquireRecycled();
#endif

    /// Return a buffer to the free list (called from RecycledVideoBuffer destructor)
    void returnBuffer(std::unique_ptr<uint8_t[]> buf)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        freeBuffers_.push(std::move(buf));
    }

    /// Reset pool for new resolution/pixel format
    void reset(const QVideoFrameFormat& fmt)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        format_ = fmt;
        bufferSize_ = computeBufferSize(fmt);
        totalAllocated_ = 0;
        totalRecycled_ = 0;
        // Discard free buffers — they're the wrong size
        while (!freeBuffers_.empty())
            freeBuffers_.pop();
    }

    int totalAllocated() const { std::lock_guard<std::mutex> lock(mutex_); return totalAllocated_; }
    int totalRecycled() const { std::lock_guard<std::mutex> lock(mutex_); return totalRecycled_; }
    int freeCount() const { std::lock_guard<std::mutex> lock(mutex_); return static_cast<int>(freeBuffers_.size()); }
    const QVideoFrameFormat& format() const { return format_; }
    int bufferSize() const { return bufferSize_; }

private:
    static int computeBufferSize(const QVideoFrameFormat& fmt)
    {
        int w = fmt.frameWidth();
        int h = fmt.frameHeight();
        return w * h * 3 / 2;  // YUV420P: Y + U + V
    }

    std::unique_ptr<uint8_t[]> acquireRawBuffer()
    {
        // Must be called with mutex held
        if (!freeBuffers_.empty()) {
            auto buf = std::move(freeBuffers_.front());
            freeBuffers_.pop();
            ++totalRecycled_;
            return buf;
        }
        ++totalAllocated_;
        return std::make_unique<uint8_t[]>(bufferSize_);
    }

    mutable std::mutex mutex_;
    QVideoFrameFormat format_;
    int poolSize_;
    int bufferSize_ = 0;
    int totalAllocated_ = 0;
    int totalRecycled_ = 0;
    std::queue<std::unique_ptr<uint8_t[]>> freeBuffers_;
};

#if QT_VERSION >= QT_VERSION_CHECK(6,8,0)
/**
 * RecycledVideoBuffer — QAbstractVideoBuffer backed by pool-managed memory.
 *
 * Owns a raw YUV420P buffer from VideoFramePool. When Qt's render thread
 * releases the last QVideoFrame reference, this destructor fires and returns
 * the buffer to the pool for reuse.
 *
 * Layout: tightly packed Y/U/V planes (stride = width for Y, width/2 for chroma).
 */
class RecycledVideoBuffer : public QAbstractVideoBuffer {
public:
    RecycledVideoBuffer(std::unique_ptr<uint8_t[]> data, int width, int height,
                        const QVideoFrameFormat& fmt, VideoFramePool* pool)
        : data_(std::move(data)), width_(width), height_(height)
        , format_(fmt), pool_(pool) {}

    ~RecycledVideoBuffer() override
    {
        if (pool_ && data_)
            pool_->returnBuffer(std::move(data_));
    }

    QVideoFrameFormat format() const override { return format_; }

    MapData map(QVideoFrame::MapMode /*mode*/) override
    {
        MapData d;
        if (!data_) return d;

        d.planeCount = 3;

        // Y plane
        d.data[0] = data_.get();
        d.bytesPerLine[0] = width_;
        d.dataSize[0] = width_ * height_;

        // U plane
        d.data[1] = data_.get() + width_ * height_;
        d.bytesPerLine[1] = width_ / 2;
        d.dataSize[1] = (width_ / 2) * (height_ / 2);

        // V plane
        d.data[2] = d.data[1] + d.dataSize[1];
        d.bytesPerLine[2] = width_ / 2;
        d.dataSize[2] = (width_ / 2) * (height_ / 2);

        return d;
    }

    void unmap() override {}

    /// Direct access for the decode thread to fill data before wrapping in QVideoFrame
    uint8_t* data() { return data_.get(); }

private:
    std::unique_ptr<uint8_t[]> data_;
    int width_;
    int height_;
    QVideoFrameFormat format_;
    VideoFramePool* pool_;
};

inline QVideoFrame VideoFramePool::acquireRecycled()
{
    std::unique_ptr<uint8_t[]> buf;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        buf = acquireRawBuffer();
    }
    int w = format_.frameWidth();
    int h = format_.frameHeight();
    auto buffer = std::make_unique<RecycledVideoBuffer>(
        std::move(buf), w, h, format_, this);
    return QVideoFrame(std::move(buffer));
}
#endif // QT_VERSION >= 6.8.0

} // namespace aa
} // namespace oap
