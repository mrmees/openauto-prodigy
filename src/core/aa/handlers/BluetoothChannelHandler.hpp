#pragma once

#include <oaa/Channel/IChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>

namespace oap {
namespace aa {

class BluetoothChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    explicit BluetoothChannelHandler(QObject* parent = nullptr);

    uint8_t channelId() const override { return oaa::ChannelId::Bluetooth; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload) override;

signals:
    void pairingRequested(const QString& phoneAddress);

private:
    void handlePairingRequest(const QByteArray& payload);
    bool channelOpen_ = false;
};

} // namespace aa
} // namespace oap
