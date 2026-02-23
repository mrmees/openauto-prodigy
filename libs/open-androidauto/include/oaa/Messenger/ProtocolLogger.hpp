#pragma once

#include <QObject>
#include <fstream>
#include <mutex>
#include <chrono>
#include <string>
#include <cstdint>

namespace oaa {

class Messenger;

/// Logs AA protocol messages to a TSV file.
/// Attach to a Messenger via attach() — connects to messageReceived and messageSent signals.
class ProtocolLogger : public QObject {
    Q_OBJECT

public:
    explicit ProtocolLogger(QObject* parent = nullptr);
    ~ProtocolLogger() override;

    void open(const std::string& path = "/tmp/oap-protocol.log");
    void close();
    bool isOpen() const;

    /// Connect to a Messenger's signals for automatic logging
    void attach(Messenger* messenger);
    void detach();

    /// Manual log entry (direction: "HU->Phone" or "Phone->HU")
    void log(const std::string& direction,
             uint8_t channelId, uint16_t messageId,
             const uint8_t* payload, size_t payloadSize);

    static std::string channelName(uint8_t id);
    static std::string messageName(uint8_t channelId, uint16_t msgId);

private:
    std::ofstream file_;
    std::mutex mutex_;
    std::chrono::steady_clock::time_point startTime_;
    bool open_ = false;
    Messenger* messenger_ = nullptr;
};

} // namespace oaa
