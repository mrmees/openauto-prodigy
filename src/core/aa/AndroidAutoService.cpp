#include "AndroidAutoService.hpp"
#include "ServiceFactory.hpp"

#include <QMetaObject>
#include <boost/log/trivial.hpp>

#include <aasdk/USB/USBWrapper.hpp>
#include <aasdk/USB/USBHub.hpp>
#include <aasdk/USB/AOAPDevice.hpp>
#include <aasdk/USB/AccessoryModeQueryFactory.hpp>
#include <aasdk/USB/AccessoryModeQueryChainFactory.hpp>
#include <aasdk/TCP/TCPWrapper.hpp>
#include <aasdk/TCP/TCPEndpoint.hpp>
#include <aasdk/Transport/SSLWrapper.hpp>
#include <aasdk/Transport/USBTransport.hpp>
#include <aasdk/Transport/TCPTransport.hpp>
#include <aasdk/Messenger/Cryptor.hpp>
#include <aasdk/Messenger/MessageInStream.hpp>
#include <aasdk/Messenger/MessageOutStream.hpp>
#include <aasdk/Messenger/Messenger.hpp>


namespace oap {
namespace aa {

static constexpr int kAsioThreadCount = 4;
static constexpr int kUsbThreadCount = 2;
static constexpr uint16_t kTCPListenPort = 5277;

AndroidAutoService::AndroidAutoService(
    std::shared_ptr<oap::Configuration> config, QObject* parent)
    : QObject(parent)
    , config_(std::move(config))
{
}

AndroidAutoService::~AndroidAutoService()
{
    stop();
}

void AndroidAutoService::start()
{
    BOOST_LOG_TRIVIAL(info) << "[AAService] Starting Android Auto service";

    setState(WaitingForDevice, "Initializing...");

    // Create ASIO io_service
    ioService_ = std::make_unique<boost::asio::io_service>();
    ioWork_ = std::make_unique<boost::asio::io_service::work>(*ioService_);

    // Start ASIO worker threads
    for (int i = 0; i < kAsioThreadCount; ++i) {
        asioThreads_.emplace_back([this, i]() {
            BOOST_LOG_TRIVIAL(info) << "[AAService] ASIO thread " << i << " started";
            ioService_->run();
            BOOST_LOG_TRIVIAL(info) << "[AAService] ASIO thread " << i << " stopped";
        });
    }

    // Init libusb
    int rc = libusb_init(&usbContext_);
    if (rc != LIBUSB_SUCCESS) {
        BOOST_LOG_TRIVIAL(error) << "[AAService] Failed to init libusb: " << libusb_error_name(rc);
        setState(Disconnected, QString("libusb init failed: %1").arg(libusb_error_name(rc)));
        return;
    }

    // Start libusb event threads
    usbRunning_ = true;
    for (int i = 0; i < kUsbThreadCount; ++i) {
        usbThreads_.emplace_back([this, i]() {
            BOOST_LOG_TRIVIAL(info) << "[AAService] USB thread " << i << " started";
            while (usbRunning_) {
                timeval tv = {1, 0}; // 1 second timeout
                libusb_handle_events_timeout_completed(usbContext_, &tv, nullptr);
            }
            BOOST_LOG_TRIVIAL(info) << "[AAService] USB thread " << i << " stopped";
        });
    }

    // Create USB wrappers and start hub
    usbWrapper_ = std::make_shared<aasdk::usb::USBWrapper>(usbContext_);
    queryFactory_ = std::make_shared<aasdk::usb::AccessoryModeQueryFactory>(*usbWrapper_, *ioService_);
    queryChainFactory_ = std::make_shared<aasdk::usb::AccessoryModeQueryChainFactory>(
        *usbWrapper_, *ioService_, *queryFactory_);

    // SSL wrapper (shared across connections)
    sslWrapper_ = std::make_shared<aasdk::transport::SSLWrapper>();

    // TCP wrapper
    tcpWrapper_ = std::make_shared<aasdk::tcp::TCPWrapper>();

    // Start USB hub detection
    startUSBHub();

    // Start TCP listener for wireless AA
    startTCPListener();

    setState(WaitingForDevice, "Waiting for device (USB or wireless on port 5277)...");
}

void AndroidAutoService::stop()
{
    BOOST_LOG_TRIVIAL(info) << "[AAService] Stopping Android Auto service";

    // Stop entity
    if (entity_) {
        entity_->stop();
        entity_.reset();
    }

    // Cancel USB hub
    if (usbHub_) {
        usbHub_->cancel();
        usbHub_.reset();
    }

    // Cancel TCP acceptor
    if (tcpAcceptor_) {
        boost::system::error_code ec;
        tcpAcceptor_->close(ec);
        tcpAcceptor_.reset();
    }

    // Stop ASIO
    if (ioWork_) {
        ioWork_.reset();
    }
    if (ioService_) {
        ioService_->stop();
    }

    for (auto& t : asioThreads_) {
        if (t.joinable()) t.join();
    }
    asioThreads_.clear();

    // Stop libusb threads
    usbRunning_ = false;
    for (auto& t : usbThreads_) {
        if (t.joinable()) t.join();
    }
    usbThreads_.clear();

    if (usbContext_) {
        libusb_exit(usbContext_);
        usbContext_ = nullptr;
    }

    queryChainFactory_.reset();
    queryFactory_.reset();
    usbWrapper_.reset();
    tcpWrapper_.reset();
    sslWrapper_.reset();
    ioService_.reset();

    setState(Disconnected, "Stopped");
}

void AndroidAutoService::setState(ConnectionState state, const QString& message)
{
    // Thread-safe: may be called from ASIO threads
    QMetaObject::invokeMethod(this, [this, state, message]() {
        if (state_ != state) {
            state_ = state;
            emit connectionStateChanged();
        }
        if (statusMessage_ != message) {
            statusMessage_ = message;
            emit statusMessageChanged();
        }
    }, Qt::QueuedConnection);
}

void AndroidAutoService::startUSBHub()
{
    usbHub_ = std::make_shared<aasdk::usb::USBHub>(*usbWrapper_, *ioService_, *queryChainFactory_);

    auto promise = aasdk::usb::IUSBHub::Promise::defer(*ioService_);
    promise->then(
        [this](aasdk::usb::DeviceHandle handle) {
            BOOST_LOG_TRIVIAL(info) << "[AAService] USB AOAP device connected!";
            onUSBDeviceConnected(std::move(handle));
        },
        [this](const aasdk::error::Error& e) {
            BOOST_LOG_TRIVIAL(error) << "[AAService] USB hub error: " << e.what();
            // Restart hub unless we're shutting down
            if (usbRunning_) {
                BOOST_LOG_TRIVIAL(info) << "[AAService] Restarting USB hub...";
                startUSBHub();
            }
        });

    usbHub_->start(std::move(promise));
}

void AndroidAutoService::onUSBDeviceConnected(aasdk::usb::DeviceHandle handle)
{
    if (entity_) {
        BOOST_LOG_TRIVIAL(warning) << "[AAService] Already have active connection, ignoring USB device";
        startUSBHub(); // Keep watching
        return;
    }

    setState(Connecting, "USB device detected, connecting...");

    try {
        auto aoapDevice = aasdk::usb::AOAPDevice::create(*usbWrapper_, *ioService_, std::move(handle));
        auto transport = std::make_shared<aasdk::transport::USBTransport>(*ioService_, std::move(aoapDevice));
        startEntity(std::move(transport));
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "[AAService] Failed to create USB transport: " << e.what();
        setState(WaitingForDevice, QString("USB error: %1").arg(e.what()));
        startUSBHub();
    }
}

void AndroidAutoService::startTCPListener()
{
    try {
        if (!tcpAcceptor_) {
            auto endpoint = boost::asio::ip::tcp::endpoint(
                boost::asio::ip::tcp::v4(), kTCPListenPort);

            tcpAcceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(*ioService_, endpoint);
            tcpAcceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

            BOOST_LOG_TRIVIAL(info) << "[AAService] TCP listener started on port " << kTCPListenPort;
        }

        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(*ioService_);
        tcpAcceptor_->async_accept(*socket,
            [this, socket](const boost::system::error_code& ec) {
                if (!ec) {
                    onTCPConnection(socket);
                } else if (ec != boost::asio::error::operation_aborted) {
                    BOOST_LOG_TRIVIAL(error) << "[AAService] TCP accept error: " << ec.message();
                }
                // Accept next connection
                if (tcpAcceptor_ && tcpAcceptor_->is_open()) {
                    startTCPListener();
                }
            });
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "[AAService] Failed to start TCP listener: " << e.what();
    }
}

void AndroidAutoService::onTCPConnection(std::shared_ptr<boost::asio::ip::tcp::socket> socket)
{
    auto remoteEndpoint = socket->remote_endpoint();
    BOOST_LOG_TRIVIAL(info) << "[AAService] Wireless AA connection from "
                            << remoteEndpoint.address().to_string()
                            << ":" << remoteEndpoint.port();

    if (entity_) {
        BOOST_LOG_TRIVIAL(warning) << "[AAService] Already have active connection, rejecting TCP";
        socket->close();
        return;
    }

    setState(Connecting, QString("Wireless connection from %1...")
             .arg(QString::fromStdString(remoteEndpoint.address().to_string())));

    try {
        auto tcpEndpoint = std::make_shared<aasdk::tcp::TCPEndpoint>(*tcpWrapper_, std::move(socket));
        auto transport = std::make_shared<aasdk::transport::TCPTransport>(*ioService_, std::move(tcpEndpoint));
        startEntity(std::move(transport));
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "[AAService] Failed to create TCP transport: " << e.what();
        setState(WaitingForDevice, QString("TCP error: %1").arg(e.what()));
    }
}

void AndroidAutoService::startEntity(aasdk::transport::ITransport::Pointer transport)
{
    BOOST_LOG_TRIVIAL(info) << "[AAService] Creating Android Auto entity";

    // Create cryptor
    auto cryptor = std::make_shared<aasdk::messenger::Cryptor>(sslWrapper_);
    cryptor->init();

    // Create messenger
    auto messageInStream = std::make_shared<aasdk::messenger::MessageInStream>(
        *ioService_, transport, cryptor);
    auto messageOutStream = std::make_shared<aasdk::messenger::MessageOutStream>(
        *ioService_, transport, cryptor);
    auto messenger = std::make_shared<aasdk::messenger::Messenger>(
        *ioService_, std::move(messageInStream), std::move(messageOutStream));

    // Create services via factory
    auto serviceList = ServiceFactory::create(*ioService_, messenger, config_);

    // Create entity (it creates its own control channel from its strand)
    entity_ = std::make_shared<AndroidAutoEntity>(
        *ioService_, std::move(cryptor), messenger, std::move(serviceList));

    entity_->start(*this);
}

// IAndroidAutoEntityEventHandler callbacks (called from ASIO thread)

void AndroidAutoService::onConnected()
{
    BOOST_LOG_TRIVIAL(info) << "[AAService] Android Auto connected!";
    setState(Connected, "Android Auto active");
}

void AndroidAutoService::onDisconnected()
{
    BOOST_LOG_TRIVIAL(info) << "[AAService] Android Auto disconnected";

    if (entity_) {
        entity_->stop();
        entity_.reset();
    }

    setState(WaitingForDevice, "Waiting for device (USB or wireless on port 5277)...");

    // Restart USB hub to watch for new connections
    if (usbRunning_ && ioService_) {
        ioService_->post([this]() { startUSBHub(); });
    }
}

void AndroidAutoService::onError(const std::string& message)
{
    BOOST_LOG_TRIVIAL(error) << "[AAService] Error: " << message;
    setState(Disconnected, QString("Error: %1").arg(QString::fromStdString(message)));
}

} // namespace aa
} // namespace oap
