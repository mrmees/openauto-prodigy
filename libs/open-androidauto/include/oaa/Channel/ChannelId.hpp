#pragma once
#include <cstdint>

namespace oaa {
namespace ChannelId {
    constexpr uint8_t Control      = 0;
    constexpr uint8_t Input        = 1;
    constexpr uint8_t Sensor       = 2;
    constexpr uint8_t Video        = 3;
    constexpr uint8_t MediaAudio   = 4;
    constexpr uint8_t SpeechAudio  = 5;
    constexpr uint8_t SystemAudio  = 6;
    constexpr uint8_t AVInput      = 7;
    constexpr uint8_t Bluetooth    = 8;
    constexpr uint8_t WiFi         = 14;
} // namespace ChannelId
} // namespace oaa
