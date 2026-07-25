#pragma once

#include "core/audio/AudioRingBuffer.hpp"

#include <QByteArray>
#include <QObject>
#include <QTimer>

#include <atomic>
#include <cstdint>
#include <functional>

namespace oap::aa {

/// Bounded SPSC bridge from PipeWire's capture callback to the AA/Qt owner
/// thread. The producer entry point is real-time safe: it only checks atomics
/// and copies into preallocated ring storage. Framing, gain, logging, and the
/// protocol sender all run from the Qt-owner drain timer.
class AVInputCaptureBridge : public QObject {
    Q_OBJECT
public:
    using Sender = std::function<bool(const QByteArray& pcm, uint64_t timestampUs)>;

    static constexpr uint32_t RingCapacity = 4096;
    static constexpr uint32_t FrameBytes = 640;  // 20 ms, 16 kHz mono S16LE
    static constexpr int DrainIntervalMs = 10;

    explicit AVInputCaptureBridge(QObject* parent = nullptr);

    /// Starts a fresh capture generation and returns its non-zero token.
    /// Must be called on the QObject owner thread after the prior producer is
    /// quiesced. An empty sender fails closed and returns zero.
    uint64_t start(double gain, Sender sender);

    /// Stops consumption and purges queued PCM. The owning capture stream must
    /// be closed first so no producer is in flight across the generation edge.
    void stop();

    /// PipeWire real-time producer entry point. Safe from any one producer
    /// thread; performs no Qt call, allocation, lock, or logging.
    void pushPcm(uint64_t generation, const uint8_t* data, int size) noexcept;

    /// Called on the Qt owner thread when the AVInput sender regains a permit.
    void notifyWindowAvailable();

    bool isActive() const { return active_; }
    uint64_t generation() const { return ownerGeneration_; }
    uint64_t droppedFrames() const { return droppedFrames_; }

    static double normalizedGain(double gain);
    static void applyGainS16Le(QByteArray& pcm, double gain);

private:
    void drainOnce();
    void purgeQueuedPcm();
    bool recordOverflowEvents(uint32_t currentEpoch, const char* context);
    static uint64_t monotonicTimestampUs();

    oap::AudioRingBuffer ring_{RingCapacity};
    QTimer drainTimer_;
    Sender sender_;
    std::atomic<uint64_t> publishedGeneration_{0};
    std::atomic<uint32_t> oversizedCallbacks_{0};
    uint64_t ownerGeneration_ = 0;
    double gain_ = 1.0;
    bool active_ = false;
    bool waitingForWindow_ = false;
    uint32_t observedDropEpoch_ = 0;
    uint64_t droppedFrames_ = 0;
};

} // namespace oap::aa
