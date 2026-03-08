#pragma once

#include <QByteArray>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>

namespace oaa {

class CircularBuffer {
public:
    explicit CircularBuffer(size_t initialCapacity = 65536)
        : m_data(initialCapacity)
        , m_capacity(initialCapacity)
    {
    }

    void append(const char* src, size_t len)
    {
        if (len == 0)
            return;

        // Auto-grow if needed
        if (m_size + len > m_capacity) {
            grow(m_size + len);
        }

        // Write data, handling wrap-around
        size_t firstChunk = std::min(len, m_capacity - m_writePos);
        std::memcpy(m_data.data() + m_writePos, src, firstChunk);
        if (firstChunk < len) {
            std::memcpy(m_data.data(), src + firstChunk, len - firstChunk);
        }
        m_writePos = (m_writePos + len) % m_capacity;
        m_size += len;
    }

    size_t available() const { return m_size; }

    QByteArray peek(size_t len) const
    {
        assert(len <= m_size);
        len = std::min(len, m_size);

        QByteArray result(static_cast<int>(len), Qt::Uninitialized);
        size_t firstChunk = std::min(len, m_capacity - m_readPos);
        std::memcpy(result.data(), m_data.data() + m_readPos, firstChunk);
        if (firstChunk < len) {
            std::memcpy(result.data() + firstChunk, m_data.data(), len - firstChunk);
        }
        return result;
    }

    const char* readPtr(size_t& contigLen) const
    {
        if (m_size == 0) {
            contigLen = 0;
            return nullptr;
        }
        contigLen = std::min(m_size, m_capacity - m_readPos);
        return m_data.data() + m_readPos;
    }

    void consume(size_t len)
    {
        assert(len <= m_size);
        len = std::min(len, m_size);
        m_readPos = (m_readPos + len) % m_capacity;
        m_size -= len;

        // Reset positions when empty to avoid unnecessary wrapping
        if (m_size == 0) {
            m_readPos = 0;
            m_writePos = 0;
        }
    }

private:
    void grow(size_t minCapacity)
    {
        size_t newCapacity = m_capacity;
        while (newCapacity < minCapacity)
            newCapacity *= 2;

        std::vector<char> newData(newCapacity);

        // Linearize existing data into new buffer
        if (m_size > 0) {
            size_t firstChunk = std::min(m_size, m_capacity - m_readPos);
            std::memcpy(newData.data(), m_data.data() + m_readPos, firstChunk);
            if (firstChunk < m_size) {
                std::memcpy(newData.data() + firstChunk, m_data.data(), m_size - firstChunk);
            }
        }

        m_data = std::move(newData);
        m_capacity = newCapacity;
        m_readPos = 0;
        m_writePos = m_size;
    }

    std::vector<char> m_data;
    size_t m_capacity = 0;
    size_t m_readPos = 0;
    size_t m_writePos = 0;
    size_t m_size = 0;
};

} // namespace oaa
