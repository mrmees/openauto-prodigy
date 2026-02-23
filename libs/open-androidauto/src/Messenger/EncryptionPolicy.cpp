#include <oaa/Messenger/EncryptionPolicy.hpp>

namespace oaa {

bool EncryptionPolicy::shouldEncrypt(uint8_t channelId, uint16_t messageId, bool sslActive) const
{
    if (!sslActive) {
        return false;
    }

    // Control channel exceptions — these messages are always plaintext
    if (channelId == 0) {
        switch (messageId) {
            case 0x0001: // VERSION_REQUEST
            case 0x0002: // VERSION_RESPONSE
            case 0x0003: // SSL_HANDSHAKE
            case 0x0004: // AUTH_COMPLETE
            case 0x000b: // PING_REQUEST
            case 0x000c: // PING_RESPONSE
                return false;
            default:
                break;
        }
    }

    return true;
}

} // namespace oaa
