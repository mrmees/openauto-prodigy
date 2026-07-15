#pragma once

#include "IAudioService.hpp"
#include "core/audio/AudioRingBuffer.hpp"
#include "core/audio/PipeWireDeviceRegistry.hpp"
#include <QObject>
#include <QMutex>
#include <QList>
#include <QTimer>
#include <atomic>
#include <functional>
#include <memory>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

namespace oap {

class EqualizerEngine;  // forward declaration for RT-safe EQ processing

/// AudioStreamHandle — wraps a PipeWire stream node.
/// Created by AudioService, destroyed by AudioService::destroyStream().
struct AudioStreamHandle {
    QString name;
    int priority = 0;
    struct pw_stream* stream = nullptr;
    AudioFocusType focusType = AudioFocusType::Gain;
    bool hasFocus = false;
    // Monotonic stamp of the most recent focus request. Breaks priority ties
    // in selectDominant() so the most recently started source wins among equals.
    uint64_t focusSequence = 0;
    // Focus gain: applyDucking() (Qt thread) stores the target; the playback
    // process callback (PW RT thread) ramps toward it sample-by-sample.
    std::atomic<float> targetGain{1.0f};  // 0.0 - 1.0 (may be ducked/muted)
    float baseVolume = 1.0f;   // gain before ducking (Qt thread only)
    float rtCurrentGain = 1.0f;  // ramp state (PW RT thread only)
    int bufferMs = 50;  // ring buffer size in milliseconds
    int maxBufferMs = 100;  // adaptive growth cap

    // Underrun tracking (written on PW RT thread, read on Qt main thread)
    std::atomic<uint32_t> underrunCount{0};

    // Rate matching state (PW RT thread only, no atomics needed)
    uint32_t rateCtlCount = 0;
    uint32_t diagCount = 0;
    uint32_t activeCallbacks = 0; // callbacks with data in current window
    float filteredFill = 0.5f;    // EMA of normalized fill level
    float rateIntegral = 0.0f;    // PI controller integral term

    // Format info for process callback
    int sampleRate = 48000;
    int channels = 2;
    int bytesPerFrame = 4; // channels * sizeof(int16_t)

    // Ring buffer for ASIO → PipeWire bridging
    std::unique_ptr<oap::AudioRingBuffer> ringBuffer;

    // EQ engine — non-owning, set by orchestrator or createStreamWithOptions
    // (attached BEFORE pw_stream_connect when supplied via options).
    EqualizerEngine* eqEngine = nullptr;

    // Stream-kind + per-handle behaviour flags
    bool isCapture = false;
    bool disableRateMatching = false;  // skips the adaptive rate-match block

    // Capture callback (per-handle). For pre-connect capture handles the
    // callback is installed at creation and is immutable; legacy handles
    // install it via setCaptureCallback. captureCallbackActive guards the RT
    // read against a concurrent legacy-path mutation on the Qt thread.
    IAudioService::CaptureCallback captureCallback;      // immutable after connect
    std::atomic<bool> captureCallbackActive{false};      // legacy-path guard only

    // Playback error hook — dispatched to the Qt main thread when the stream
    // enters PW_STREAM_STATE_ERROR (never invoked on the PW RT thread).
    std::function<void()> onStreamError;

    // PipeWire listener (must outlive stream)
    struct spa_hook listener{};
    struct pw_stream_events events{};
};

/// PipeWire-based audio service.
///
/// Creates PipeWire stream nodes for plugins. Gracefully handles
/// PipeWire daemon being unavailable (returns nullptr from createStream,
/// logs warning). Safe for dev VM where PipeWire may not be running.
class AudioService : public QObject, public IAudioService {
    Q_OBJECT
    Q_PROPERTY(int masterVolume READ masterVolume NOTIFY masterVolumeChanged)
public:
    explicit AudioService(QObject* parent = nullptr);
    ~AudioService() override;

    /// Options for createStreamWithOptions(). Concrete-class API — later tasks
    /// (focus, EQ, BT A2DP) rely on these exact fields.
    struct PlaybackStreamOptions {
        QString name;
        int priority = 50;
        int sampleRate = 48000;
        int channels = 2;
        QString targetDevice = QStringLiteral("auto");
        int bufferMs = 50;
        EqualizerEngine* eqEngine = nullptr;   // attached BEFORE pw_stream_connect
        bool startInactive = false;            // adds PW_STREAM_FLAG_INACTIVE
        bool disableRateMatching = false;      // skips the PI controller + set_rate
        std::function<void()> onStreamError;   // PW_STREAM_STATE_ERROR → Qt thread
    };

    /// Options for openCaptureStreamWithOptions(). Concrete-class API.
    struct CaptureStreamOptions {
        QString name;
        int sampleRate = 48000;
        int channels = 2;
        int bitDepth = 16;
        bool autoconnect = true;   // false ⇒ no AUTOCONNECT flag, inputDevice_ ignored
        IAudioService::CaptureCallback callback;  // installed BEFORE connect, immutable
    };

