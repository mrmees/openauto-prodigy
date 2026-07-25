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
    ring_.resetDropCount();
    oversizedCallbacks_.store(0, std::memory_order_relaxed);
    observedDropEpoch_ = ring_.dropEpoch();
}

void AVInputCaptureBridge::pushPcm(uint64_t generation,
                                   const uint8_t* data, int size) noexcept
{
    if (!data || size < 2 || generation == 0
        || publishedGeneration_.load(std::memory_order_acquire) != generation) {
        return;
    }

    // Preserve complete S16LE samples if a malformed producer supplies an odd
    // byte count. A capture callback is published atomically or dropped whole;
    // partial publication could cross an overflow purge and corrupt framing.
    const uint32_t evenSize = static_cast<uint32_t>(size) & ~uint32_t{1};

    // PipeWire normally delivers a much smaller quantum, but negotiated input
    // buffers are not contractually capped by this bridge. If an oversized
    // callback arrives at an empty ring, retain its newest complete AA frame so
    // repeated large callbacks cannot create permanent silence. If older PCM
    // is still queued, record an ordinary overflow and let the consumer purge
    // it first; the next callback can then publish from the live edge.
    if (evenSize > RingCapacity) {
        if (ring_.available() != 0) {
            ring_.writeAllOrDrop(data, evenSize);
            return;
        }
        const uint8_t* newestFrame = data + evenSize - FrameBytes;
        if (ring_.writeAllOrDrop(newestFrame, FrameBytes) == FrameBytes) {
            oversizedCallbacks_.fetch_add(1, std::memory_order_relaxed);
        }
        return;
    }
    ring_.writeAllOrDrop(data, evenSize);
}

void AVInputCaptureBridge::notifyWindowAvailable()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!active_ || !waitingForWindow_)
        return;

    // PCM accumulated while the phone withheld permits is stale voice. Resume
    // from the live edge rather than replaying delayed speech.
    const uint32_t oversized = oversizedCallbacks_.exchange(
        0, std::memory_order_relaxed);
    if (oversized > 0) {
        droppedFrames_ += oversized;
        qCWarning(lcAA) << "AA microphone oversized capture buffers truncated"
                        << "events=" << oversized;
    }
    recordOverflowEvents(ring_.dropEpoch(),
                         "AA microphone PCM overflow while waiting for ACK");
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

    const uint32_t oversized = oversizedCallbacks_.exchange(
        0, std::memory_order_relaxed);
    if (oversized > 0) {
        droppedFrames_ += oversized;
        qCWarning(lcAA) << "AA microphone oversized capture buffers truncated"
                        << "events=" << oversized;
    }
    const uint32_t overflowBeforeRead = ring_.dropEpoch();
    if (recordOverflowEvents(
            overflowBeforeRead,
            "AA microphone PCM ring overflow; purging stale audio")) {
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

    // A producer can report overflow after the first epoch check but before
    // this read completes. Do not transmit the now-stale frame; purge from the
    // live edge and let the next callback resume with fresh speech.
    const uint32_t overflowAfterRead = ring_.dropEpoch();
    if (overflowAfterRead != overflowBeforeRead) {
        recordOverflowEvents(
            overflowAfterRead,
            "AA microphone PCM overflow raced frame read; purging stale audio");
        ring_.drain();
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
}

bool AVInputCaptureBridge::recordOverflowEvents(uint32_t currentEpoch,
                                                const char* context)
{
    const uint32_t events = currentEpoch - observedDropEpoch_;
    observedDropEpoch_ = currentEpoch;
    if (events == 0)
        return false;

    droppedFrames_ += events;
    qCWarning(lcAA) << context << "events=" << events;
    return true;
}

uint64_t AVInputCaptureBridge::monotonicTimestampUs()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

} // namespace oap::aa
