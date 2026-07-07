#include "core/api/ApiSerializers.hpp"

namespace oap::api::serial {

namespace pb = prodigy::api::v1;

namespace {

// Active theme palette, exposed verbatim over the wire as
// "--prodigy-<token>" CSS custom properties by the web runtime. Order does
// not matter (destination is a map), but this is the full and exact set --
// see design doc §8.
static const char* kThemeTokens[] = {
    "primary","on-primary","primary-container","on-primary-container",
    "secondary","on-secondary","secondary-container","on-secondary-container",
    "tertiary","on-tertiary","tertiary-container","on-tertiary-container",
    "error","on-error","error-container","on-error-container",
    "background","on-background","surface","on-surface",
    "surface-variant","on-surface-variant","surface-dim","surface-bright",
    "surface-container-lowest","surface-container-low","surface-container",
    "surface-container-high","surface-container-highest",
    "outline","outline-variant",
    "inverse-surface","inverse-on-surface","inverse-primary",
    "scrim","shadow",
    "success","on-success","surface-tint-high","surface-tint-highest",
    "warning","on-warning",
};

// Call-state normalization -- HFP-derived raw int (oap::ICallStateProvider::
// CallState) -> wire CallState. Explicit switch, never a static_cast: the
// two enums' numeric values intentionally diverge (Idle has no wire
// counterpart; Active/Dialing/Alerting are reordered), see
// ICallStateProvider.hpp and phone.proto for each enum's own ordering.
pb::CallState mapCallState(int s) {
    using PS = oap::ICallStateProvider;
    switch (s) {
    case PS::Ringing:  return pb::CALL_STATE_INCOMING;
    case PS::Dialing:  return pb::CALL_STATE_DIALING;
    case PS::Alerting: return pb::CALL_STATE_ALERTING;
    case PS::Active:   return pb::CALL_STATE_ACTIVE;
    case PS::Held:     return pb::CALL_STATE_HELD;
    case PS::Waiting:  return pb::CALL_STATE_WAITING;
    default:           return pb::CALL_STATE_UNSPECIFIED;
    }
}

// Maneuver normalization -- raw AA maneuver code (INavigationProvider::
// maneuverType(), the oaa ManeuverTypeEnum.proto list) -> (ManeuverType,
// TurnSide) pair. One switch, normative table in the Task 7 brief. The
// maneuver code is the PRIMARY side source; when it yields
// TURN_SIDE_UNSPECIFIED, buildNavigationStatus() falls back to
// mapTurnDirectionFallback(p.turnDirection()) below (design doc §8.2,
// DECIDED 2026-07-06 after Codex review of PR #12 -- hybrid, primary wins
// whenever it encodes a side).
struct ManeuverMapping {
    pb::ManeuverType type;
    pb::TurnSide side;
};

ManeuverMapping mapManeuver(int raw) {
    const pb::TurnSide oddLeftEvenRight =
        (raw % 2 == 1) ? pb::TURN_SIDE_LEFT : pb::TURN_SIDE_RIGHT;

    switch (raw) {
    case 0:  return {pb::MANEUVER_TYPE_UNSPECIFIED, pb::TURN_SIDE_UNSPECIFIED};
    case 1:  return {pb::MANEUVER_TYPE_DEPART, pb::TURN_SIDE_UNSPECIFIED};
    case 2:  return {pb::MANEUVER_TYPE_NAME_CHANGE, pb::TURN_SIDE_UNSPECIFIED};
    case 3:  return {pb::MANEUVER_TYPE_KEEP, pb::TURN_SIDE_LEFT};
    case 4:  return {pb::MANEUVER_TYPE_KEEP, pb::TURN_SIDE_RIGHT};
    case 5:  return {pb::MANEUVER_TYPE_SLIGHT_TURN, pb::TURN_SIDE_LEFT};
    case 6:  return {pb::MANEUVER_TYPE_SLIGHT_TURN, pb::TURN_SIDE_RIGHT};
    case 7:  return {pb::MANEUVER_TYPE_TURN, pb::TURN_SIDE_LEFT};
    case 8:  return {pb::MANEUVER_TYPE_TURN, pb::TURN_SIDE_RIGHT};
    case 9:  return {pb::MANEUVER_TYPE_SHARP_TURN, pb::TURN_SIDE_LEFT};
    case 10: return {pb::MANEUVER_TYPE_SHARP_TURN, pb::TURN_SIDE_RIGHT};
    case 11: return {pb::MANEUVER_TYPE_U_TURN, pb::TURN_SIDE_LEFT};
    case 12: return {pb::MANEUVER_TYPE_U_TURN, pb::TURN_SIDE_RIGHT};
    case 13: case 14: case 15: case 16:
    case 17: case 18: case 19: case 20:
        return {pb::MANEUVER_TYPE_ON_RAMP, oddLeftEvenRight};
    case 21: case 22: case 23: case 24:
        return {pb::MANEUVER_TYPE_OFF_RAMP, oddLeftEvenRight};
    case 25: return {pb::MANEUVER_TYPE_FORK, pb::TURN_SIDE_LEFT};
    case 26: return {pb::MANEUVER_TYPE_FORK, pb::TURN_SIDE_RIGHT};
    case 27: return {pb::MANEUVER_TYPE_MERGE, pb::TURN_SIDE_LEFT};
    case 28: return {pb::MANEUVER_TYPE_MERGE, pb::TURN_SIDE_RIGHT};
    case 29: return {pb::MANEUVER_TYPE_MERGE, pb::TURN_SIDE_UNSPECIFIED};
    case 32: case 33: case 34: case 35:
        return {pb::MANEUVER_TYPE_ROUNDABOUT_ENTER_AND_EXIT, pb::TURN_SIDE_UNSPECIFIED};
    case 36: return {pb::MANEUVER_TYPE_STRAIGHT, pb::TURN_SIDE_UNSPECIFIED};
    case 37: case 38:
        return {pb::MANEUVER_TYPE_FERRY, pb::TURN_SIDE_UNSPECIFIED};
    case 39: case 40:
        return {pb::MANEUVER_TYPE_DESTINATION, pb::TURN_SIDE_UNSPECIFIED};
    case 41: return {pb::MANEUVER_TYPE_DESTINATION, pb::TURN_SIDE_LEFT};
    case 42: return {pb::MANEUVER_TYPE_DESTINATION, pb::TURN_SIDE_RIGHT};
    case 43: case 45:
        return {pb::MANEUVER_TYPE_ROUNDABOUT_ENTER, pb::TURN_SIDE_UNSPECIFIED};
    case 44: case 46:
        return {pb::MANEUVER_TYPE_ROUNDABOUT_EXIT, pb::TURN_SIDE_UNSPECIFIED};
    case 47: case 49:
        return {pb::MANEUVER_TYPE_FERRY, pb::TURN_SIDE_LEFT};
    case 48: case 50:
        return {pb::MANEUVER_TYPE_FERRY, pb::TURN_SIDE_RIGHT};
    default:
        return {pb::MANEUVER_TYPE_OTHER, pb::TURN_SIDE_UNSPECIFIED};
    }
}

// Turn-side fallback -- raw AA turn_direction int (INavigationProvider::
// turnDirection(), sourced from NavigationTurnEvent.turn_direction, an
// oaa.proto.enums.TurnSide.Enum: UNKNOWN=0, LEFT=1, RIGHT=2 --
// libs/prodigy-oaa-protocol/proto/oaa/navigation/TurnSideEnum.proto,
// forwarded unchanged as static_cast<int> by
// NavigationChannelHandler::handleTurnEvent) -> wire TurnSide. Used ONLY
// when mapManeuver()'s code-derived side is TURN_SIDE_UNSPECIFIED -- the
// maneuver code always wins when it has an opinion.
pb::TurnSide mapTurnDirectionFallback(int raw) {
    switch (raw) {
    case 1:  return pb::TURN_SIDE_LEFT;
    case 2:  return pb::TURN_SIDE_RIGHT;
    default: return pb::TURN_SIDE_UNSPECIFIED;
    }
}

} // namespace

prodigy::api::v1::MediaStatus buildMediaStatus(const oap::IMediaStatusProvider& p) {
    pb::MediaStatus status;
    status.set_has_media(p.hasMedia());
    status.set_title(p.title().toStdString());
    status.set_artist(p.artist().toStdString());
    status.set_album(p.album().toStdString());
    status.set_app_name(p.appName().toStdString());

    // Source normalization (design doc §8): raw source() string -> MediaSource.
    const QString sourceStr = p.source();
    pb::MediaSource source;
    if (sourceStr.isEmpty()) {
        source = pb::MEDIA_SOURCE_NONE;
    } else if (sourceStr == QStringLiteral("Bluetooth")) {
        source = pb::MEDIA_SOURCE_BLUETOOTH;
    } else if (sourceStr == QStringLiteral("AndroidAuto")) {
        source = pb::MEDIA_SOURCE_ANDROID_AUTO;
    } else {
        source = pb::MEDIA_SOURCE_UNSPECIFIED;
    }
    status.set_source(source);

    // Playback state normalization is SOURCE-DEPENDENT: the same raw int
    // means a different state depending on which source produced it.
    // Bluetooth (BtAudioPlugin.hpp:50-53): 0=Stopped, 1=Playing, 2=Paused.
    // AndroidAuto (MediaStatusChannelHandler.hpp:20-23): 1=Stopped,
    // 2=Playing, 3=Paused. Never static_cast between the two.
    const int raw = p.playbackState();
    pb::PlaybackState playback = pb::PLAYBACK_STATE_UNSPECIFIED;
    switch (source) {
    case pb::MEDIA_SOURCE_BLUETOOTH:
        switch (raw) {
        case 0: playback = pb::PLAYBACK_STATE_STOPPED; break;
        case 1: playback = pb::PLAYBACK_STATE_PLAYING; break;
        case 2: playback = pb::PLAYBACK_STATE_PAUSED; break;
        default: playback = pb::PLAYBACK_STATE_UNSPECIFIED; break;
        }
        break;
    case pb::MEDIA_SOURCE_ANDROID_AUTO:
        switch (raw) {
        case 1: playback = pb::PLAYBACK_STATE_STOPPED; break;
        case 2: playback = pb::PLAYBACK_STATE_PLAYING; break;
        case 3: playback = pb::PLAYBACK_STATE_PAUSED; break;
        default: playback = pb::PLAYBACK_STATE_UNSPECIFIED; break;
        }
        break;
    case pb::MEDIA_SOURCE_NONE:
    case pb::MEDIA_SOURCE_UNSPECIFIED:
    default:
        playback = pb::PLAYBACK_STATE_UNSPECIFIED;
        break;
    }
    status.set_playback_state(playback);

    return status;
}

prodigy::api::v1::ProjectionStatus buildProjectionStatus(const oap::IProjectionStatusProvider& p) {
    pb::ProjectionStatus status;
    status.set_status_message(p.statusMessage().toStdString());

    // Explicit switch (never a static_cast) -- see IProjectionStatusProvider::ProjectionState.
    pb::ProjectionState state;
    switch (p.projectionState()) {
    case 0: state = pb::PROJECTION_STATE_DISCONNECTED; break;
    case 1: state = pb::PROJECTION_STATE_WAITING_FOR_DEVICE; break;
    case 2: state = pb::PROJECTION_STATE_CONNECTING; break;
    case 3: state = pb::PROJECTION_STATE_PROJECTING; break;
    case 4: state = pb::PROJECTION_STATE_BACKGROUNDED; break;
    default: state = pb::PROJECTION_STATE_UNSPECIFIED; break;
    }
    status.set_state(state);

    return status;
}

prodigy::api::v1::SystemStatus buildSystemStatus(oap::ThemeService& theme,
                                                  const QString& appVersion,
                                                  oap::BluetoothManager* bt,
                                                  const oap::DisplayInfo* display) {
    pb::SystemStatus status;
    status.set_night_mode(theme.realNightMode());
    status.set_theme_id(theme.currentThemeId().toStdString());
    status.set_app_version(appVersion.toStdString());

    auto* tokens = status.mutable_theme_tokens();
    for (const char* name : kThemeTokens) {
        (*tokens)[name] = theme.color(QString::fromUtf8(name)).name().toStdString();
    }

    auto* summary = status.mutable_bluetooth();
    const QString deviceName = (bt != nullptr) ? bt->connectedDeviceName() : QString();
    summary->set_connected(!deviceName.isEmpty());
    summary->set_device_name(deviceName.toStdString());

    // v1.1 additive: absent on servers below v1.1 (feature-detect contract) --
    // only set when a live DisplayInfo was provided.
    if (display != nullptr) {
        status.set_display_width(static_cast<uint32_t>(display->windowWidth()));
        status.set_display_height(static_cast<uint32_t>(display->windowHeight()));
    }

    return status;
}

prodigy::api::v1::PhoneStatus buildPhoneStatus(const oap::IPhoneStateService& p,
                                                qint64 activeCallStartedAtMs) {
    pb::PhoneStatus status;
    status.set_hfp_connected(p.phoneConnected());
    status.set_device_name(p.deviceName().toStdString());

    const int raw = p.callState();
    if (raw != oap::ICallStateProvider::Idle) {
        // v1 is single-call: exactly one Call while not Idle (see file
        // header -- calls[] is repeated for future multi-call backends).
        auto* call = status.add_calls();
        call->set_state(mapCallState(raw));
        call->set_line_identification(p.callerNumber().toStdString());
        call->set_display_name(p.callerName().toStdString());
        if (raw == oap::ICallStateProvider::Active) {
            call->set_started_at_unix_ms(activeCallStartedAtMs);
        }
    }

    // Capabilities never claim more than the provider backs -- the frozen
    // proto contract (design doc §8.4): a flag and a command result must
    // never contradict each other.
    const bool avail = p.telephonyAvailable();
    auto* caps = status.mutable_capabilities();
    caps->set_can_dial(avail);
    caps->set_can_answer(avail);
    caps->set_can_hangup(avail);
    caps->set_can_send_dtmf(avail);
    caps->set_can_hold_swap(false);    // hard-false in v1 -- frozen contract
    caps->set_can_multiparty(false);   // hard-false in v1 -- frozen contract

    return status;
}

prodigy::api::v1::NavigationStatus buildNavigationStatus(const oap::INavigationProvider& p) {
    pb::NavigationStatus status;
    status.set_nav_active(p.navActive());
    status.set_road_name(p.roadName().toStdString());
    status.set_formatted_distance(p.formattedDistance().toStdString());
    status.set_distance_meters(p.distanceMeters());

    const ManeuverMapping m = mapManeuver(p.maneuverType());
    status.set_maneuver(m.type);
    status.set_turn_side(m.side != pb::TURN_SIDE_UNSPECIFIED
                              ? m.side
                              : mapTurnDirectionFallback(p.turnDirection()));

    return status;
}

} // namespace oap::api::serial
