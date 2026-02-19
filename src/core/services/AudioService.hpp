#pragma once

#include "IAudioService.hpp"
#include <QObject>
#include <QMutex>
#include <QList>
#include <pipewire/pipewire.h>

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
    bool isAvailable() const { return mainLoop_ != nullptr; }

    // IAudioService
    AudioStreamHandle* createStream(const QString& name, int priority) override;
    void destroyStream(AudioStreamHandle* handle) override;
    int writeAudio(AudioStreamHandle* handle, const uint8_t* data, int size) override;
    void setMasterVolume(int volume) override;
    int masterVolume() const override;
    void requestAudioFocus(AudioStreamHandle* handle, AudioFocusType type) override;
    void releaseAudioFocus(AudioStreamHandle* handle) override;

private:
    void applyDucking();

    struct pw_main_loop* mainLoop_ = nullptr;
    struct pw_context* context_ = nullptr;
    struct pw_core* core_ = nullptr;

    mutable QMutex mutex_;
    QList<AudioStreamHandle*> streams_;
    int masterVolume_ = 80;
};

} // namespace oap
