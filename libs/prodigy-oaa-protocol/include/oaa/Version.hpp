#pragma once
#include <cstdint>

namespace oaa {

constexpr uint16_t PROTOCOL_MAJOR = 1;
constexpr uint16_t PROTOCOL_MINOR = 7;

constexpr uint16_t FRAME_MAX_PAYLOAD = 0x4000; // 16384 bytes
constexpr uint16_t FRAME_HEADER_SIZE = 2;
constexpr uint16_t FRAME_SIZE_SHORT = 2;
constexpr uint16_t FRAME_SIZE_EXTENDED = 6;
constexpr int TLS_OVERHEAD = 29;
constexpr int BIO_BUFFER_SIZE = 20480;

} // namespace oaa
