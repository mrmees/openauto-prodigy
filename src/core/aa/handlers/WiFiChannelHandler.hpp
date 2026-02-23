#pragma once

#include <oaa/Channel/IChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>
#include <QString>

namespace oap {
namespace aa {

class WiFiChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    explicit WiFiChannelHandler(const QString& ssid = QString(),
                                 const QString& password = QString(),
                                 QObject* parent = nullptr);

    uint8_t channelId() const override { return oaa::ChannelId::WiFi; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload) override;

private:
    void handleSecurityRequest(const QByteArray& payload);

    QString ssid_;
    QString password_;
    bool channelOpen_ = false;
};

} // namespace aa
} // namespace oap
