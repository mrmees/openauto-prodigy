#pragma once
#include <cstdint>

namespace oaa {
namespace ChannelId {
    constexpr uint8_t Control      = 0;
    constexpr uint8_t Video        = 1;
    constexpr uint8_t Input        = 2;
    constexpr uint8_t MediaAudio   = 3;
    constexpr uint8_t SpeechAudio  = 4;
    constexpr uint8_t SystemAudio  = 5;
    constexpr uint8_t Sensor       = 6;
    constexpr uint8_t Bluetooth    = 7;
    constexpr uint8_t WiFi         = 8;
    constexpr uint8_t AVInput      = 10;
} // namespace ChannelId
} // namespace oaa
