#pragma once

#include <cstdint>

namespace oaa {

enum class FrameType : uint8_t {
    Middle = 0x00,
    First  = 0x01,
    Last   = 0x02,
    Bulk   = 0x03
};

enum class EncryptionType : uint8_t {
    Plain     = 0x00,
    Encrypted = 0x08
};

enum class MessageType : uint8_t {
    Specific = 0x00,
    Control  = 0x04
};

} // namespace oaa
