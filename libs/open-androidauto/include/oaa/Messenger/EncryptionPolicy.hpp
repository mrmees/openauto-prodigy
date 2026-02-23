#pragma once

#include <cstdint>

namespace oaa {

class EncryptionPolicy {
public:
    bool shouldEncrypt(uint8_t channelId, uint16_t messageId, bool sslActive) const;
};

} // namespace oaa
