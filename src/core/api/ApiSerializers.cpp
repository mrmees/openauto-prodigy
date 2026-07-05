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
                                                  oap::BluetoothManager* bt) {
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

    return status;
}

} // namespace oap::api::serial
