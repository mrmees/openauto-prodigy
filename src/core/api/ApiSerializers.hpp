#pragma once

// Serializers: raw service ints/strings -> External API v1 wire enums.
//
// This is the ONLY place normalization happens. Every raw source-native
// state code (Bluetooth playback ints, AA playback ints, projection state
// ints, ...) is translated here via an explicit switch — never a
// static_cast. Callers pass already-live services; these functions are
// pure (no side effects, no caching) and safe to call on every state
// change.

#include "api/media.pb.h"
#include "api/projection.pb.h"
#include "api/system.pb.h"

#include "core/services/IMediaStatusProvider.hpp"
#include "core/services/IProjectionStatusProvider.hpp"
#include "core/services/ThemeService.hpp"
#include "core/services/BluetoothManager.hpp"

namespace oap::api::serial {

/// Build a MediaStatus snapshot. MediaSource is normalized from the
/// provider's source() string; PlaybackState is normalized from the raw
/// playbackState() int, whose meaning is SOURCE-DEPENDENT (Bluetooth and
/// AndroidAuto use different raw codes for the same state — see
/// ApiSerializers.cpp for the tables).
prodigy::api::v1::MediaStatus buildMediaStatus(const oap::IMediaStatusProvider& p);

/// Build a ProjectionStatus snapshot. ProjectionState is normalized via an
/// explicit switch on p.projectionState() (never a static_cast).
prodigy::api::v1::ProjectionStatus buildProjectionStatus(const oap::IProjectionStatusProvider& p);

/// Build a SystemStatus snapshot: night mode + active theme id/tokens, the
/// caller-composed app version string, and a Bluetooth connection summary.
/// `bt` is nullable (Bluetooth stack unavailable) -- in that case the
/// bluetooth summary reports connected=false with an empty device name.
prodigy::api::v1::SystemStatus buildSystemStatus(oap::ThemeService& theme,
                                                  const QString& appVersion,
                                                  oap::BluetoothManager* bt);

} // namespace oap::api::serial
