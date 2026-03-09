#pragma once

#include <QString>

namespace oap {
namespace aa {

/// Map battery percentage (0-100) to Material Symbols Outlined icon codepoint.
/// Returns empty string for -1 or invalid (no data available).
inline QString batteryIconForLevel(int level)
{
    if (level < 0) return {};
    if (level == 0) return QStringLiteral("\ue19c");       // battery_alert
    if (level <= 19) return QStringLiteral("\uebd0");      // battery_1_bar
    if (level <= 39) return QStringLiteral("\uebd2");      // battery_3_bar
    if (level <= 59) return QStringLiteral("\uebd3");      // battery_4_bar
    if (level <= 79) return QStringLiteral("\uebd4");      // battery_5_bar
    if (level <= 95) return QStringLiteral("\uebd5");      // battery_6_bar
    return QStringLiteral("\ue1a4");                        // battery_full (96+)
}

/// Map signal strength (0-4) to Material Symbols Outlined icon codepoint.
/// Returns empty string for -1 or invalid (no data available).
/// Values above 4 are clamped to 4-bar.
inline QString signalIconForStrength(int strength)
{
    if (strength < 0) return {};
    static const char16_t codepoints[] = {
        0xf0a8,  // signal_cellular_0_bar
        0xf0a9,  // signal_cellular_1_bar
        0xf0aa,  // signal_cellular_2_bar
        0xf0ab,  // signal_cellular_3_bar
        0xf0ac,  // signal_cellular_4_bar
    };
    int idx = (strength > 4) ? 4 : strength;
    return QString(QChar(codepoints[idx]));
}

} // namespace aa
} // namespace oap
