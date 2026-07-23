#pragma once

#include <cstdint>

namespace oaa {

enum class SessionState {
    Idle,
    Connecting,
    VersionExchange,
    TLSHandshake,
    ServiceDiscovery,
    Active,
    ShuttingDown,
    Disconnected
};

enum class DisconnectReason {
    Normal,
    PingTimeout,
    TransportError,
    VersionMismatch,
    Timeout,
    UserRequested,
    HandshakeError,
    TlsError
};

} // namespace oaa
