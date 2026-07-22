#pragma once

#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <mutex>
#include <queue>
#include <memory>
#include <cstdint>
#include <QAbstractVideoBuffer>

namespace oap {
namespace aa {

struct PooledVideoAllocation {
    std::unique_ptr<uint8_t[]> data;
    int capacity = 0;
    uint64_t generation = 0;
};

struct VideoFramePoolState {
    mutable std::mutex mutex;
    int bufferSize = 0;
    int maxFree = 0;          // cap on retained free buffers (from poolSize)
    uint64_t generation = 0;
    int totalAllocated = 0;
    int totalRecycled = 0;
    std::queue<PooledVideoAllocation> freeBuffers;
};

/**
 * VideoFramePool — recycling pool for software-decode QVideoFrame buffers.
 *
 * Uses RecycledVideoBuffer (custom QAbstractVideoBuffer) with pooled raw
 * memory. When Qt's render thread releases a frame, the buffer's destructor
 * returns the memory to the pool's free list. Eliminates per-frame heap
 * allocation in steady state.
 *
 * Thread safety: acquireRecycled/reset run on the decode worker; backing-buffer
 * destruction can run on any thread (including Qt's render thread). Both paths
 * synchronize through the shared return-state mutex.
 */
class VideoFramePool {
public:
    explicit VideoFramePool(const QVideoFrameFormat& fmt, int poolSize = 5)
        : state_(std::make_shared<VideoFramePoolState>())
        , format_(fmt)
    {
        state_->bufferSize = computeBufferSize(fmt);
        state_->maxFree = poolSize;
    }

    /// Returns a QVideoFrame backed by a recycled buffer.
    /// The buffer memory is returned to the pool when Qt releases the frame.
    QVideoFrame acquireRecycled();

    /// Reset pool for new resolution/pixel format
    void reset(const QVideoFrameFormat& fmt)
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        format_ = fmt;
        state_->bufferSize = computeBufferSize(fmt);
        ++state_->generation;
        state_->totalAllocated = 0;
        state_->totalRecycled = 0;
        // Discard free buffers — they're the wrong size
        while (!state_->freeBuffers.empty())
            state_->freeBuffers.pop();
    }

    int totalAllocated() const { std::lock_guard<std::mutex> lock(state_->mutex); return state_->totalAllocated; }
    int totalRecycled() const { std::lock_guard<std::mutex> lock(state_->mutex); return state_->totalRecycled; }
    int freeCount() const { std::lock_guard<std::mutex> lock(state_->mutex); return static_cast<int>(state_->freeBuffers.size()); }
    const QVideoFrameFormat& format() const { return format_; }

private:
    static int computeBufferSize(const QVideoFrameFormat& fmt)
    {
        int w = fmt.frameWidth();
        int h = fmt.frameHeight();
        return w * h * 3 / 2;  // YUV420P: Y + U + V
    }

    PooledVideoAllocation acquireRawBuffer()
    {
        // Must be called with state_->mutex held. Returned buffers are already
        // generation/size-filtered, but keep the checks here as a defensive
        // invariant against future return-path changes.
        while (!state_->freeBuffers.empty()) {
            auto allocation = std::move(state_->freeBuffers.front());
            state_->freeBuffers.pop();
            if (allocation.generation == state_->generation
                && allocation.capacity >= state_->bufferSize) {
                ++state_->totalRecycled;
                return allocation;
            }
        }
        ++state_->totalAllocated;
        return {
            std::make_unique<uint8_t[]>(state_->bufferSize),
            state_->bufferSize,
            state_->generation,
        };
    }

    std::shared_ptr<VideoFramePoolState> state_;
    QVideoFrameFormat format_;
};

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
    RecycledVideoBuffer(PooledVideoAllocation allocation, int width, int height,
                        const QVideoFrameFormat& fmt,
                        std::weak_ptr<VideoFramePoolState> state)
        : data_(std::move(allocation.data))
        , capacity_(allocation.capacity)
        , generation_(allocation.generation)
        , width_(width)
        , height_(height)
        , format_(fmt)
        , state_(std::move(state)) {}

    ~RecycledVideoBuffer() override
    {
        if (!data_)
            return;

        auto state = state_.lock();
        if (!state)
            return;

        std::lock_guard<std::mutex> lock(state->mutex);
        if (generation_ != state->generation || capacity_ < state->bufferSize)
            return;
        // Bound the free list to poolSize; drop excess buffers (freed here).
        if (static_cast<int>(state->freeBuffers.size()) >= state->maxFree)
            return;

        state->freeBuffers.push({std::move(data_), capacity_, generation_});
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
    int capacity_;
    uint64_t generation_;
    int width_;
    int height_;
    QVideoFrameFormat format_;
    std::weak_ptr<VideoFramePoolState> state_;
};

inline QVideoFrame VideoFramePool::acquireRecycled()
{
    PooledVideoAllocation allocation;
    QVideoFrameFormat format;
    int w = 0;
    int h = 0;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        allocation = acquireRawBuffer();
        format = format_;
        w = format.frameWidth();
        h = format.frameHeight();
    }
    auto buffer = std::make_unique<RecycledVideoBuffer>(
        std::move(allocation), w, h, format, state_);
    return QVideoFrame(std::move(buffer));
}

} // namespace aa
} // namespace oap
