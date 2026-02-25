#pragma once

#include <oaa/Channel/IChannelHandler.hpp>
#include <oaa/Channel/ChannelId.hpp>
#include <oaa/Channel/MessageIds.hpp>

namespace oaa {
namespace hu {

class NavigationChannelHandler : public oaa::IChannelHandler {
    Q_OBJECT
public:
    explicit NavigationChannelHandler(QObject* parent = nullptr);

    uint8_t channelId() const override { return oaa::ChannelId::Navigation; }
    void onChannelOpened() override;
    void onChannelClosed() override;
    void onMessage(uint16_t messageId, const QByteArray& payload) override;

signals:
    /// Emitted when navigation starts (active=true) or ends (active=false)
    void navigationStateChanged(bool active);

    /// Emitted with turn-by-turn instruction update
    void navigationStepChanged(const QString& instruction, const QString& destination,
                                int maneuverType);

    /// Emitted with distance/ETA update
    void navigationDistanceChanged(const QString& distance, int unit);

private:
    void handleNavState(const QByteArray& payload);
    void handleNavStep(const QByteArray& payload);
    void handleNavDistance(const QByteArray& payload);

    bool navActive_ = false;
};

} // namespace hu
} // namespace oaa
