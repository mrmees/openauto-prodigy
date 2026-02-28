#include <oaa/Messenger/ProtocolLogger.hpp>
#include <oaa/Messenger/Messenger.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace oaa {

namespace {

bool isAVMediaFrame(uint8_t channelId, uint16_t messageId)
{
    if (messageId != AVMessageId::AV_MEDIA_WITH_TIMESTAMP
        && messageId != AVMessageId::AV_MEDIA_INDICATION) {
        return false;
    }

    return channelId == ChannelId::Video
        || channelId == ChannelId::MediaAudio
        || channelId == ChannelId::SpeechAudio
        || channelId == ChannelId::SystemAudio
        || channelId == ChannelId::AVInput;
}

std::string hexNoSpaces(const uint8_t* payload, size_t payloadSize)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (size_t i = 0; i < payloadSize; ++i) {
        out << std::setw(2) << static_cast<int>(payload[i]);
    }
    return out.str();
}

std::string jsonEscape(const std::string& input)
{
    std::ostringstream out;
    for (char c : input) {
        switch (c) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                out << "\\u" << std::hex << std::setfill('0') << std::setw(4)
                    << static_cast<int>(static_cast<unsigned char>(c));
            } else {
                out << c;
            }
            break;
        }
    }
    return out.str();
}

} // namespace

ProtocolLogger::ProtocolLogger(QObject* parent)
    : QObject(parent)
{
}

ProtocolLogger::~ProtocolLogger()
{
    detach();
    close();
}

void ProtocolLogger::open(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (open_) file_.close();
    file_.open(path, std::ios::trunc);
    startTime_ = std::chrono::steady_clock::now();
    open_ = file_.is_open();
    if (open_ && format_ == OutputFormat::Tsv) {
        file_ << "TIME\tDIR\tCHANNEL\tMESSAGE\tSIZE\tPAYLOAD_PREVIEW\n";
        file_.flush();
    }
}

void ProtocolLogger::close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (open_) { file_.close(); open_ = false; }
}

bool ProtocolLogger::isOpen() const
{
    return open_;
}

void ProtocolLogger::setFormat(OutputFormat format)
{
    std::lock_guard<std::mutex> lock(mutex_);
    format_ = format;
}

ProtocolLogger::OutputFormat ProtocolLogger::format() const
{
    return format_;
}

void ProtocolLogger::setIncludeMedia(bool includeMedia)
{
    std::lock_guard<std::mutex> lock(mutex_);
    includeMedia_ = includeMedia;
}

bool ProtocolLogger::includeMedia() const
{
    return includeMedia_;
}

void ProtocolLogger::attach(Messenger* messenger)
{
    detach();
    messenger_ = messenger;
    if (!messenger_) return;

    connect(messenger_, &Messenger::messageReceived,
            this, [this](uint8_t ch, uint16_t msgId, const QByteArray& payload, int dataOffset) {
                log("Phone->HU", ch, msgId,
                    reinterpret_cast<const uint8_t*>(payload.constData() + dataOffset),
                    payload.size() - dataOffset);
            });
    connect(messenger_, &Messenger::messageSent,
            this, [this](uint8_t ch, uint16_t msgId, const QByteArray& payload) {
                log("HU->Phone", ch, msgId,
                    reinterpret_cast<const uint8_t*>(payload.constData()),
                    payload.size());
            });
}

void ProtocolLogger::detach()
{
    if (messenger_) {
        disconnect(messenger_, nullptr, this, nullptr);
        messenger_ = nullptr;
    }
}

