#pragma once
#include <cstdint>

namespace oaa {

/// AV channel messages (Video, Audio, AVInput)
namespace AVMessageId {
    constexpr uint16_t AV_MEDIA_WITH_TIMESTAMP = 0x0000;
    constexpr uint16_t AV_MEDIA_INDICATION     = 0x0001;
    constexpr uint16_t SETUP_REQUEST           = 0x8000;
    constexpr uint16_t START_INDICATION        = 0x8001;
    constexpr uint16_t STOP_INDICATION         = 0x8002;
    constexpr uint16_t SETUP_RESPONSE          = 0x8003;
    constexpr uint16_t ACK_INDICATION          = 0x8004;
    constexpr uint16_t INPUT_OPEN_REQUEST      = 0x8005;
    constexpr uint16_t INPUT_OPEN_RESPONSE     = 0x8006;
    constexpr uint16_t VIDEO_FOCUS_REQUEST     = 0x8007;
    constexpr uint16_t VIDEO_FOCUS_INDICATION  = 0x8008;
    constexpr uint16_t VIDEO_FOCUS_NOTIFICATION       = 0x8009;
    constexpr uint16_t UPDATE_UI_CONFIG_REQUEST        = 0x800A;
    constexpr uint16_t UPDATE_UI_CONFIG_REPLY          = 0x800B;
    constexpr uint16_t AUDIO_UNDERFLOW                 = 0x800C;
    constexpr uint16_t ACTION_TAKEN                    = 0x800D;
    constexpr uint16_t OVERLAY_PARAMETERS              = 0x800E;
    constexpr uint16_t OVERLAY_START                   = 0x800F;
    constexpr uint16_t OVERLAY_STOP                    = 0x8010;
    constexpr uint16_t OVERLAY_SESSION_UPDATE          = 0x8011;
    constexpr uint16_t UPDATE_HU_UI_CONFIG_REQUEST     = 0x8012;
    constexpr uint16_t UPDATE_HU_UI_CONFIG_RESPONSE    = 0x8013;
    constexpr uint16_t MEDIA_STATS                     = 0x8014;
    constexpr uint16_t MEDIA_OPTIONS                   = 0x8015;
    constexpr uint16_t AUDIO_FOCUS_STATE               = 0x8021;
    constexpr uint16_t AUDIO_STREAM_TYPE               = 0x8022;
}

/// Input channel messages
namespace InputMessageId {
    constexpr uint16_t INPUT_EVENT_INDICATION  = 0x8001;
    constexpr uint16_t BINDING_REQUEST         = 0x8002;
    constexpr uint16_t BINDING_RESPONSE        = 0x8003;
    constexpr uint16_t BINDING_NOTIFICATION    = 0x8004; // Provisional — 0x8004 on Input channel (no collision, channel-scoped)
}

/// Sensor channel messages
namespace SensorMessageId {
    constexpr uint16_t SENSOR_START_REQUEST    = 0x8001;
    constexpr uint16_t SENSOR_START_RESPONSE   = 0x8002;
    constexpr uint16_t SENSOR_EVENT_INDICATION = 0x8003;
}

/// Bluetooth channel messages
namespace BluetoothMessageId {
    constexpr uint16_t PAIRING_REQUEST         = 0x8001;
    constexpr uint16_t PAIRING_RESPONSE        = 0x8002;
    constexpr uint16_t AUTH_DATA               = 0x8003;
    constexpr uint16_t AUTH_RESULT             = 0x8004; // Provisional — no proto schema, raw payload only
}

/// Navigation channel messages
namespace NavigationMessageId {
    constexpr uint16_t NAV_STATE            = 0x8003;
    constexpr uint16_t NAV_TURN_EVENT       = 0x8004;
    // 0x8005 was NAV_FOCUS_INDICATION — retracted (nav focus is on Control channel, msgs 13/14)
    constexpr uint16_t NAV_STEP             = 0x8006;
    constexpr uint16_t NAV_DISTANCE         = 0x8007;
}

/// Media status channel messages (CAR.GAL.INST, GAL type 11)
namespace MediaStatusMessageId {
    constexpr uint16_t PLAYBACK_STATUS       = 0x8001; // Phone → HU
    constexpr uint16_t PLAYBACK_STATUS_EVENT = 0x8002; // HU → Phone (instrument cluster d-pad input)
    constexpr uint16_t PLAYBACK_METADATA     = 0x8003; // Phone → HU
}

/// Phone status channel messages
namespace PhoneStatusMessageId {
    constexpr uint16_t PHONE_STATUS       = 0x8001;
}

/// WiFi channel messages
namespace WiFiMessageId {
    constexpr uint16_t CREDENTIALS_REQUEST     = 0x8001;
    constexpr uint16_t CREDENTIALS_RESPONSE    = 0x8002;
    constexpr uint16_t SECURITY_REQUEST        = 0x8001;
    constexpr uint16_t SECURITY_RESPONSE       = 0x8002;
}

} // namespace oaa
