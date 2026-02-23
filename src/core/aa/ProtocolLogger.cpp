#include "ProtocolLogger.hpp"
#include <iomanip>
#include <sstream>

namespace openauto {

ProtocolLogger& ProtocolLogger::instance() {
    static ProtocolLogger inst;
    return inst;
}

void ProtocolLogger::open(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (open_) file_.close();
    file_.open(path, std::ios::trunc);
    startTime_ = std::chrono::steady_clock::now();
    open_ = file_.is_open();
    if (open_) {
        file_ << "TIME\tDIR\tCHANNEL\tMESSAGE\tSIZE\tPAYLOAD_PREVIEW\n";
        file_.flush();
    }
}

void ProtocolLogger::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (open_) { file_.close(); open_ = false; }
}

void ProtocolLogger::log(const std::string& direction,
                          aasdk::messenger::ChannelId channelId,
                          uint16_t messageId,
                          const uint8_t* payload, size_t payloadSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!open_) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - startTime_).count();

    // Hex preview of first 64 payload bytes (skip the 2-byte message ID prefix)
    std::ostringstream hex;
    const size_t previewStart = 2;
    const size_t previewMax = 64;
    bool isDataMsg = (messageId == 0x0000 || messageId == 0x0001) &&
        (channelId == aasdk::messenger::ChannelId::VIDEO ||
         channelId == aasdk::messenger::ChannelId::MEDIA_AUDIO ||
         channelId == aasdk::messenger::ChannelId::SPEECH_AUDIO ||
         channelId == aasdk::messenger::ChannelId::SYSTEM_AUDIO);

    if (isDataMsg) {
        hex << "[" << (channelId == aasdk::messenger::ChannelId::VIDEO ? "video" : "audio")
            << " data]";
    } else if (payloadSize > previewStart) {
        size_t previewLen = std::min(payloadSize - previewStart, previewMax);
        for (size_t i = 0; i < previewLen; ++i) {
            if (i > 0) hex << ' ';
            hex << std::hex << std::setfill('0') << std::setw(2)
                << static_cast<int>(payload[previewStart + i]);
        }
        if (payloadSize - previewStart > previewMax) hex << "...";
    }

    std::ostringstream line;
    line << std::fixed << std::setprecision(3) << elapsed << '\t'
         << direction << '\t'
         << "ch" << static_cast<int>(channelId) << "/" << channelName(channelId) << '\t'
         << messageName(channelId, messageId) << '\t'
         << payloadSize << '\t'
         << hex.str() << '\n';

    file_ << line.str();
    file_.flush();
}

std::string ProtocolLogger::channelName(aasdk::messenger::ChannelId id) {
    using CI = aasdk::messenger::ChannelId;
    switch (id) {
        case CI::CONTROL:      return "CONTROL";
        case CI::INPUT:        return "INPUT";
        case CI::SENSOR:       return "SENSOR";
        case CI::VIDEO:        return "VIDEO";
        case CI::MEDIA_AUDIO:  return "MEDIA_AUDIO";
        case CI::SPEECH_AUDIO: return "SPEECH_AUDIO";
        case CI::SYSTEM_AUDIO: return "SYSTEM_AUDIO";
        case CI::AV_INPUT:     return "AV_INPUT";
        case CI::BLUETOOTH:    return "BLUETOOTH";
        case CI::WIFI:         return "WIFI";
        default:               return "UNKNOWN(" + std::to_string(static_cast<int>(id)) + ")";
    }
}

std::string ProtocolLogger::messageName(aasdk::messenger::ChannelId channelId, uint16_t msgId) {
    using CI = aasdk::messenger::ChannelId;

    if (channelId == CI::CONTROL) {
        switch (msgId) {
            case 0x0001: return "VERSION_REQUEST";
            case 0x0002: return "VERSION_RESPONSE";
            case 0x0003: return "SSL_HANDSHAKE";
            case 0x0004: return "AUTH_COMPLETE";
            case 0x0005: return "SERVICE_DISCOVERY_REQUEST";
            case 0x0006: return "SERVICE_DISCOVERY_RESPONSE";
            case 0x0007: return "CHANNEL_OPEN_REQUEST";
            case 0x0008: return "CHANNEL_OPEN_RESPONSE";
            case 0x000b: return "PING_REQUEST";
            case 0x000c: return "PING_RESPONSE";
            case 0x000d: return "NAVIGATION_FOCUS_REQUEST";
            case 0x000e: return "NAVIGATION_FOCUS_RESPONSE";
            case 0x000f: return "SHUTDOWN_REQUEST";
            case 0x0010: return "SHUTDOWN_RESPONSE";
            case 0x0011: return "VOICE_SESSION_REQUEST";
            case 0x0012: return "AUDIO_FOCUS_REQUEST";
            case 0x0013: return "AUDIO_FOCUS_RESPONSE";
            default: break;
        }
    }

    // AV channels (VIDEO, MEDIA_AUDIO, SPEECH_AUDIO, SYSTEM_AUDIO, AV_INPUT)
    if (channelId == CI::VIDEO || channelId == CI::MEDIA_AUDIO ||
        channelId == CI::SPEECH_AUDIO || channelId == CI::SYSTEM_AUDIO ||
        channelId == CI::AV_INPUT) {
        switch (msgId) {
            case 0x0000: return "AV_MEDIA_WITH_TIMESTAMP";
            case 0x0001: return "AV_MEDIA_INDICATION";
            case 0x8000: return "AV_SETUP_REQUEST";
            case 0x8001: return "AV_START_INDICATION";
            case 0x8002: return "AV_STOP_INDICATION";
            case 0x8003: return "AV_SETUP_RESPONSE";
            case 0x8004: return "AV_MEDIA_ACK";
            case 0x8005: return "AV_INPUT_OPEN_REQUEST";
            case 0x8006: return "AV_INPUT_OPEN_RESPONSE";
            case 0x8007: return "VIDEO_FOCUS_REQUEST";
            case 0x8008: return "VIDEO_FOCUS_INDICATION";
            default: break;
        }
    }

    if (channelId == CI::INPUT) {
        switch (msgId) {
            case 0x8001: return "INPUT_EVENT_INDICATION";
            case 0x8002: return "BINDING_REQUEST";
            case 0x8003: return "BINDING_RESPONSE";
            default: break;
        }
    }

    if (channelId == CI::SENSOR) {
        switch (msgId) {
            case 0x8001: return "SENSOR_START_REQUEST";
            case 0x8002: return "SENSOR_START_RESPONSE";
            case 0x8003: return "SENSOR_EVENT_INDICATION";
            default: break;
        }
    }

    if (channelId == CI::BLUETOOTH) {
        switch (msgId) {
            case 0x8001: return "BT_PAIRING_REQUEST";
            case 0x8002: return "BT_PAIRING_RESPONSE";
            case 0x8003: return "BT_AUTH_DATA";
            default: break;
        }
    }

    if (channelId == CI::WIFI) {
        switch (msgId) {
            case 0x8001: return "WIFI_CREDENTIALS_REQUEST";
            case 0x8002: return "WIFI_CREDENTIALS_RESPONSE";
            default: break;
        }
    }

    std::ostringstream oss;
    oss << "0x" << std::hex << std::setfill('0') << std::setw(4) << msgId;
    return oss.str();
}

} // namespace openauto