void ProtocolLogger::log(const std::string& direction,
                          uint8_t channelId, uint16_t messageId,
                          const uint8_t* payload, size_t payloadSize)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!open_) return;

    const bool isDataMsg = isAVMediaFrame(channelId, messageId);
    if (!includeMedia_ && isDataMsg) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - startTime_).count();

    if (format_ == OutputFormat::Jsonl) {
        std::ostringstream line;
        line << "{\"ts_ms\":" << elapsedMs
             << ",\"direction\":\"" << jsonEscape(direction) << "\""
             << ",\"channel_id\":" << static_cast<int>(channelId)
             << ",\"message_id\":" << messageId
             << ",\"message_name\":\"" << jsonEscape(messageName(channelId, messageId)) << "\""
             << ",\"payload_hex\":\"" << hexNoSpaces(payload, payloadSize) << "\"}\n";
        file_ << line.str();
        file_.flush();
        return;
    }

    const auto elapsed = std::chrono::duration<double>(now - startTime_).count();

    // Hex preview of first 64 payload bytes
    std::ostringstream hex;
    const size_t previewMax = 64;

    if (isDataMsg) {
        hex << "[" << (channelId == ChannelId::Video ? "video" : "audio")
            << " data]";
    } else if (payloadSize > 0) {
        size_t previewLen = std::min(payloadSize, previewMax);
        for (size_t i = 0; i < previewLen; ++i) {
            if (i > 0) hex << ' ';
            hex << std::hex << std::setfill('0') << std::setw(2)
                << static_cast<int>(payload[i]);
        }
        if (payloadSize > previewMax) hex << "...";
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

std::string ProtocolLogger::channelName(uint8_t id)
{
    switch (id) {
        case ChannelId::Control:      return "CONTROL";
        case ChannelId::Input:        return "INPUT";
        case ChannelId::Sensor:       return "SENSOR";
        case ChannelId::Video:        return "VIDEO";
        case ChannelId::MediaAudio:   return "MEDIA_AUDIO";
        case ChannelId::SpeechAudio:  return "SPEECH_AUDIO";
        case ChannelId::SystemAudio:  return "SYSTEM_AUDIO";
        case ChannelId::AVInput:      return "AV_INPUT";
        case ChannelId::Bluetooth:    return "BLUETOOTH";
        case ChannelId::WiFi:         return "WIFI";
        default:                      return "UNKNOWN(" + std::to_string(id) + ")";
    }
}

std::string ProtocolLogger::messageName(uint8_t channelId, uint16_t msgId)
{
    // Universal messages (appear on any channel)
    if (msgId == 0x0007) return "CHANNEL_OPEN_REQUEST";
    if (msgId == 0x0008) return "CHANNEL_OPEN_RESPONSE";

    if (channelId == ChannelId::Control) {
        switch (msgId) {
            case 0x0001: return "VERSION_REQUEST";
            case 0x0002: return "VERSION_RESPONSE";
            case 0x0003: return "SSL_HANDSHAKE";
            case 0x0004: return "AUTH_COMPLETE";
            case 0x0005: return "SERVICE_DISCOVERY_REQUEST";
            case 0x0006: return "SERVICE_DISCOVERY_RESPONSE";
            // 0x0007/0x0008 handled as universal messages above
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

    // AV channels
    if (channelId == ChannelId::Video || channelId == ChannelId::MediaAudio
        || channelId == ChannelId::SpeechAudio || channelId == ChannelId::SystemAudio
        || channelId == ChannelId::AVInput) {
        switch (msgId) {
            case AVMessageId::AV_MEDIA_WITH_TIMESTAMP: return "AV_MEDIA_WITH_TIMESTAMP";
            case AVMessageId::AV_MEDIA_INDICATION:     return "AV_MEDIA_INDICATION";
            case AVMessageId::SETUP_REQUEST:           return "AV_SETUP_REQUEST";
            case AVMessageId::START_INDICATION:        return "AV_START_INDICATION";
            case AVMessageId::STOP_INDICATION:         return "AV_STOP_INDICATION";
            case AVMessageId::SETUP_RESPONSE:          return "AV_SETUP_RESPONSE";
            case AVMessageId::ACK_INDICATION:          return "AV_MEDIA_ACK";
            case AVMessageId::INPUT_OPEN_REQUEST:      return "AV_INPUT_OPEN_REQUEST";
            case AVMessageId::INPUT_OPEN_RESPONSE:     return "AV_INPUT_OPEN_RESPONSE";
            case AVMessageId::VIDEO_FOCUS_REQUEST:     return "VIDEO_FOCUS_REQUEST";
            case AVMessageId::VIDEO_FOCUS_INDICATION:  return "VIDEO_FOCUS_INDICATION";
            default: break;
        }
    }

    if (channelId == ChannelId::Input) {
        switch (msgId) {
            case InputMessageId::INPUT_EVENT_INDICATION: return "INPUT_EVENT_INDICATION";
            case InputMessageId::BINDING_REQUEST:        return "BINDING_REQUEST";
            case InputMessageId::BINDING_RESPONSE:       return "BINDING_RESPONSE";
            default: break;
        }
    }

    if (channelId == ChannelId::Sensor) {
        switch (msgId) {
            case SensorMessageId::SENSOR_START_REQUEST:    return "SENSOR_START_REQUEST";
            case SensorMessageId::SENSOR_START_RESPONSE:   return "SENSOR_START_RESPONSE";
            case SensorMessageId::SENSOR_EVENT_INDICATION: return "SENSOR_EVENT_INDICATION";
            default: break;
        }
    }

    if (channelId == ChannelId::Bluetooth) {
        switch (msgId) {
            case BluetoothMessageId::PAIRING_REQUEST:  return "BT_PAIRING_REQUEST";
            case BluetoothMessageId::PAIRING_RESPONSE: return "BT_PAIRING_RESPONSE";
            case BluetoothMessageId::AUTH_DATA:        return "BT_AUTH_DATA";
            default: break;
        }
    }

    if (channelId == ChannelId::WiFi) {
        switch (msgId) {
            case WiFiMessageId::CREDENTIALS_REQUEST:  return "WIFI_CREDENTIALS_REQUEST";
            case WiFiMessageId::CREDENTIALS_RESPONSE: return "WIFI_CREDENTIALS_RESPONSE";
            default: break;
        }
    }

    std::ostringstream oss;
    oss << "0x" << std::hex << std::setfill('0') << std::setw(4) << msgId;
    return oss.str();
}

} // namespace oaa
