#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace oap {

// Known system Widevine CDM locations (RPi OS libwidevinecdm0 layouts),
// priority order. Spec: 2026-07-07-web-surface-strategy §Slice 1.2.
QStringList widevineCdmCandidates();

// First candidate that exists on disk, or an empty string.
QString resolveWidevineCdmPath(const QStringList& candidates);

// Value QTWEBENGINE_CHROMIUM_FLAGS should take so Chromium loads the CDM.
// Returns existingFlags unchanged when cdmPath is empty or the flags
// already mention widevine-path (operator override wins).
QByteArray appendWidevineFlag(const QByteArray& existingFlags, const QString& cdmPath);

} // namespace oap
