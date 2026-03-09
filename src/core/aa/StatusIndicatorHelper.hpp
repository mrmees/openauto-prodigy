#pragma once

#include <QString>

namespace oap {
namespace aa {

/// Map battery percentage (0-100) to Material Symbols Outlined icon codepoint.
/// Returns empty string for -1 or invalid (no data available).
inline QString batteryIconForLevel(int level)
{
    Q_UNUSED(level);
    return QString(); // stub
}

/// Map signal strength (0-4) to Material Symbols Outlined icon codepoint.
/// Returns empty string for -1 or invalid (no data available).
inline QString signalIconForStrength(int strength)
{
    Q_UNUSED(strength);
    return QString(); // stub
}

} // namespace aa
} // namespace oap
