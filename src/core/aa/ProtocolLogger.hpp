#pragma once

#include <fstream>
#include <mutex>
#include <chrono>
#include <string>
#include <cstdint>

namespace oap {
namespace aa {

class ProtocolLogger {
public:
    static ProtocolLogger& instance();

    void open(const std::string& path = "/tmp/oap-protocol.log");
    void close();

    // direction: "HU->Phone" or "Phone->HU"
    void log(const std::string& direction,
             uint8_t channelId,
             uint16_t messageId,
             const uint8_t* payload, size_t payloadSize);

    static std::string channelName(uint8_t id);
    static std::string messageName(uint8_t channelId, uint16_t msgId);

private:
    ProtocolLogger() = default;
    std::ofstream file_;
    std::mutex mutex_;
    std::chrono::steady_clock::time_point startTime_;
    bool open_ = false;
};

} // namespace aa
} // namespace oap
