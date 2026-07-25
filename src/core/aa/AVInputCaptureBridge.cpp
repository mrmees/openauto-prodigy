#include "AVInputCaptureBridge.hpp"

#include "core/Logging.hpp"

#include <QThread>
#include <QtEndian>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>

namespace oap::aa {

AVInputCaptureBridge::AVInputCaptureBridge(QObject* parent)
    : QObject(parent)
{
    drainTimer_.setInterval(DrainIntervalMs);
    drainTimer_.setTimerType(Qt::PreciseTimer);
    connect(&drainTimer_, &QTimer::timeout, this,
            &AVInputCaptureBridge::drainOnce);
}

uint64_t AVInputCaptureBridge::start(double gain, Sender sender)
{
    Q_ASSERT(QThread::currentThread() == thread());

    // The capture owner quiesces the prior producer before starting a new
    // generation. Purge anything left by the old reader/writer pair.
    stop();
    if (!sender)
        return 0;
    if (++ownerGeneration_ == 0)
        ++ownerGeneration_;

    gain_ = normalizedGain(gain);
    sender_ = std::move(sender);
    waitingForWindow_ = false;
    active_ = true;
    publishedGeneration_.store(ownerGeneration_, std::memory_order_release);
    drainTimer_.start();
    return ownerGeneration_;
}

void AVInputCaptureBridge::stop()
{
    Q_ASSERT(QThread::currentThread() == thread());
    publishedGeneration_.store(0, std::memory_order_release);
    drainTimer_.stop();
    active_ = false;
    waitingForWindow_ = false;
    sender_ = {};
    purgeQueuedPcm();
}

void AVInputCaptureBridge::pushPcm(uint64_t generation,
                                   const uint8_t* data, int size) noexcept
{
    if (!data || size < 2 || generation == 0
        || publishedGeneration_.load(std::memory_order_acquire) != generation) {
        return;
    }

    // Preserve complete S16LE samples if a malformed producer supplies an odd
    // byte count. AudioRingBuffer::write is bounded and drop-newest.
    const uint32_t evenSize = static_cast<uint32_t>(size) & ~uint32_t{1};
    ring_.write(data, evenSize);
}

void AVInputCaptureBridge::notifyWindowAvailable()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!active_ || !waitingForWindow_)
        return;

    // PCM accumulated while the phone withheld permits is stale voice. Resume
    // from the live edge rather than replaying delayed speech.
    purgeQueuedPcm();
    waitingForWindow_ = false;
    drainTimer_.start();
}

double AVInputCaptureBridge::normalizedGain(double gain)
{
    if (!std::isfinite(gain))
        return 1.0;
    return std::clamp(gain, 0.5, 4.0);
}

void AVInputCaptureBridge::applyGainS16Le(QByteArray& pcm, double gain)
{
    const double normalized = normalizedGain(gain);
    const qsizetype evenSize = pcm.size() & ~qsizetype{1};
    auto* bytes = reinterpret_cast<uchar*>(pcm.data());

    for (qsizetype offset = 0; offset < evenSize; offset += 2) {
        const qint16 sample = qFromLittleEndian<qint16>(bytes + offset);
        const long scaled = std::lround(static_cast<double>(sample) * normalized);
        const qint16 saturated = static_cast<qint16>(std::clamp(
            scaled,
            static_cast<long>(std::numeric_limits<qint16>::min()),
            static_cast<long>(std::numeric_limits<qint16>::max())));
        qToLittleEndian<qint16>(saturated, bytes + offset);
    }
}

void AVInputCaptureBridge::drainOnce()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!active_ || waitingForWindow_ || !sender_)
        return;

    const uint32_t overflowDrops = ring_.resetDropCount();
    if (overflowDrops > 0) {
        ++droppedFrames_;
        qCWarning(lcAA) << "AA microphone PCM ring overflow; purging stale audio"
                        << "events=" << overflowDrops;
        ring_.drain();
        return;
    }

    if (ring_.available() < FrameBytes)
        return;

    QByteArray frame(static_cast<qsizetype>(FrameBytes), '\0');
    if (ring_.read(reinterpret_cast<uint8_t*>(frame.data()), FrameBytes)
        != FrameBytes) {
        return;
    }
    applyGainS16Le(frame, gain_);

    if (!sender_(frame, monotonicTimestampUs())) {
        ++droppedFrames_;
        waitingForWindow_ = true;
        drainTimer_.stop();
        ring_.drain();
    }
}

void AVInputCaptureBridge::purgeQueuedPcm()
{
    ring_.drain();
    ring_.resetDropCount();
}

uint64_t AVInputCaptureBridge::monotonicTimestampUs()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

} // namespace oap::aa
