#ifndef OAP_EQUALIZER_PRESETS_HPP
#define OAP_EQUALIZER_PRESETS_HPP

#include <array>
#include <cstring>
#include "core/audio/BiquadFilter.hpp"

namespace oap {

/// A named equalizer preset with gain values for each band
struct EqPreset {
    const char* name;
    std::array<float, kNumBands> gains;
};

/// 9 bundled (immutable) equalizer presets
static constexpr EqPreset kBundledPresets[] = {
    {"Flat",         { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0}},
    {"Rock",         { 4,  3, -1, -2,  0,  2,  4,  5,  4,  3}},
    {"Pop",          {-1,  1,  3,  4,  3,  0, -1, -1,  1,  2}},
    {"Jazz",         { 2,  1,  0,  1, -1, -1,  0,  1,  2,  2}},
    {"Classical",    { 0,  0,  0,  0,  0,  0, -1, -2, -1,  1}},
    {"Bass Boost",   { 6,  5,  4,  2,  0,  0,  0,  0,  0,  0}},
    {"Treble Boost", { 0,  0,  0,  0,  0,  1,  2,  4,  5,  6}},
    {"Vocal",        {-2, -1,  0,  1,  3,  3,  2,  1,  0, -1}},
    {"Voice",        {-3, -2, -1,  0,  1,  3,  4,  3,  0, -1}},
};

/// Number of bundled presets
constexpr int kBundledPresetCount = sizeof(kBundledPresets) / sizeof(kBundledPresets[0]);

/// Default preset indices
constexpr int kDefaultMediaPresetIndex = 0;  // Flat
constexpr int kDefaultVoicePresetIndex = 8;  // Voice

/// Find a bundled preset by name (linear search, case-sensitive)
/// @return Pointer to preset, or nullptr if not found
inline const EqPreset* findBundledPreset(const char* name)
{
    for (int i = 0; i < kBundledPresetCount; ++i) {
        if (std::strcmp(kBundledPresets[i].name, name) == 0) {
            return &kBundledPresets[i];
        }
    }
    return nullptr;
}

} // namespace oap

#endif // OAP_EQUALIZER_PRESETS_HPP
