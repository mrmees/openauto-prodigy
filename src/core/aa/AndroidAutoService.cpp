#include "AndroidAutoService.hpp"
#include "ServiceFactory.hpp"

#include <QMetaObject>
#include <boost/log/trivial.hpp>

#include <aasdk/TCP/TCPWrapper.hpp>
#include <aasdk/TCP/TCPEndpoint.hpp>
#include <aasdk/Transport/SSLWrapper.hpp>
#include <aasdk/Transport/TCPTransport.hpp>
#include <aasdk/Messenger/Cryptor.hpp>
#include <aasdk/Messenger/MessageInStream.hpp>
#include <aasdk/Messenger/MessageOutStream.hpp>
#include <aasdk/Messenger/Messenger.hpp>

#ifdef HAS_BLUETOOTH
#include "BluetoothDiscoveryService.hpp"
#endif

namespace oap {
namespace aa {

static constexpr int kAsioThreadCount = 4;

AndroidAutoService::AndroidAutoService(
    std::shared_ptr<oap::Configuration> config, QObject* parent)
    : QObject(parent)
    , config_(std::move(config))
{
    videoDecoder_ = new VideoDecoder(this);
}

AndroidAutoService::~AndroidAutoService()
{
    stop();
}

void AndroidAutoService::start()
{
    BOOST_LOG_TRIVIAL(info) << "[AAService] Starting Android Auto service (wireless mode)";

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

    // SSL wrapper (shared across connections)
    sslWrapper_ = std::make_shared<aasdk::transport::SSLWrapper>();

    // TCP wrapper
    tcpWrapper_ = std::make_shared<aasdk::tcp::TCPWrapper>();

    // Start TCP listener for wireless AA
    startTCPListener();

#ifdef HAS_BLUETOOTH
    // Start Bluetooth discovery service
    if (config_->wirelessEnabled()) {
        btService_ = new BluetoothDiscoveryService(config_, this);
        connect(btService_, &BluetoothDiscoveryService::phoneWillConnect,
                this, [this]() {
                    setState(Connecting, "Phone connecting via WiFi...");
                });
        connect(btService_, &BluetoothDiscoveryService::error,
                this, [this](const QString& msg) {
                    BOOST_LOG_TRIVIAL(error) << "[AAService] BT error: " << msg.toStdString();
                });
        btService_->start();
        BOOST_LOG_TRIVIAL(info) << "[AAService] Bluetooth discovery started";
    }
#endif

    uint16_t port = config_->tcpPort();
    setState(WaitingForDevice,
             QString("Waiting for wireless connection on port %1...").arg(port));
}

void AndroidAutoService::stop()
{
    BOOST_LOG_TRIVIAL(info) << "[AAService] Stopping Android Auto service";

    // Stop entity
    if (entity_) {
        entity_->stop();
        entity_.reset();
    }

#ifdef HAS_BLUETOOTH
    if (btService_) {
        btService_->stop();
        delete btService_;
        btService_ = nullptr;
    }
#endif

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

void AndroidAutoService::startTCPListener()
{
    uint16_t port = config_->tcpPort();
    try {
        if (!tcpAcceptor_) {
            auto endpoint = boost::asio::ip::tcp::endpoint(
                boost::asio::ip::tcp::v4(), port);

            tcpAcceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(*ioService_, endpoint);
            tcpAcceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

            BOOST_LOG_TRIVIAL(info) << "[AAService] TCP listener started on port " << port;
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
        BOOST_LOG_TRIVIAL(warning) << "[AAService] Already have active connection — tearing down old session for reconnect";
        entity_->stop();
        entity_.reset();
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
    auto serviceList = ServiceFactory::create(*ioService_, messenger, config_, videoDecoder_);

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

    uint16_t port = config_->tcpPort();
    setState(WaitingForDevice,
             QString("Waiting for wireless connection on port %1...").arg(port));
}

void AndroidAutoService::onError(const std::string& message)
{
    BOOST_LOG_TRIVIAL(error) << "[AAService] Error: " << message;
    setState(Disconnected, QString("Error: %1").arg(QString::fromStdString(message)));
}

} // namespace aa
} // namespace oap
