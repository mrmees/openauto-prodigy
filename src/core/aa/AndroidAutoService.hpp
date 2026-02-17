#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <thread>
#include <vector>
#include <boost/asio.hpp>
#include <libusb.h>

#include "AndroidAutoEntity.hpp"
#include "IService.hpp"
#include "../../core/Configuration.hpp"

namespace aasdk {
namespace usb { class IUSBWrapper; class IAccessoryModeQueryFactory; class IAccessoryModeQueryChainFactory; class IUSBHub; }
namespace tcp { class ITCPWrapper; }
namespace transport { class ISSLWrapper; }
}

namespace oap {
namespace aa {

class AndroidAutoService : public QObject, public IAndroidAutoEntityEventHandler
{
    Q_OBJECT
    Q_PROPERTY(int connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    enum ConnectionState {
        Disconnected = 0,
        WaitingForDevice,
        Connecting,
        Connected
    };
    Q_ENUM(ConnectionState)

    explicit AndroidAutoService(std::shared_ptr<oap::Configuration> config, QObject* parent = nullptr);
    ~AndroidAutoService() override;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    int connectionState() const { return state_; }
    QString statusMessage() const { return statusMessage_; }

    // IAndroidAutoEntityEventHandler
    void onConnected() override;
    void onDisconnected() override;
    void onError(const std::string& message) override;

signals:
    void connectionStateChanged();
    void statusMessageChanged();

private:
    void setState(ConnectionState state, const QString& message);
    void startUSBHub();
    void onUSBDeviceConnected(aasdk::usb::DeviceHandle handle);
    void startTCPListener();
    void onTCPConnection(std::shared_ptr<boost::asio::ip::tcp::socket> socket);
    void startEntity(aasdk::transport::ITransport::Pointer transport);

    std::shared_ptr<oap::Configuration> config_;

    // ASIO
    std::unique_ptr<boost::asio::io_service> ioService_;
    std::unique_ptr<boost::asio::io_service::work> ioWork_;
    std::vector<std::thread> asioThreads_;

    // libusb
    libusb_context* usbContext_ = nullptr;
    std::vector<std::thread> usbThreads_;
    bool usbRunning_ = false;

    // USB detection
    std::shared_ptr<aasdk::usb::IUSBWrapper> usbWrapper_;
    std::shared_ptr<aasdk::usb::IAccessoryModeQueryFactory> queryFactory_;
    std::shared_ptr<aasdk::usb::IAccessoryModeQueryChainFactory> queryChainFactory_;
    std::shared_ptr<aasdk::usb::IUSBHub> usbHub_;

    // TCP listener
    std::shared_ptr<aasdk::tcp::ITCPWrapper> tcpWrapper_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> tcpAcceptor_;

    // SSL
    std::shared_ptr<aasdk::transport::ISSLWrapper> sslWrapper_;

    // Active entity
    AndroidAutoEntity::Pointer entity_;

    ConnectionState state_ = Disconnected;
    QString statusMessage_;
};

} // namespace aa
} // namespace oap
