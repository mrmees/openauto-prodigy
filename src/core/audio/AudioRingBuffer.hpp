#pragma once

#include <spa/utils/ringbuffer.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace oap {

// Lock-free SPSC ring buffer wrapping PipeWire's spa_ringbuffer.
// One producer (ASIO thread), one consumer (PipeWire RT callback).
// Capacity MUST be a power of 2.
class AudioRingBuffer {
public:
    explicit AudioRingBuffer(uint32_t capacity)
        : capacity_(capacity)
        , data_(capacity, 0)
    {
        spa_ringbuffer_init(&ring_);
    }

    uint32_t capacity() const { return capacity_; }

    uint32_t available() const
    {
        uint32_t idx;
        int32_t avail = spa_ringbuffer_get_read_index(
            const_cast<struct spa_ringbuffer*>(&ring_), &idx);
        if (avail <= 0) return 0u;
        return std::min(static_cast<uint32_t>(avail), capacity_);
    }

    uint32_t write(const uint8_t* src, uint32_t size)
    {
        if (!src || size == 0) return 0;

        // Strict SPSC ownership: the producer owns ONLY writeindex and the
        // consumer owns ONLY readindex. Snapshot current fill through the
        // writer-side SPA helper, then copy only what fits. The unwritten tail
        // is drop-newest and is observable to the caller as a short write.
        uint32_t writeIdx;
        const int32_t fill = spa_ringbuffer_get_write_index(&ring_, &writeIdx);
        const uint32_t used = (fill <= 0)
            ? 0u : std::min(static_cast<uint32_t>(fill), capacity_);
        const uint32_t free = capacity_ - used;
        const uint32_t toWrite = std::min(size, free);
        if (toWrite < size)
            dropCount_.fetch_add(1, std::memory_order_relaxed);

        if (toWrite == 0) return 0;

        uint32_t offset = writeIdx & (capacity_ - 1);
        spa_ringbuffer_write_data(&ring_, data_.data(), capacity_,
                                  offset, src, toWrite);

        spa_ringbuffer_write_update(&ring_,
            static_cast<int32_t>(writeIdx + toWrite));
        return toWrite;
    }

    uint32_t read(uint8_t* dst, uint32_t size)
    {
        if (!dst || size == 0) return 0;

        uint32_t readIdx;
        int32_t avail = spa_ringbuffer_get_read_index(&ring_, &readIdx);
        if (avail <= 0) return 0;

        uint32_t toRead = (size > static_cast<uint32_t>(avail))
                              ? static_cast<uint32_t>(avail) : size;

        uint32_t offset = readIdx & (capacity_ - 1);
        spa_ringbuffer_read_data(&ring_, data_.data(), capacity_,
                                 offset, dst, toRead);

        spa_ringbuffer_read_update(&ring_,
            static_cast<int32_t>(readIdx + toRead));
        return toRead;
    }

    // Full re-init of both indices. NOT writer-safe: the non-atomic reset of
    // write index tears against a live writer. Use ONLY in genuinely-quiescent
    // contexts (e.g. construction) — never while an RT process/capture callback
    // may be writing. For runtime flushing use drain() instead.
    void reset()
    {
        spa_ringbuffer_init(&ring_);
    }

    // Reader-side drain: snapshot the write index and advance the read index to
    // it, discarding all bytes published before the snapshot. Touches ONLY the
    // read index, like read(). The reader must be quiesced so drain() is its sole
    // cursor mutator. A live writer is safe: bytes published after the snapshot
    // remain readable. The BT tap retains its stronger both-sides-quiesced
    // transition ordering so activation always starts from an empty queue.
    void drain()
    {
        uint32_t readIdx;
        int32_t avail = spa_ringbuffer_get_read_index(&ring_, &readIdx);
        if (avail <= 0) return;
        spa_ringbuffer_read_update(&ring_,
            static_cast<int32_t>(readIdx + static_cast<uint32_t>(avail)));
    }

    uint32_t dropCount() const
    {
        return dropCount_.load(std::memory_order_relaxed);
    }

    uint32_t resetDropCount()
    {
        return dropCount_.exchange(0, std::memory_order_relaxed);
    }

private:
    uint32_t capacity_;
    std::vector<uint8_t> data_;
    struct spa_ringbuffer ring_{};
    std::atomic<uint32_t> dropCount_{0};
};

} // namespace oap
