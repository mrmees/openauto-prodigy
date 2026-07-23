#ifndef OAP_IEQUALIZER_SERVICE_HPP
#define OAP_IEQUALIZER_SERVICE_HPP

#include <array>
#include <QString>
#include <QStringList>
#include "core/audio/BiquadFilter.hpp"

namespace oap {

/// Identifies an audio stream for per-stream EQ control
enum class StreamId {
    Media,
    Navigation,
    System,           // renamed from Phone — value stays 2 (processes AA system
                      // sounds, not telephony; honest labeling, design §4.6)
    Phone = System    // deprecated source-compat alias for external plugins
};

/// Pure virtual interface for the equalizer service (testability)
class IEqualizerService {
public:
    virtual ~IEqualizerService() = default;

    /// Get the active preset name for a stream ("" if manually adjusted or
    /// the stream is invalid)
    virtual QString activePreset(StreamId stream) const = 0;

    /// Apply a named preset to a stream (bundled or user). Invalid streams are
    /// ignored.
    /// Falls back to Flat if preset not found.
    virtual void applyPreset(StreamId stream, const QString& presetName) = 0;

    /// Set a single band gain on a stream (clears active preset name). Invalid
    /// streams are ignored.
    virtual void setGain(StreamId stream, int band, float dB) = 0;

    /// Get a single band gain for a stream (0 for invalid input)
    virtual float gain(StreamId stream, int band) const = 0;

    /// Get all 10 band gains for a stream (all zero for an invalid stream)
    virtual std::array<float, kNumBands> gainsForStream(StreamId stream) const = 0;

    /// Set bypass state for a stream. Invalid streams are ignored.
    virtual void setBypassed(StreamId stream, bool bypassed) = 0;

    /// Get bypass state for a stream (false for an invalid stream)
    virtual bool isBypassed(StreamId stream) const = 0;

    /// Get list of bundled preset names
    virtual QStringList bundledPresetNames() const = 0;

    /// Get list of user preset names
    virtual QStringList userPresetNames() const = 0;

    /// Save current gains from a stream as a user preset
    /// @param source Stream to copy gains from
    /// @param name   Preset name (auto-generated if empty)
    /// @return The name used, or empty string if rejected (invalid stream or
    /// name collision with bundled)
    virtual QString saveUserPreset(StreamId source, const QString& name = {}) = 0;

    /// Delete a user preset. Streams using it revert to Flat.
    /// @return true if preset existed and was deleted
    virtual bool deleteUserPreset(const QString& name) = 0;

    /// Rename a user preset. Names are case-sensitive. Empty/whitespace-only,
    /// bundled, and duplicate target names are rejected. Renaming to the exact
    /// current name is a successful no-op.
    /// @return true if successful, false if the old name is not found or the
    /// target is rejected
    virtual bool renameUserPreset(const QString& oldName, const QString& newName) = 0;
};

} // namespace oap

#endif // OAP_IEQUALIZER_SERVICE_HPP
