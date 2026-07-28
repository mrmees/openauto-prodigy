#pragma once

#include <cstdint>

namespace oaa {

struct ProtocolVersion {
    uint16_t major = 1;
    uint16_t minor = 7;

    constexpr bool operator==(const ProtocolVersion& other) const
    {
        return major == other.major && minor == other.minor;
    }

    constexpr bool operator!=(const ProtocolVersion& other) const
    {
        return !(*this == other);
    }

    constexpr bool operator<(const ProtocolVersion& other) const
    {
        return major < other.major
            || (major == other.major && minor < other.minor);
    }

    constexpr bool operator>=(const ProtocolVersion& other) const
    {
        return !(*this < other);
    }
};

inline constexpr ProtocolVersion kGalVersion1_7{1, 7};
inline constexpr ProtocolVersion kGalVersion4_3{4, 3};
inline constexpr ProtocolVersion kGalVersion5_0{5, 0};
inline constexpr ProtocolVersion kGalVersion5_1{5, 1};
inline constexpr ProtocolVersion kGalVersion6_0{6, 0};

class SessionProtocolPolicy {
public:
    explicit constexpr SessionProtocolPolicy(
        ProtocolVersion requested = kGalVersion1_7)
        : requested_(requested)
    {
    }

    constexpr ProtocolVersion requestedVersion() const
    {
        return requested_;
    }

    constexpr bool atLeast(ProtocolVersion threshold) const
    {
        return requested_ >= threshold;
    }

    constexpr bool requiresMinimumCompatibleResponse() const
    {
        return atLeast(kGalVersion4_3);
    }

    constexpr bool usesModernDisplayPolicy() const
    {
        return atLeast(kGalVersion4_3);
    }

    constexpr bool usesAcklessAudio() const
    {
        return atLeast(kGalVersion5_0);
    }

    constexpr bool requiresSingleVideoCodecPerDisplay() const
    {
        return atLeast(kGalVersion5_0);
    }

    constexpr bool acceptsAudioMediaOptions() const
    {
        return atLeast(kGalVersion5_1);
    }

    constexpr bool acceptsVehicleEnergyForecast() const
    {
        return atLeast(kGalVersion5_1);
    }

    constexpr bool acceptsVideoMediaOptions() const
    {
        return atLeast(kGalVersion6_0);
    }

private:
    ProtocolVersion requested_;
};

} // namespace oaa
