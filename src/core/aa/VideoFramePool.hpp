#pragma once

#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <mutex>

namespace oap {
namespace aa {

/**
 * VideoFramePool — encapsulates QVideoFrame allocation for the software decode path.
 *
 * Currently wraps QVideoFrame(format) construction with cached format management
 * and the reset() interface for resolution changes. The real allocation optimization
 * comes in Task 11 (Qt 6.8 QAbstractVideoBuffer with custom backing memory).
 *
 * Thread safety: acquire() is called from the decode worker thread.
 * reset() is called when resolution changes (also decode worker thread).
 * No cross-thread contention in current usage, but mutex is included
 * for future-proofing if render-thread recycling is added.
 */
class VideoFramePool {
public:
    explicit VideoFramePool(const QVideoFrameFormat& fmt, int poolSize = 5)
        : format_(fmt), poolSize_(poolSize) {}

    /// Returns a new QVideoFrame ready for plane copy (caller must map/unmap)
    QVideoFrame acquire()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++totalAllocated_;
        return QVideoFrame(format_);
    }

    /// Reset pool for new resolution/pixel format
    void reset(const QVideoFrameFormat& fmt)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        format_ = fmt;
        totalAllocated_ = 0;
    }

    /// Total frames allocated since construction (for testing/diagnostics)
    int totalAllocated() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return totalAllocated_;
    }

    /// Current format
    const QVideoFrameFormat& format() const { return format_; }

private:
    mutable std::mutex mutex_;
    QVideoFrameFormat format_;
    int poolSize_;          // Hint for future pre-allocation
    int totalAllocated_ = 0;
};

} // namespace aa
} // namespace oap