    /// Whether PipeWire was successfully initialized.
    bool isAvailable() const { return threadLoop_ != nullptr; }

    /// PipeWire handles for auxiliary watchers (e.g. ScoNodeMonitor).
    /// Null when PipeWire is unavailable.
    struct pw_thread_loop* pwThreadLoop() const { return threadLoop_; }
    struct pw_core* pwCore() const { return core_; }

    // IAudioService — output
    AudioStreamHandle* createStream(const QString& name, int priority,
                                     int sampleRate = 48000, int channels = 2,
                                     const QString& targetDevice = "auto",
                                     int bufferMs = 50) override;
    void destroyStream(AudioStreamHandle* handle) override;
    int writeAudio(AudioStreamHandle* handle, const uint8_t* data, int size) override;
    Q_INVOKABLE void setMasterVolume(int volume) override;
    Q_INVOKABLE int masterVolume() const override;
    void requestAudioFocus(AudioStreamHandle* handle, AudioFocusType type) override;
    void releaseAudioFocus(AudioStreamHandle* handle) override;

    // IAudioService — device selection
    Q_INVOKABLE void setOutputDevice(const QString& deviceName) override;
    Q_INVOKABLE void setInputDevice(const QString& deviceName) override;
    Q_INVOKABLE QString outputDevice() const override;
    Q_INVOKABLE QString inputDevice() const override;

    /// Device registry — enumerates PipeWire sinks/sources
    PipeWireDeviceRegistry* deviceRegistry() { return &deviceRegistry_; }

    // IAudioService — capture
    AudioStreamHandle* openCaptureStream(const QString& name,
                                          int sampleRate, int channels, int bitDepth) override;
    void closeCaptureStream(AudioStreamHandle* handle) override;
    void setCaptureCallback(AudioStreamHandle* handle, CaptureCallback cb) override;

    // ---- Concrete-class API (NOT on IAudioService) ----
    // Options-based factories and stream primitives that later tasks build on.

    /// Create a playback stream from options (superset of createStream()).
    /// Returns nullptr when PipeWire is unavailable. Caller owns the handle.
    AudioStreamHandle* createStreamWithOptions(const PlaybackStreamOptions& opts);

    /// Open a capture stream from options. Returns nullptr when PipeWire is
    /// unavailable. Caller owns the handle.
    AudioStreamHandle* openCaptureStreamWithOptions(const CaptureStreamOptions& opts);

    /// Close and destroy a capture stream by handle. Safe to call with nullptr.
    void closeCaptureStreamHandle(AudioStreamHandle* handle);

    /// Activate/deactivate a stream (loop-locked). No-op on null/uninitialised.
    void setStreamActive(AudioStreamHandle* handle, bool active);

    /// Reset a stream's ring buffer (loop-locked, excludes process callbacks).
    /// The caller MUST have deactivated the stream first — the lock excludes
    /// concurrent callbacks, but inactivity is what keeps the reset meaningful.
    void resetStreamRing(AudioStreamHandle* handle);

    /// Perceptual cubic volume curve: (v/100)^3, clamped to [0,100].
    /// Pure/static so the curve is testable without a PipeWire daemon.
    static float cubicVolume(int masterVolume0to100);

    /// Select the dominant focus holder: among streams with hasFocus, the
    /// highest priority; ties broken by highest focusSequence (most recently
    /// requested). Returns nullptr when no stream holds focus. Pure and
    /// lock-free — testable without a PipeWire daemon.
    static AudioStreamHandle* selectDominant(const QList<AudioStreamHandle*>& streams);

signals:
    void masterVolumeChanged();
    void deviceFallback(const QString& lostDevice);

private slots:
    void onDeviceRemoved(uint32_t registryId);

private:
    void applyDucking();
    void checkAdaptiveBuffers();
    static void onPlaybackProcess(void* userdata);
    static void onPlaybackStateChanged(void* userdata, enum pw_stream_state old,
                                       enum pw_stream_state state, const char* error);
    static void onCaptureProcess(void* userdata);

    struct pw_thread_loop* threadLoop_ = nullptr;
    struct pw_context* context_ = nullptr;
    struct pw_core* core_ = nullptr;

    QString outputDevice_ = "auto";
    QString inputDevice_ = "auto";

    mutable QMutex mutex_;
    QList<AudioStreamHandle*> streams_;
    int masterVolume_ = 80;
    // Monotonic focus-request counter (guarded by mutex_ like the rest of focus
    // state). Stamped onto handle->focusSequence on each requestAudioFocus().
    uint64_t focusSeqCounter_ = 0;

    // Capture streams — per-handle listeners live on the handles themselves.
    QList<AudioStreamHandle*> captures_;

    PipeWireDeviceRegistry deviceRegistry_{this};

    // Adaptive buffer growth
    bool adaptiveBuffers_ = true;
    QTimer adaptiveTimer_;
};

} // namespace oap
