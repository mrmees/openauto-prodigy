#pragma once

#include "IAudioService.hpp"
#include "core/audio/AudioRingBuffer.hpp"
#include <QObject>
#include <QMutex>
#include <QList>
#include <memory>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

namespace oap {

/// AudioStreamHandle — wraps a PipeWire stream node.
/// Created by AudioService, destroyed by AudioService::destroyStream().
struct AudioStreamHandle {
    QString name;
    int priority = 0;
    struct pw_stream* stream = nullptr;
    AudioFocusType focusType = AudioFocusType::Gain;
    bool hasFocus = false;
    float volume = 1.0f;  // 0.0 - 1.0 (current, may be ducked)
    float baseVolume = 1.0f;  // Volume before ducking

    // Format info for process callback
    int sampleRate = 48000;
    int channels = 2;
    int bytesPerFrame = 4; // channels * sizeof(int16_t)

    // Ring buffer for ASIO → PipeWire bridging
    std::unique_ptr<oap::AudioRingBuffer> ringBuffer;

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
public:
    explicit AudioService(QObject* parent = nullptr);
    ~AudioService() override;

    /// Whether PipeWire was successfully initialized.
    bool isAvailable() const { return threadLoop_ != nullptr; }

    // IAudioService — output
    AudioStreamHandle* createStream(const QString& name, int priority,
                                     int sampleRate = 48000, int channels = 2,
                                     const QString& targetDevice = "auto") override;
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

    // IAudioService — capture
    AudioStreamHandle* openCaptureStream(const QString& name,
                                          int sampleRate, int channels, int bitDepth) override;
    void closeCaptureStream(AudioStreamHandle* handle) override;
    void setCaptureCallback(AudioStreamHandle* handle, CaptureCallback cb) override;

private:
    void applyDucking();
    static void onPlaybackProcess(void* userdata);
    static void onCaptureProcess(void* userdata);

    struct pw_thread_loop* threadLoop_ = nullptr;
    struct pw_context* context_ = nullptr;
    struct pw_core* core_ = nullptr;

    QString outputDevice_ = "auto";
    QString inputDevice_ = "auto";

    mutable QMutex mutex_;
    QList<AudioStreamHandle*> streams_;
    int masterVolume_ = 80;

    // Capture state — one capture stream at a time (mic)
    struct CaptureState {
        AudioStreamHandle* handle = nullptr;
        CaptureCallback callback;
        struct pw_stream_events events{};
    };
    CaptureState capture_;
    struct spa_hook captureListener_{};
};

} // namespace oap
