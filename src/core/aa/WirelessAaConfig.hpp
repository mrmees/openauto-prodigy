#pragma once

#include <QVariant>
#include <QtTypes>

namespace oap::aa {

inline constexpr quint16 kDefaultWirelessAaTcpPort = 5277;

/// Resolve the TCP listener request without narrowing invalid values.
/// An integer zero is reserved for deterministic ephemeral-port tests;
/// textual and out-of-range values fall back to the production default.
quint16 resolveWirelessAaTcpPort(const QVariant& value,
                                 bool* usedFallback = nullptr);

} // namespace oap::aa
