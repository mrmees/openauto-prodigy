#pragma once

#include <spa/utils/ringbuffer.h>
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
        return (avail > 0) ? static_cast<uint32_t>(avail) : 0u;
    }

    uint32_t write(const uint8_t* src, uint32_t size)
    {
        if (!src || size == 0) return 0;

        uint32_t toWrite = (size > capacity_) ? capacity_ : size;

        // If writing would overflow, advance read pointer to drop oldest data
        uint32_t avail = available();
        if (avail + toWrite > capacity_) {
            uint32_t drop = avail + toWrite - capacity_;
            uint32_t readIdx;
            spa_ringbuffer_get_read_index(&ring_, &readIdx);
            spa_ringbuffer_read_update(&ring_,
                static_cast<int32_t>(readIdx + drop));
            dropCount_.fetch_add(1, std::memory_order_relaxed);
        }

        uint32_t writeIdx;
        spa_ringbuffer_get_write_index(&ring_, &writeIdx);

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

    void reset()
    {
        spa_ringbuffer_init(&ring_);
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
