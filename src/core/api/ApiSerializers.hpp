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
#include "api/phone.pb.h"
#include "api/navigation.pb.h"

#include "core/services/IMediaStatusProvider.hpp"
#include "core/services/IProjectionStatusProvider.hpp"
#include "core/services/ThemeService.hpp"
#include "core/services/BluetoothManager.hpp"
#include "core/services/IPhoneStateService.hpp"
#include "core/services/INavigationProvider.hpp"
#include "ui/DisplayInfo.hpp"

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
/// caller-composed app version string, a Bluetooth connection summary, and
/// the head-unit display dimensions. `bt` is nullable (Bluetooth stack
/// unavailable) -- in that case the bluetooth summary reports
/// connected=false with an empty device name. `display` is nullable
/// (feature-detect contract, v1.1): when null, display_width/display_height
/// are left unset; when non-null, they are set from
/// windowWidth()/windowHeight().
prodigy::api::v1::SystemStatus buildSystemStatus(oap::ThemeService& theme,
                                                  const QString& appVersion,
                                                  oap::BluetoothManager* bt,
                                                  const oap::DisplayInfo* display);

/// Build a PhoneStatus snapshot. CallState is normalized from the provider's
/// widened HFP-derived callState() int via an explicit switch (mapCallState
/// in ApiSerializers.cpp) -- Idle produces an empty calls[] list, any other
/// state produces exactly one Call (v1 is single-call). `activeCallStartedAtMs`
/// is echoed onto that Call's started_at_unix_ms ONLY while the call is
/// Active (0 otherwise); the caller captures the timestamp at the moment of
/// the transition into Active (see design doc §8.4) -- this function is pure
/// and does no timekeeping itself. Capabilities (can_dial/can_answer/
/// can_hangup/can_send_dtmf) mirror p.telephonyAvailable() -- the frozen
/// contract is that they never claim availability the provider doesn't back.
/// can_hold_swap/can_multiparty are hard-false in v1, permanently.
prodigy::api::v1::PhoneStatus buildPhoneStatus(const oap::IPhoneStateService& p,
                                                qint64 activeCallStartedAtMs);

/// Build a NavigationStatus snapshot. ManeuverType/TurnSide are normalized
/// from the raw AA maneuver code (p.maneuverType()) via one switch -- see
/// ApiSerializers.cpp for the full table. TurnSide is a hybrid: the
/// maneuver-code table is the primary/authoritative source, and
/// p.turnDirection() (raw AA TurnSide.Enum) is consulted ONLY as a fallback
/// when the code encodes no side (DECIDED 2026-07-06 after Codex review of
/// PR #12; see design doc §8.2). distance_meters is left 0 in this task --
/// Task 13 populates it once the provider interface is promoted to carry
/// raw meters.
prodigy::api::v1::NavigationStatus buildNavigationStatus(const oap::INavigationProvider& p);

} // namespace oap::api::serial
