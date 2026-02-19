#pragma once

#include <QString>
#include <cstdint>

namespace oap {

/// Audio focus types matching Android audio focus semantics.
enum class AudioFocusType {
    Gain,                ///< Exclusive, long-duration (music playback)
    GainTransient,       ///< Short interruption (navigation prompt)
    GainTransientMayDuck ///< Lower other streams, don't pause (notification)
};

/// Opaque handle to a PipeWire stream created by the audio service.
/// Plugins do NOT manage PipeWire internals directly.
struct AudioStreamHandle;

class IAudioService {
public:
    virtual ~IAudioService() = default;

    /// Create a named audio stream with a given priority (0-100).
    /// Higher priority streams may duck or mute lower ones.
    /// Must be called from the main thread. Returns nullptr on failure
    /// (e.g., PipeWire daemon not available). Caller owns the handle.
    virtual AudioStreamHandle* createStream(const QString& name, int priority) = 0;

    /// Destroy a previously created stream. Safe to call with nullptr.
    /// Must be called from the main thread.
    virtual void destroyStream(AudioStreamHandle* handle) = 0;

    /// Write PCM audio data to a stream. Can be called from any thread.
    /// Returns number of bytes written, or -1 on error.
    virtual int writeAudio(AudioStreamHandle* handle, const uint8_t* data, int size) = 0;

    /// Set master output volume (0-100).
    /// Thread-safe.
    virtual void setMasterVolume(int volume) = 0;

    /// Get current master output volume (0-100).
    /// Thread-safe.
    virtual int masterVolume() const = 0;

    /// Request audio focus for the given stream. Other streams may be
    /// ducked or paused based on focus type and relative priority.
    /// Thread-safe.
    virtual void requestAudioFocus(AudioStreamHandle* handle, AudioFocusType type) = 0;

    /// Release audio focus. Previously ducked streams are restored.
    /// Thread-safe.
    virtual void releaseAudioFocus(AudioStreamHandle* handle) = 0;
};

} // namespace oap
